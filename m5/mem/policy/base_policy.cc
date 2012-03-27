/*
 * miss_bandwidth_policy.cc
 *
 *  Created on: Oct 28, 2009
 *      Author: jahre
 */

#include "base_policy.hh"
#include "base/intmath.hh"

#include <cmath>

using namespace std;

BasePolicy::BasePolicy(string _name,
					   InterferenceManager* _intManager,
					   Tick _period,
					   int _cpuCount,
					   PerformanceEstimationMethod _perfEstMethod,
					   bool _persistentAllocations,
					   int _iterationLatency,
					   Metric* _performanceMetric,
					   bool _enforcePolicy,
					   ThrottleControl* _sharedCacheThrottle,
					   std::vector<ThrottleControl* > _privateCacheThrottles)
: SimObject(_name){

	intManager = _intManager;
	perfEstMethod = _perfEstMethod;
	performanceMetric = _performanceMetric;

//	dumpInitalized = false;
//	dumpSearchSpaceAt = 0; // set this to zero to turn off

	usePersistentAllocations = _persistentAllocations;
	iterationLatency = _iterationLatency;

	privateCacheThrottles = _privateCacheThrottles;
	sharedCacheThrottle = _sharedCacheThrottle;

	intManager->registerMissBandwidthPolicy(this);

	period = _period;
	if(_enforcePolicy){
		policyEvent = new MissBandwidthPolicyEvent(this, period);
		policyEvent->schedule(period);
		traceEvent = NULL;
	}
	else{
		traceEvent = new MissBandwidthTraceEvent(this, period);
		traceEvent->schedule(period);
		policyEvent = NULL;
	}

	measurementTrace = RequestTrace(_name, "MeasurementTrace", 1);
	predictionTrace = RequestTrace(_name, "PredictionTrace", 1);
	aloneIPCTrace = RequestTrace(_name, "AloneIPCTrace", 1);
	numMSHRsTrace = RequestTrace(_name, "NumMSHRsTrace", 1);
	searchTrace = RequestTrace(_name, "SearchTrace", 1);

	cpuCount = _cpuCount;
	caches.resize(cpuCount, NULL);

	cummulativeMemoryRequests.resize(_cpuCount, 0);
	cummulativeCommittedInsts.resize(_cpuCount, 0);

	aloneIPCEstimates.resize(_cpuCount, 0.0);
	aloneCycles.resize(_cpuCount, 0.0);
	avgLatencyAloneIPCModel.resize(_cpuCount, 0.0);

	currentMeasurements = NULL;

	mostRecentMWSEstimate.resize(cpuCount, vector<double>());
	mostRecentMLPEstimate.resize(cpuCount, vector<double>());

	comInstModelTraceCummulativeInst.resize(cpuCount, 0);

	bestRequestProjection.resize(cpuCount, 0.0);
	measurementsValid = false;

	requestAccumulator.resize(cpuCount, 0.0);
	requestSqAccumulator.resize(cpuCount, 0.0);
	avgReqsPerSample.resize(cpuCount, 0.0);
	reqsPerSampleStdDev.resize(cpuCount, 0.0);

	computedOverlap.resize(cpuCount, 0.0);

	initProjectionTrace(_cpuCount);
	initAloneIPCTrace(_cpuCount, _enforcePolicy);
	initNumMSHRsTrace(_cpuCount);
	initComInstModelTrace(_cpuCount);

	enableOccupancyTrace = false;
}

BasePolicy::~BasePolicy(){
	if(policyEvent != NULL){
		assert(!policyEvent->scheduled());
		delete policyEvent;
	}
}

void
BasePolicy::regStats(){
	using namespace Stats;

	aloneEstimationFailed
		.init(cpuCount)
		.name(name() + ".alone_estimation_failed")
		.desc("the number of times the alone cycles estimation failed")
		.flags(total);

	requestAbsError
		.init(cpuCount)
		.name(name() + ".req_est_abs_error")
		.desc("Absolute request estimation errors (in requests)")
		.flags(total);

	sharedLatencyAbsError
		.init(cpuCount)
		.name(name() + ".shared_latency_est_abs_error")
		.desc("Absolute shared latency errors (in cc)")
		.flags(total);

	requestRelError
		.init(cpuCount)
		.name(name() + ".req_est_rel_error")
		.desc("Relative request estimation errors (in %)")
		.flags(total);

	sharedLatencyRelError
		.init(cpuCount)
		.name(name() + ".shared_latency_est_rel_error")
		.desc("Relative shared latency errors (in %)")
		.flags(total);

}

void
BasePolicy::registerCache(BaseCache* _cache, int _cpuID, int _maxMSHRs){

	assert(caches[_cpuID] == NULL);
	caches[_cpuID] = _cache;
	maxMSHRs = _maxMSHRs;

	if(enableOccupancyTrace){
		_cache->enableOccupancyList();
	}

	//if(!searchTrace.isInitialized()) initSearchTrace(cpuCount, searchAlgorithm);
}

void
BasePolicy::registerBus(Bus *_bus){
	buses.push_back(_bus);
}

void
BasePolicy::registerSharedCache(BaseCache* _cache){
	sharedCaches.push_back(_cache);
}

void
BasePolicy::handlePolicyEvent(){
	PerformanceMeasurement curMeasurement = intManager->buildInterferenceMeasurement(period);
	runPolicy(curMeasurement);
}

void
BasePolicy::handleTraceEvent(){
	PerformanceMeasurement curMeasurement = intManager->buildInterferenceMeasurement(period);
	vector<double> measuredIPC(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++){
		measuredIPC[i] = (double) curMeasurement.committedInstructions[i] / (double) period;
	}
	traceAloneIPC(curMeasurement.requestsInSample,
			      measuredIPC,
			      curMeasurement.committedInstructions,
			      curMeasurement.cpuStallCycles,
			      curMeasurement.sharedLatencies,
			      curMeasurement.avgMissesWhileStalled);
}

void
BasePolicy::initAloneIPCTrace(int cpuCount, bool policyEnforced){
	vector<string> headers;

	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Requests", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Cummulative Requests", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Committed Insts", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Cummulative Insts", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Stall cycles", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Run cycles", i));

	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Max MLP MWS", i));
	if(cpuCount > 1){
		if(policyEnforced){
			for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Alone IPC Estimate", i));
		}
		else{
			for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Shared IPC Measurement", i));
		}
		for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Shared Avg Latency Measurement", i));
		for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Avg Latency Based Alone IPC Estimate", i));
	}
	else{
		for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Alone IPC Measurement", i));
		for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Alone Avg Latency Measurement", i));
	}

	aloneIPCTrace.initalizeTrace(headers);
}

void
BasePolicy::traceAloneIPC(std::vector<int> memoryRequests,
		                           std::vector<double> ipcs,
		                           std::vector<int> committedInstructions,
		                           std::vector<int> stallCycles,
		                           std::vector<double> avgLatencies,
		                           std::vector<std::vector<double> > missesWhileStalled){
	vector<RequestTraceEntry> data;

	assert(memoryRequests.size() == cpuCount);
	assert(ipcs.size() == cpuCount);

	for(int i=0;i<cpuCount;i++){
		cummulativeMemoryRequests[i] += memoryRequests[i];
		cummulativeCommittedInsts[i] += committedInstructions[i];
	}

	for(int i=0;i<cpuCount;i++) data.push_back(memoryRequests[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(cummulativeMemoryRequests[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(committedInstructions[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(cummulativeCommittedInsts[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(stallCycles[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(period - stallCycles[i]);

	for(int i=0;i<cpuCount;i++) data.push_back(missesWhileStalled[i][maxMSHRs]);
	for(int i=0;i<cpuCount;i++) data.push_back(ipcs[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(avgLatencies[i]);

	if(cpuCount > 1){
		for(int i=0;i<cpuCount;i++) data.push_back(avgLatencyAloneIPCModel[i]);
	}

	aloneIPCTrace.addTrace(data);
}

void
BasePolicy::addTraceEntry(PerformanceMeasurement* measurement){

	if(!measurementTrace.isInitialized()){
		vector<string> header = measurement->getTraceHeader();
		measurementTrace.initalizeTrace(header);
	}
	vector<RequestTraceEntry> line = measurement->createTraceLine();
	measurementTrace.addTrace(line);

}

void
BasePolicy::traceVerboseVector(const char* message, std::vector<int>& data){
	DPRINTFR(MissBWPolicyExtra, message);
	for(int i=0;i<data.size();i++) DPRINTFR(MissBWPolicyExtra, "%d:%d ", i, data[i]);
	DPRINTFR(MissBWPolicyExtra, "\n");
}

void
BasePolicy::traceVerboseVector(const char* message, std::vector<double>& data){
	DPRINTFR(MissBWPolicyExtra, message);
	for(int i=0;i<data.size();i++) DPRINTFR(MissBWPolicyExtra, "%d:%f ", i, data[i]);
	DPRINTFR(MissBWPolicyExtra, "\n");
}

void
BasePolicy::traceVector(const char* message, std::vector<int>& data){
	DPRINTF(MissBWPolicy, message);
	for(int i=0;i<data.size();i++) DPRINTFR(MissBWPolicy, "%d:%d ", i, data[i]);
	DPRINTFR(MissBWPolicy, "\n");
}

void
BasePolicy::traceVector(const char* message, std::vector<double>& data){
	DPRINTF(MissBWPolicy, message);
	for(int i=0;i<data.size();i++) DPRINTFR(MissBWPolicy, "%d:%f ", i, data[i]);
	DPRINTFR(MissBWPolicy, "\n");
}

void
BasePolicy::tracePerformance(std::vector<double>& sharedIPCEstimate,
		                              std::vector<double>& speedups){
	traceVerboseVector("Shared IPC Estimate: ", sharedIPCEstimate);
	traceVerboseVector("Alone IPC Estimate: ", aloneIPCEstimates);
	traceVerboseVector("Estimated Speedup: ", speedups);
}

double
BasePolicy::computeError(double estimate, double actual){
	if(actual == 0) return 0;
	double relErr = (estimate - actual) / actual;
	return relErr*100;
}

double
BasePolicy::computeSpeedup(double sharedIPCEstimate, int cpuID){
	if(aloneIPCEstimates[cpuID] == 0) return 1.0;
	return sharedIPCEstimate / aloneIPCEstimates[cpuID];
}

double
BasePolicy::computeCurrentMetricValue(){

	vector<double> speedups(cpuCount, 0.0);
	vector<double> sharedIPCs(cpuCount, 0.0);

	for(int i=0;i<cpuCount;i++){
		sharedIPCs[i] = (double) currentMeasurements->committedInstructions[i] / (double) period;
		speedups[i] = computeSpeedup(sharedIPCs[i], i);
	}
	traceVector("Estimated Current Speedups: ", speedups);
	traceVector("Estimated Current Shared IPCs: ", sharedIPCs);

	double metricValue = performanceMetric->computeMetric(&speedups, &sharedIPCs);
	DPRINTF(MissBWPolicy, "Estimated current metric value to be %f\n", metricValue);
	return metricValue;
}

double
BasePolicy::estimateStallCycles(double currentStallTime,
								double privateStallTime,
							    double currentAvgSharedLat,
								double newAvgSharedLat,
								double sharedRequests,
								int cpuID,
								int cpl,
								double privateMissRate){

	DPRINTF(MissBWPolicyExtra, "Estimating private stall cycles for CPU %d\n", cpuID);

	if(perfEstMethod == RATIO_MWS || perfEstMethod == LATENCY_MLP || perfEstMethod == LATENCY_MLP_SREQ || perfEstMethod == NO_MLP_CACHE){
		fatal("deprecated performance estimation mode");
	}
	else if(perfEstMethod == NO_MLP ){

		DPRINTF(MissBWPolicyExtra, "Running no-MLP method with shared lat %f, requests %f, private stall cycles %f, %f stall cycles\n",
						currentAvgSharedLat,
						sharedRequests,
						privateStallTime,
						currentStallTime);

		if(sharedRequests == 0){
			// hidden loads might cause the assertion to fail
			// assert((int) currentStallTime == 0);
			computedOverlap[cpuID] = 0;
			DPRINTF(MissBWPolicyExtra, "No latency and no stall cycles, returning private stall cycles %d\n", privateStallTime);
			return privateStallTime;
		}

		computedOverlap[cpuID] = currentStallTime / (currentAvgSharedLat * sharedRequests);

		DPRINTF(MissBWPolicyExtra, "Computed overlap %d\n", computedOverlap[cpuID]);

//		assert(computedOverlap[cpuID] <= 1.0);
//		assert(computedOverlap[cpuID] >= 0.0);

		double newStallTime = privateStallTime +  computedOverlap[cpuID] * newAvgSharedLat * sharedRequests;

		DPRINTF(MissBWPolicyExtra, "Estimated new stall time %f with new shared lat %f\n",
							        newStallTime,
							        newAvgSharedLat);

		assert(newStallTime >= 0);

		return newStallTime;
	}
	else if(perfEstMethod == CPL){
		computedOverlap[cpuID] = 0.0;
		if(sharedRequests == 0 || cpl == 0){
			DPRINTF(MissBWPolicyExtra, "No shared requests or clp=0, returning private stall time %d (reqs=%d, cpl=%d)\n", privateStallTime, sharedRequests, cpl);
			return privateStallTime;
		}

		double avgFanOut = (double) sharedRequests / (double) cpl;
		DPRINTF(MissBWPolicyExtra, "Computed average fan out %f\n", avgFanOut);

		double avgBusServiceLatency = 120.0; //FIXME: implement different policies
		//double busSerializationCorrection = (avgFanOut-1.0) * privateMissRate * avgBusServiceLatency; //FIXME: add bus serialization idea as separate policy
		double busSerializationCorrection = 0.0;
		if(busSerializationCorrection < 0) busSerializationCorrection = 0.0;

		DPRINTF(MissBWPolicyExtra, "Computed bus serialization correction %f with bus latency %f and private miss rate %d\n",
				busSerializationCorrection,
				avgBusServiceLatency,
				privateMissRate);

		double newStallTime = cpl * (newAvgSharedLat + busSerializationCorrection);

		DPRINTF(MissBWPolicyExtra, "Computed new stall time %f with new shared latency %f, private stall time is %f, current stall %f, current latency %f\n",
				newStallTime,
				newAvgSharedLat,
				privateStallTime,
				currentStallTime,
				currentAvgSharedLat);

		return privateStallTime+newStallTime;
	}

	fatal("Unknown performance estimation method");
	return 0.0;
}

void
BasePolicy::updateAloneIPCEstimate(){
	fatal("call to estimateStallCycles must be reimplemented");
//	for(int i=0;i<cpuCount;i++){
//		if(caches[i]->getCurrentMSHRCount(true) == maxMSHRs){
//
//			double stallParallelism = currentMeasurements->avgMissesWhileStalled[i][maxMSHRs];
//			double curMLP = currentMeasurements->mlpEstimate[i][maxMSHRs];
//			double curReqs = currentMeasurements->requestsInSample[i];
//			double privateMisses = currentMeasurements->perCoreCacheMeasurements[i].readMisses - currentMeasurements->perCoreCacheMeasurements[i].interferenceMisses;
//			double newStallEstimate =
//			estimateStallCycles(currentMeasurements->cpuStallCycles[i],
//														  stallParallelism,
//														  curMLP,
//														  currentMeasurements->sharedLatencies[i],
//														  curReqs,
//														  stallParallelism,
//														  curMLP,
//														  currentMeasurements->estimatedPrivateLatencies[i],
//														  curReqs,
//														  currentMeasurements->responsesWhileStalled[i],
//														  i,
//														  currentMeasurements->perCoreCacheMeasurements[i].readMisses,
//														  privateMisses);
//
//			aloneIPCEstimates[i] = currentMeasurements->committedInstructions[i] / (currentMeasurements->getNonStallCycles(i, period) + newStallEstimate);
//			DPRINTF(MissBWPolicy, "Updating alone IPC estimate for cpu %i to %f, %d committed insts, %d non-stall cycles, %f new stall cycle estimate\n",
//					              i,
//					              aloneIPCEstimates[i],
//					              currentMeasurements->committedInstructions[i],
//					              currentMeasurements->getNonStallCycles(i, period),
//								  newStallEstimate);
//
//			if(currentMeasurements->estimatedPrivateLatencies[i] < currentMeasurements->sharedLatencies[i]){
//				double avgInterference = currentMeasurements->sharedLatencies[i] - currentMeasurements->estimatedPrivateLatencies[i];
//				double totalInterferenceCycles = avgInterference * currentMeasurements->requestsInSample[i];
//				double visibleIntCycles = computedOverlap[i] * totalInterferenceCycles;
//				avgLatencyAloneIPCModel[i]= (double) currentMeasurements->committedInstructions[i] / ((double) period - visibleIntCycles);
//
//				assert(period > visibleIntCycles);
//				assert(visibleIntCycles >= 0);
//				aloneCycles[i] = ((double) period - visibleIntCycles);
//
//				DPRINTF(MissBWPolicy, "Estimating alone IPC to %f and alone cycles to %f\n",
//						avgLatencyAloneIPCModel[i],
//						aloneCycles[i]);
//
//			}
//			else{
//				avgLatencyAloneIPCModel[i]= (double) currentMeasurements->committedInstructions[i] / ((double) period);
//				aloneCycles[i] = (double) period;
//
//				DPRINTF(MissBWPolicy, "The estimated private latency %d is greater than the measured shared latency %d, concluding no interference with alone IPC %f and alone cycles %f\n",
//						currentMeasurements->estimatedPrivateLatencies[i],
//						currentMeasurements->sharedLatencies[i],
//						avgLatencyAloneIPCModel[i],
//						aloneCycles[i]);
//
//			}
//
//		}
//	}
}

void
BasePolicy::updateMWSEstimates(){

	for(int i=0;i<cpuCount;i++){
		if(caches[i]->getCurrentMSHRCount(true) == maxMSHRs){
			DPRINTF(MissBWPolicy, "Updating local MLP estimate for CPU %i\n", i);
			mostRecentMWSEstimate[i] = currentMeasurements->avgMissesWhileStalled[i];
			mostRecentMLPEstimate[i] = currentMeasurements->mlpEstimate[i];
		}
	}

}

void
BasePolicy::implementMHA(std::vector<int> bestMHA){
	DPRINTF(MissBWPolicy, "-- Implementing new MHA\n");
	for(int i=0;i<caches.size();i++){
		DPRINTF(MissBWPolicy, "Setting CPU%d MSHRs to %d\n", i , bestMHA[i]);
		caches[i]->setNumMSHRs(bestMHA[i]);
	}
}

double
BasePolicy::squareRoot(double num){

	assert(num >= 0);

	int iterations = 10;
	int digits = 0;
	Tick tempnum = (Tick) num;
	while(tempnum != 0){
		tempnum = tempnum >> 1;
		digits++;
	}

	double sqroot = 1 << (digits/2);

	for(int i=0;i<iterations;i++){
		sqroot = 0.5 * (sqroot + (num / sqroot));
	}

	return sqroot;
}

void
BasePolicy::initProjectionTrace(int cpuCount){
	vector<string> headers;

	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Estimated Num Requests", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Measured Num Requests", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Estimated Avg Shared Latency", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Measured Avg Shared Latency", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Average Requests per Sample", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Stdev Requests per Sample", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Estimated IPC", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Measured IPC", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Estimated Interference Misses", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Actual Interference Misses", i));

	predictionTrace.initalizeTrace(headers);
}

void
BasePolicy::initNumMSHRsTrace(int cpuCount){
	vector<string> headers;
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Num MSHRs", i));
	numMSHRsTrace.initalizeTrace(headers);
}

void
BasePolicy::traceNumMSHRs(){
	vector<RequestTraceEntry> data;
	for(int i=0;i<cpuCount;i++) data.push_back(caches[i]->getCurrentMSHRCount(true));
	numMSHRsTrace.addTrace(data);
}

void
BasePolicy::traceBestProjection(){

	vector<RequestTraceEntry> data;

	for(int i=0;i<cpuCount;i++){
		if(doEvaluation(i)) data.push_back(bestRequestProjection[i]);
		else data.push_back(INT_MAX);
	}
	for(int i=0;i<cpuCount;i++){
		if(doEvaluation(i)) data.push_back(currentMeasurements->requestsInSample[i]);
		else data.push_back(INT_MAX);
	}
	for(int i=0;i<cpuCount;i++){
		if(doEvaluation(i)) data.push_back(bestLatencyProjection[i]);
		else data.push_back(INT_MAX);
	}
	for(int i=0;i<cpuCount;i++){
		if(doEvaluation(i)) data.push_back(currentMeasurements->sharedLatencies[i]);
		else data.push_back(INT_MAX);
	}

	for(int i=0;i<cpuCount;i++) data.push_back(avgReqsPerSample[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(reqsPerSampleStdDev[i]);

	for(int i=0;i<cpuCount;i++){
		if(doEvaluation(i)) data.push_back(bestIPCProjection[i]);
		else data.push_back(INT_MAX);
	}

	for(int i=0;i<cpuCount;i++){
		if(doEvaluation(i)){
			double ipc = (double) currentMeasurements->committedInstructions[i] / (double) period;
			data.push_back(ipc);
		}
		else data.push_back(INT_MAX);
	}

    for(int i=0;i<cpuCount;i++){
      if(doEvaluation(i)) data.push_back(bestInterferenceMissProjection[i]);
      else data.push_back(INT_MAX);
    }
    for(int i=0;i<cpuCount;i++){
      data.push_back(currentMeasurements->perCoreCacheMeasurements[i].interferenceMisses);
    }

	predictionTrace.addTrace(data);
}

void
BasePolicy::updateBestProjections(){
	bestRequestProjection = currentRequestProjection;
	bestLatencyProjection = currentLatencyProjection;
	bestIPCProjection = currentIPCProjection;
    bestInterferenceMissProjection = currentInterferenceMissProjection;
}

void
BasePolicy::initComInstModelTrace(int cpuCount){
	vector<string> headers;

	headers.push_back("Cummulative Committed Instructions");
	headers.push_back("Total Cycles");
	headers.push_back("Stall Cycles");
	headers.push_back("Private Stall Cycles");
	headers.push_back("Write Stall Cycles");
	headers.push_back("Compute Cycles");
	headers.push_back("Memory Independent Stalls");
	headers.push_back("Total Requests");
	headers.push_back("Total Latency");
	headers.push_back("Hidden Loads");
	headers.push_back("CPL");

	if(cpuCount > 1){
		headers.push_back("Average Shared Latency");
		headers.push_back("Average Shared Private Memsys Latency");
		headers.push_back("Estimated Private Latency");
		headers.push_back("Shared IPC");
		headers.push_back("Estimated Alone IPC");
		headers.push_back("Measured Shared Overlap");
		headers.push_back("Estimated Alone Overlap");
		headers.push_back("Private Mode Miss Rate Estimate");
		headers.push_back("Stall Estimate");
	}
	else{
		headers.push_back("Alone Memory Latency");
		headers.push_back("Average Alone Private Memsys Latency");
		headers.push_back("Measured Alone IPC");
		headers.push_back("Measured Alone Overlap");
		headers.push_back("Measured Private Mode Miss Rate");
		headers.push_back("Actual Stall");
	}

	comInstModelTraces.resize(cpuCount, RequestTrace());
	for(int i=0;i<cpuCount;i++){
		comInstModelTraces[i] = RequestTrace(name(), RequestTrace::buildFilename("CommittedInsts", i).c_str(), 1);
		comInstModelTraces[i].initalizeTrace(headers);
	}
}

void
BasePolicy::doCommittedInstructionTrace(int cpuID,
		                                double avgSharedLat,
		                                double avgPrivateLatEstimate,
		                                int reqs,
		                                int stallCycles,
		                                int cyclesInSample,
		                                int committedInsts,
					                    int commitCycles,
					                    Tick privateStallCycles,
					                    double avgPrivateMemsysLat,
					                    Tick writeStall,
					                    int hiddenLoads,
					                    Tick memoryIndependentStallCycles,
					                    int cpl,
					                    double privateMissRate){

	vector<RequestTraceEntry> data;

	DPRINTF(MissBWPolicy, "-- Running alone IPC estimation trace for CPU %d, %d cycles in sample, %d commit cycles, %d committed insts, private stall %d, shared stall %d, cpl %d\n",
			              cpuID,
			              cyclesInSample,
			              commitCycles,
			              committedInsts,
			              privateStallCycles,
			              stallCycles,
			              cpl);

	comInstModelTraceCummulativeInst[cpuID] += committedInsts;

	data.push_back(comInstModelTraceCummulativeInst[cpuID]);
	data.push_back(cyclesInSample);
	data.push_back(stallCycles);
	data.push_back(privateStallCycles);
	data.push_back(writeStall);
	data.push_back(commitCycles);
	data.push_back(memoryIndependentStallCycles);
	data.push_back(reqs);
	data.push_back(reqs*(avgSharedLat+avgPrivateMemsysLat));
	data.push_back(hiddenLoads);
	data.push_back(cpl);

	if(cpuCount > 1){
		double newStallEstimate = estimateStallCycles(stallCycles,
													  privateStallCycles,
				                                      avgSharedLat + avgPrivateMemsysLat,
				                                      avgPrivateLatEstimate + avgPrivateMemsysLat,
				                                      reqs,
				                                      cpuID,
				                                      cpl,
				                                      privateMissRate);

		double sharedIPC = (double) committedInsts / (double) cyclesInSample;
		double aloneIPCEstimate = (double) committedInsts / (commitCycles + memoryIndependentStallCycles + newStallEstimate);


		data.push_back(avgSharedLat);
		data.push_back(avgPrivateMemsysLat);
		data.push_back(avgPrivateLatEstimate);
		data.push_back(sharedIPC);
		data.push_back(aloneIPCEstimate);

		double sharedOverlap = 0;
		if(reqs > 0){
			sharedOverlap = (double) stallCycles / (double) ((avgSharedLat+avgPrivateMemsysLat)*reqs);
		}

		data.push_back(sharedOverlap);
		data.push_back(computedOverlap[cpuID]);
		data.push_back(privateMissRate);
		data.push_back(newStallEstimate);
	}
	else{
		double aloneIPC = (double) committedInsts / (double) cyclesInSample;
		double aloneOverlap = 0;
		if(reqs > 0){
			aloneOverlap = (double) stallCycles / (double) ((avgSharedLat+avgPrivateMemsysLat)*reqs);
		}

		DPRINTF(MissBWPolicy, "Computed alone IPC %d and alone overlap %d from latency %d and requests %d\n",
				aloneIPC,
				aloneOverlap,
				avgSharedLat + avgPrivateMemsysLat,
				reqs);

		data.push_back(avgSharedLat);
		data.push_back(avgPrivateMemsysLat);
		data.push_back(aloneIPC);
		data.push_back(aloneOverlap);
		data.push_back(privateMissRate);
		data.push_back(stallCycles);
	}

	comInstModelTraces[cpuID].addTrace(data);
}

void
BasePolicy::initSearchTrace(int cpuCount, SearchAlgorithm searchAlg){
	if(searchAlg != EXHAUSTIVE_SEARCH){
		vector<string> headers;

		for(int i=0;i<cpuCount;i++){
			headers.push_back(RequestTrace::buildTraceName("Bus Order CPUID", i));
		}

		int numMHAs = maxMSHRs;
		if(searchAlg == BUS_SORTED_LOG) numMHAs = FloorLog2(maxMSHRs)+1;

		for(int i=0;i<cpuCount;i++){
			for(int j=0;j<numMHAs;j++){
				stringstream strstream;
				strstream << "Order" << i << " MHA" << j;
				headers.push_back(strstream.str());
			}
		}

		searchTrace.initalizeTrace(headers);
	}
}


BasePolicy::RequestEstimationMethod
BasePolicy::parseRequestMethod(std::string methodName){

	if(methodName == "MWS") return MWS;
	if(methodName == "MLP") return MLP;

	fatal("unknown request estimation method");
	return MWS;
}

BasePolicy::PerformanceEstimationMethod
BasePolicy::parsePerformanceMethod(std::string methodName){

	if(methodName == "latency-mlp") return LATENCY_MLP;
	if(methodName == "ratio-mws") return RATIO_MWS;
	if(methodName == "latency-mlp-sreq") return LATENCY_MLP_SREQ;
	if(methodName == "no-mlp") return NO_MLP;
	if(methodName == "no-mlp-cache") return NO_MLP_CACHE;
	if(methodName == "cpl") return CPL;

	fatal("unknown performance estimation method");
	return LATENCY_MLP;
}



BasePolicy::SearchAlgorithm
BasePolicy::parseSearchAlgorithm(std::string methodName){
	if(methodName == "exhaustive") return EXHAUSTIVE_SEARCH;
	if(methodName == "bus-sorted") return BUS_SORTED;
	if(methodName == "bus-sorted-log") return BUS_SORTED_LOG;

	fatal("unknown search algorithm");
	return EXHAUSTIVE_SEARCH;
}

Metric*
BasePolicy::parseOptimizationMetric(std::string metricName){
	if(metricName == "hmos") return new HmosPolicy();
	if(metricName == "stp") return new STPPolicy();
	if(metricName == "fairness") return new FairnessPolicy();
	if(metricName == "aggregateIPC") return new AggregateIPCPolicy();

	fatal("unknown optimization metric");
	return NULL;
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("BasePolicy", BasePolicy);

#endif

