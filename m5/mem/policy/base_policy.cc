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
					   WriteStallTechnique _wst,
					   PrivBlockedStallTechnique _pbst,
					   EmptyROBStallTechnique _rst,
					   double _maximumDamping,
					   double _hybridDecisionError,
					   int _hybridBufferSize)
: SimObject(_name){

	intManager = _intManager;
	perfEstMethod = _perfEstMethod;
	performanceMetric = _performanceMetric;
	writeStallTech = _wst;
	privBlockedStallTech = _pbst;
	emptyROBStallTech = _rst;

	maximumDamping = _maximumDamping;

//	dumpInitalized = false;
//	dumpSearchSpaceAt = 0; // set this to zero to turn off

	usePersistentAllocations = _persistentAllocations;
	iterationLatency = _iterationLatency;

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

	measurementTrace = RequestTrace(_name, "MeasurementTrace", true);
	predictionTrace = RequestTrace(_name, "PredictionTrace", true);
	aloneIPCTrace = RequestTrace(_name, "AloneIPCTrace", true);
	numMSHRsTrace = RequestTrace(_name, "NumMSHRsTrace", true);
	searchTrace = RequestTrace(_name, "SearchTrace", true);

	cpuCount = _cpuCount;
	caches.resize(cpuCount, NULL);
	maxMSHRs = 0;
	cpus.resize(cpuCount, NULL);

	cummulativeMemoryRequests.resize(_cpuCount, 0);
	cummulativeCommittedInsts.resize(_cpuCount, 0);

	aloneIPCEstimates.resize(_cpuCount, 0.0);
	aloneCycles.resize(_cpuCount, 0.0);
	avgLatencyAloneIPCModel.resize(_cpuCount, 0.0);

	sharedCPLMeasurements.resize(_cpuCount, 0.0);
	sharedNonSharedLoadCycles.resize(_cpuCount, 0.0);
	sharedLoadStallCycles.resize(_cpuCount, 0.0);
	privateMemsysAvgLatency.resize(_cpuCount, 0.0);
	sharedMemsysAvgLatency.resize(_cpuCount, 0.0);

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
	lastModelError.resize(cpuCount, 0.0);
	lastModelErrorWithCutoff.resize(cpuCount, 0.0);
	lastCPLPolicyDesicion.resize(cpuCount, 0.0);

	hybridErrorBuffer.resize(cpuCount, vector<double>());
	hybridErrorBufferSize = _hybridBufferSize;
	hybridDesicionError = _hybridDecisionError;

	initProjectionTrace(_cpuCount);
	initAloneIPCTrace(_cpuCount, _enforcePolicy);
	initNumMSHRsTrace(_cpuCount);
	initComInstModelTrace(_cpuCount);
	initPerfModelTrace(_cpuCount);

	enableOccupancyTrace = false;

	asmPrivateModeSlowdownEsts.resize(cpuCount, 0.0);

	BasePolicyInitEvent* init = new BasePolicyInitEvent(this);
	init->schedule(curTick);
}

BasePolicy::~BasePolicy(){
	if(policyEvent != NULL){
		assert(!policyEvent->scheduled());
		delete policyEvent;
	}
}

void
BasePolicy::initPolicy(){
	// Empty base class init method
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
BasePolicy::registerFullCPU(FullCPU* _cpu, int _cpuID){
	assert(cpus[_cpuID] == NULL);
	cpus[_cpuID] = _cpu;
}

void
BasePolicy::disableCommitSampling(){
	for(int i=0;i<cpus.size();i++){
		if(cpus[i] == NULL) fatal("CPU %d has not registered", i);
		(cpus[i])->disableCommitTrace();
	}
}

void
BasePolicy::handlePolicyEvent(){
	DPRINTF(MissBWPolicy, "Handling policy event\n");
	if(cpuCount > 1){
		prepareEstimates();
		PerformanceMeasurement curMeasurement = intManager->buildInterferenceMeasurement(period);
		runPolicy(curMeasurement);
	}
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
			      curMeasurement.cpuSharedStallCycles,
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
BasePolicy::computeRawError(double estimate, double actual){
	if(actual == 0.0){
		return 0;
	}
	double error = (estimate - actual) / actual;
	return error;
}

double
BasePolicy::computeDampedEstimate(double privateModelEstimate, double sharedModelEstimate, double curStallTime, int cpuID){

	double error = computeRawError(sharedModelEstimate, curStallTime);
	DPRINTF(MissBWPolicyExtra, "Computed error %d from estimate %d, actual %d, error limit +/- %d\n",
			error,
			sharedModelEstimate,
			curStallTime,
			maximumDamping);

	if(error > maximumDamping){
		DPRINTF(MissBWPolicyExtra, "Enforcing positive limit\n");
		error = maximumDamping;
	}

	if(error < -maximumDamping){
		DPRINTF(MissBWPolicyExtra, "Enforcing negative limit\n");
		error = -maximumDamping;
	}

	lastModelErrorWithCutoff[cpuID] = error;

	double newStallTime = privateModelEstimate * (1 - error);

	DPRINTF(MissBWPolicyExtra, "Correcting error of %d in estimate %d, returning stall estimate is %d\n",
			error,
			privateModelEstimate,
			newStallTime);

	return newStallTime;
}

void
BasePolicy::addHybridError(int cpuID, double error){
    hybridErrorBuffer[cpuID].push_back(error);
    if(hybridErrorBuffer[cpuID].size() > hybridErrorBufferSize){
        hybridErrorBuffer[cpuID].erase(hybridErrorBuffer[cpuID].begin());
    }
}

double
BasePolicy::getHybridAverageError(int cpuID){
    double sum = 0;
    for(int i=0;i<hybridErrorBuffer[cpuID].size();i++){
        sum += hybridErrorBuffer[cpuID][i];
    }
    return sum / (double) hybridErrorBuffer[cpuID].size();
}

double
BasePolicy::estimateStallCycles(double currentStallTime,
								double privateStallTime,
							    double currentAvgSharedLat,
								double newAvgSharedLat,
								double sharedRequests,
								int cpuID,
								int cpl,
								double privateMissRate,
								double cwp,
								Tick boisAloneStallEst,
								CriticalPathTableMeasurements cptMeasurements){

	DPRINTF(MissBWPolicyExtra, "Estimating private stall cycles for CPU %d\n", cpuID);

	if(perfEstMethod == RATIO_MWS || perfEstMethod == LATENCY_MLP || perfEstMethod == LATENCY_MLP_SREQ || perfEstMethod == NO_MLP_CACHE || perfEstMethod == CPL_CWP_SER){
		fatal("deprecated performance estimation mode");
	}
	else if(perfEstMethod == CONST_1){
		DPRINTF(MissBWPolicyExtra, "Returning 0 for const-1 policy on CPU %d\n", cpuID);
		return 0;
	}
	else if (perfEstMethod == LIN_TREE_10){
		DPRINTF(MissBWPolicyExtra, "Returning 0 for lin-tree-10 policy on CPU %d\n", cpuID);
		return 0;
	}
	else if (perfEstMethod == LIN_TREE_40){
		DPRINTF(MissBWPolicyExtra, "Returning 0 for lin-tree-40 policy on CPU %d\n", cpuID);
		return 0;
	}
	else if (perfEstMethod == LIN_TREE_80){
		DPRINTF(MissBWPolicyExtra, "Returning 0 for lin-tree-80 policy on CPU %d\n", cpuID);
		return 0;
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
	else if(perfEstMethod == PRIVATE_LATENCY_ONLY){
		DPRINTF(MissBWPolicyExtra, "Running private-latency-only method with shared lat %f, requests %f, private stall cycles %f, %f stall cycles\n",
								currentAvgSharedLat,
								sharedRequests,
								privateStallTime,
								currentStallTime);

		double newStallTime = privateStallTime + newAvgSharedLat * sharedRequests;

		DPRINTF(MissBWPolicyExtra, "Estimated new stall time %f with new shared lat %f\n",
							        newStallTime,
							        newAvgSharedLat);

		if(newStallTime < 0){
			warn("private-latency-only policy attempted to return %d stall cycles, changing to 0", newStallTime);
			return 0;
		}

		assert(newStallTime >= 0);

		return newStallTime;

	}
	else if(perfEstMethod == SHARED_STALL){
		DPRINTF(MissBWPolicyExtra, "Running shared-latency-only method with shared lat %f, requests %f, private stall cycles %f, %f stall cycles\n",
								currentAvgSharedLat,
								sharedRequests,
								privateStallTime,
								currentStallTime);

		double newStallTime = privateStallTime+currentStallTime;

		DPRINTF(MissBWPolicyExtra, "Estimated new stall time %f\n",
							        newStallTime);

		assert(newStallTime >= 0);

		return newStallTime;

	}
	else if(perfEstMethod == ZERO_STALL){
		DPRINTF(MissBWPolicyExtra, "Running zero-stall method with shared lat %f, requests %f, private stall cycles %f, %f stall cycles\n",
								currentAvgSharedLat,
								sharedRequests,
								privateStallTime,
								currentStallTime);

		double newStallTime = privateStallTime;
		assert(newStallTime >= 0);

		DPRINTF(MissBWPolicyExtra, "Estimated new stall time is %f\n",
							        newStallTime);


		return newStallTime;

	}
	else if(perfEstMethod == CPL || perfEstMethod == CPL_CWP
			|| perfEstMethod == CPL_DAMP || perfEstMethod == CPL_CWP_DAMP
			|| perfEstMethod == CPL_HYBRID || perfEstMethod == CPL_HYBRID_DAMP){
		computedOverlap[cpuID] = 0.0;
		if(sharedRequests == 0 || cpl == 0){
			DPRINTF(MissBWPolicyExtra, "No shared requests or cpl=0, returning private stall time %d (reqs=%d, cpl=%d)\n", privateStallTime, sharedRequests, cpl);
			return privateStallTime;
		}

		double newStallTime = 0.0;
		double cplAloneEstimate = cpl * newAvgSharedLat;
		double cplCWPAloneEstimate =  cpl * (newAvgSharedLat - cwp);

		double cplSharedEstimate = cpl * currentAvgSharedLat;
		double sharedModelError = computeRawError(cplSharedEstimate, currentStallTime);
		lastModelError[cpuID] = sharedModelError;

		if(perfEstMethod == CPL){
			newStallTime = cplAloneEstimate;
			lastCPLPolicyDesicion[cpuID] = CPL;
		}
		else if(perfEstMethod == CPL_DAMP){
			newStallTime = computeDampedEstimate(cplAloneEstimate, cplSharedEstimate, currentStallTime, cpuID);
			lastCPLPolicyDesicion[cpuID] = CPL_DAMP;
		}
		else if(perfEstMethod == CPL_CWP){
			newStallTime = cplCWPAloneEstimate;
			lastCPLPolicyDesicion[cpuID] = CPL_CWP;
		}
		else if(perfEstMethod == CPL_CWP_DAMP){
			newStallTime = computeDampedEstimate(cplCWPAloneEstimate, cplSharedEstimate, currentStallTime, cpuID);
			lastCPLPolicyDesicion[cpuID] = CPL_CWP_DAMP;
		}
		else if(perfEstMethod == CPL_HYBRID || perfEstMethod == CPL_HYBRID_DAMP){

		    addHybridError(cpuID, sharedModelError);
		    double avgError = getHybridAverageError(cpuID);

			DPRINTF(MissBWPolicyExtra, "Hybrid scheme, current error %d, average error %d, desicion error %d\n",
					sharedModelError,
					avgError,
					hybridDesicionError);

			if(avgError < hybridDesicionError){ // Underestimate, don't subtract and make the error larger
				if(perfEstMethod == CPL_HYBRID_DAMP){
					newStallTime = computeDampedEstimate(cplAloneEstimate, cplSharedEstimate, currentStallTime, cpuID);
					DPRINTF(MissBWPolicyExtra, "Choosing damped CPL model, returning estimate %d\n", newStallTime);

					lastCPLPolicyDesicion[cpuID] = CPL_DAMP;
				}
				else{
					newStallTime = cplAloneEstimate;
					DPRINTF(MissBWPolicyExtra, "Choosing CPL model, returning estimate %d\n", newStallTime);
					lastCPLPolicyDesicion[cpuID] = CPL;
				}
			}
			else{ // Overestimate, we can safely subtract the commit overlap
				if(perfEstMethod == CPL_HYBRID_DAMP){
					newStallTime = computeDampedEstimate(cplCWPAloneEstimate, cplSharedEstimate, currentStallTime, cpuID);
					DPRINTF(MissBWPolicyExtra, "Choosing damped CPL-CWP model, returning estimate %d\n", newStallTime);

					lastCPLPolicyDesicion[cpuID] = CPL_CWP_DAMP;
				}
				else{
					newStallTime = cplCWPAloneEstimate;
					DPRINTF(MissBWPolicyExtra, "Choosing CPL-CWP model, returning estimate %d\n", newStallTime);
					lastCPLPolicyDesicion[cpuID] = CPL_CWP;
				}
			}

		}
		else{
			fatal("unknown CPL-based method");
		}


		DPRINTF(MissBWPolicyExtra, "Computed new stall time %f with new shared latency %f, private stall time is %f, current stall %f, current latency %f, %d cpl, %f cwp\n",
				newStallTime,
				newAvgSharedLat,
				privateStallTime,
				currentStallTime,
				currentAvgSharedLat,
				cpl,
				cwp);

		if(privateStallTime+newStallTime < 0.0){
			DPRINTF(MissBWPolicyExtra, "Returning stall time 0 since the estimate was negaive\n");
			return 0;
		}
		return privateStallTime+newStallTime;
	}
	else if(perfEstMethod == BOIS){
	    DPRINTF(MissBWPolicyExtra, "Returning Bois estimate of stall time, %d cycles\n", boisAloneStallEst);
	    return boisAloneStallEst;
	}
	else if(perfEstMethod == ITCA || perfEstMethod == ASR){
		//ITCA and ASR are handled directly in the trace since they do not partition the categories like we do
		return 0.0;
	}

	fatal("Unknown performance estimation method");
	return 0.0;
}

template<typename T>
std::vector<RequestTraceEntry>
BasePolicy::getTraceCurve(std::vector<T> data){
	vector<RequestTraceEntry> reqData;
	for(int i=0;i<data.size();i++){
		reqData.push_back(RequestTraceEntry(data[i]));
	}
	return reqData;
}

std::vector<RequestTraceEntry>
BasePolicy::getTraceCurveInt(std::vector<int> data){
	return getTraceCurve(data);
}

std::vector<RequestTraceEntry>
BasePolicy::getTraceCurveDbl(std::vector<double> data){
	return getTraceCurve(data);
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
BasePolicy::initPerfModelTrace(int cpuCount){
	vector<string> headers;

	headers.push_back("Cummulative Committed Instructions");
	headers.push_back("Bandwidth allocation");
	headers.push_back("Bandwidth use");
	headers.push_back("Actual Bus Service Latency");
	headers.push_back("Actual Bus Latency");
	headers.push_back("Little");
	for(double i=1.0; i<2.01; i+=0.1){
		stringstream graphead;
		graphead << "Graph Model " << i;
		headers.push_back(graphead.str());

		stringstream histhead;
		histhead << "Histogram Model " << i;
		headers.push_back(histhead.str());
	}

	perfModelTraces.resize(cpuCount, RequestTrace());
	for(int i=0;i<cpuCount;i++){
		perfModelTraces[i] = RequestTrace(name(), RequestTrace::buildFilename("PerfModel", i).c_str());
		perfModelTraces[i].initalizeTrace(headers);
	}
}

void
BasePolicy::doPerformanceModelTrace(int cpuID, PerformanceModelMeasurements modelMeasurements){
	vector<RequestTraceEntry> data;

	data.push_back(comInstModelTraceCummulativeInst[cpuID]);
	data.push_back(modelMeasurements.bandwidthAllocation);
	data.push_back(modelMeasurements.getActualBusUtilization());
	data.push_back(modelMeasurements.avgMemoryBusServiceLat);
	data.push_back(modelMeasurements.avgMemoryBusQueueLat);
	data.push_back(modelMeasurements.getLittlesLawBusQueueLatency());
	for(double i=1.0; i<2.01; i+=0.1){
		data.push_back(modelMeasurements.getGraphModelBusQueueLatency(i));
		data.push_back(modelMeasurements.getGraphHistorgramBusQueueLatency(i));
	}
	perfModelTraces[cpuID].addTrace(data);
}

void
BasePolicy::initComInstModelTrace(int cpuCount){
	vector<string> headers;

	headers.push_back("Cummulative Committed Instructions");
	headers.push_back("Total Cycles");
	headers.push_back("Stall Cycles");
	headers.push_back("Private Stall Cycles");
	headers.push_back("Shared+Priv Memsys Stalls");
	headers.push_back("Write Stall Cycles");
	headers.push_back("Private Blocked Stall Cycles");
	headers.push_back("Compute Cycles");
	headers.push_back("Memory Independent Stalls");
	headers.push_back("Empty ROB Stall Cycles");
	headers.push_back("Total Requests");
	headers.push_back("Total Latency");
	headers.push_back("Hidden Loads");
	headers.push_back("Table CPL");
	headers.push_back("Graph CPL");
	headers.push_back("CWP");
	headers.push_back("CWP-table");
	headers.push_back("Num Write Stalls");

	if(cpuCount > 1){
		headers.push_back("Average Shared Latency");
		headers.push_back("Average Shared Private Memsys Latency");
		headers.push_back("Estimated Private Latency");
		headers.push_back("CPT Private Mode Latency Estimate");
		headers.push_back("Shared IPC");
		headers.push_back("Estimated Alone IPC");
		headers.push_back("Measured Shared Overlap");
		headers.push_back("Estimated Alone Overlap");
		headers.push_back("Private Mode Miss Rate Estimate");
		headers.push_back("Stall Estimate");
		headers.push_back("Alone Write Stall Estimate");
		headers.push_back("Alone Private Blocked Stall Estimate");
		headers.push_back("Shared Store Lat");
		headers.push_back("Estimated Alone Store Lat");
		headers.push_back("Num Shared Stores");
		headers.push_back("Alone Empty ROB Stall Estimate");
		headers.push_back("Bois et al. Alone Stall Estimate");
		headers.push_back("Damping Model Error");
		headers.push_back("Damping Model Error w/ Cutoff");
		headers.push_back("Policy type");
		headers.push_back("Average Shared Model Error");
		headers.push_back("ITCA Accounted Cycles");
		headers.push_back("Private LLC Hit Estimate");
		headers.push_back("Private LLC Access Estimate");
		headers.push_back("Private LLC Writeback Estimate");
		headers.push_back("LLC Interference Estimate");
		headers.push_back("Shared LLC Hits");
		headers.push_back("Shared LLC Accesses");
		headers.push_back("Shared LLC Writebacks");

	}
	else{
		headers.push_back("Alone Memory Latency");
		headers.push_back("Average Alone Private Memsys Latency");
		headers.push_back("Actual CPT Private Mode Latency");
		headers.push_back("Measured Alone IPC");
		headers.push_back("Measured Alone Overlap");
		headers.push_back("Measured Private Mode Miss Rate");
		headers.push_back("Actual Stall");
		headers.push_back("CPL Stall Estimate");
		headers.push_back("CPL-CWP Stall Estimate");
		headers.push_back("CPL-table Stall Estimate");
		headers.push_back("CPL-CWP-table Stall Estimate");
		headers.push_back("Shared LLC Hits");
		headers.push_back("Shared LLC Accesses");
		headers.push_back("Shared LLC Writebacks");
	}

	comInstModelTraces.resize(cpuCount, RequestTrace());
	for(int i=0;i<cpuCount;i++){
		comInstModelTraces[i] = RequestTrace(name(), RequestTrace::buildFilename("CommittedInsts", i).c_str());
		comInstModelTraces[i].initalizeTrace(headers);
	}
}

void
BasePolicy::updatePrivPerfEst(int cpuID,
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
							  OverlapStatistics ols,
							  double privateMissRate,
							  double cwp,
							  double privateBlockedStall,
							  double avgSharedStoreLat,
							  double avgPrivmodeStoreLat,
							  double numStores,
							  int numWriteStalls,
							  int emptyROBStallCycles,
							  Tick boisAloneStallEst,
							  CacheAccessMeasurement privateLLCEstimates,
							  CacheAccessMeasurement sharedLLCMeasurements){

	vector<RequestTraceEntry> data;

	DPRINTF(MissBWPolicy, "-- Running alone IPC estimation trace for CPU %d, %d cycles in sample, %d commit cycles, %d committed insts, private stall %d, shared stall %d, cpl %d, cwp %d\n",
			              cpuID,
			              cyclesInSample,
			              commitCycles,
			              committedInsts,
			              privateStallCycles,
			              stallCycles,
			              ols.tableCPL,
			              cwp);

	comInstModelTraceCummulativeInst[cpuID] += committedInsts;

	data.push_back(RequestTraceEntry(comInstModelTraceCummulativeInst[cpuID], TICK_TRACE));
	data.push_back(cyclesInSample);
	data.push_back(stallCycles);
	data.push_back(privateStallCycles);
	data.push_back(stallCycles + privateStallCycles);
	data.push_back(writeStall);
	data.push_back(privateBlockedStall);
	data.push_back(commitCycles);
	data.push_back(memoryIndependentStallCycles);
	data.push_back(emptyROBStallCycles);
	data.push_back(reqs);
	data.push_back(reqs*(avgSharedLat+avgPrivateMemsysLat));
	data.push_back(hiddenLoads);
	data.push_back(ols.tableCPL);
	data.push_back(ols.graphCPL);
	data.push_back(cwp);
	data.push_back(ols.cptMeasurements.averageCPCWP());
	data.push_back(numWriteStalls);

	if(cpuCount > 1){

		double newStallEstimate = estimateStallCycles(stallCycles,
													  privateStallCycles,
													  avgSharedLat + avgPrivateMemsysLat,
													  avgPrivateLatEstimate + avgPrivateMemsysLat,
													  reqs,
													  cpuID,
													  ols.tableCPL,
													  privateMissRate,
													  cwp,
													  boisAloneStallEst,
													  ols.cptMeasurements);

		double writeStallEstimate = estimateWriteStallCycles(writeStall, avgPrivmodeStoreLat, numWriteStalls, avgSharedStoreLat);
		double alonePrivBlockedStallEstimate = estimatePrivateBlockedStall(privateBlockedStall, avgPrivateLatEstimate + avgPrivateMemsysLat, avgSharedLat + avgPrivateMemsysLat);
		double aloneROBStallEstimate = estimatePrivateROBStall(emptyROBStallCycles, avgPrivateLatEstimate + avgPrivateMemsysLat, avgSharedLat + avgPrivateMemsysLat);

		double sharedIPC = (double) committedInsts / (double) cyclesInSample;
		double aloneIPCEstimate = (double) committedInsts / (commitCycles + writeStallEstimate + memoryIndependentStallCycles + alonePrivBlockedStallEstimate + aloneROBStallEstimate + newStallEstimate);

		Tick curStallCycles = stallCycles + privateStallCycles;
		if(perfEstMethod == ITCA){
			aloneIPCEstimate = (double) committedInsts / (double) ols.itcaAccountedCycles.accountedCycles;
			assert(newStallEstimate == 0.0);
			assert(cyclesInSample == ols.itcaAccountedCycles.accountedCycles + ols.itcaAccountedCycles.notAccountedCycles);

			if(curStallCycles > ols.itcaAccountedCycles.notAccountedCycles){
				newStallEstimate = curStallCycles - ols.itcaAccountedCycles.notAccountedCycles;
			}
			else{
				newStallEstimate = 0.0;
			}
			assert(newStallEstimate >= 0.0);
		}
		else if(perfEstMethod == ASR){
			assert(asmPrivateModeSlowdownEsts[cpuID] >= 0.0);
			aloneIPCEstimate = asmPrivateModeSlowdownEsts[cpuID] * sharedIPC;
			newStallEstimate = curStallCycles / asmPrivateModeSlowdownEsts[cpuID];
			DPRINTF(MissBWPolicy, "CPU %d - Computed alone IPC estimate %f and stall estimate %f with speed-up %f\n",
					cpuID, aloneIPCEstimate, newStallEstimate, asmPrivateModeSlowdownEsts[cpuID]);
		}
		else if(perfEstMethod == CONST_1){
		     aloneIPCEstimate = 1.0;
		}
		else if (perfEstMethod == LIN_TREE_10){
			 if ( commitCycles <= 352755.0 ) {
			     if ( memoryIndependentStallCycles <= 9638.5 ) {
			         if ( comInstModelTraceCummulativeInst[cpuID] <= 340454880.0 ) {
			             if ( avgPrivateMemsysLat <= 34.475 ) {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 7.38870610594089960443
			                 + privateLLCEstimates.writebacks * -0.00002831280545605063
			                 + privateStallCycles * 0.00000149512729414108
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000417992212626
			                 + avgSharedLat * 0.00000678720669058228
			                 + commitCycles * -0.00000416030547265136
			                 + memoryIndependentStallCycles * 0.00000136536165244779
			                 + avgPrivateMemsysLat * -0.00523248947545718133
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000036288845755
			                 + sharedLLCMeasurements.accesses * -0.00000062984847764610
			                 + privateLLCEstimates.hits * 0.00003598996585736574
			                 + sharedLLCMeasurements.hits * -0.00003463505042512470
			                 + 0.4910389455626179 ;
			             } else {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 10.89048002494468825319
			                 + privateLLCEstimates.writebacks * -0.00001342421928260556
			                 + privateStallCycles * 0.00000077446260191706
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000170360570857
			                 + avgSharedLat * -0.00000117001467200722
			                 + commitCycles * -0.00000614020820916986
			                 + memoryIndependentStallCycles * 0.00000960493963399034
			                 + avgPrivateMemsysLat * -0.00408535872215065078
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000003874787414
			                 + sharedLLCMeasurements.accesses * -0.00000845232411459461
			                 + privateLLCEstimates.hits * 0.00001210401073590238
			                 + sharedLLCMeasurements.hits * 0.00000412686072818827
			                 + 0.23211174569516052 ;
			             }
			         } else {
			             aloneIPCEstimate = 0.0
			             + sharedIPC * 6.55353868157604679823
			             + privateLLCEstimates.writebacks * -0.00001236232042188747
			             + privateStallCycles * -0.00000041855395177433
			             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001678693907978
			             + avgSharedLat * -0.00001973720330515516
			             + commitCycles * -0.00000328627354700895
			             + memoryIndependentStallCycles * -0.00000320634455008444
			             + avgPrivateMemsysLat * 0.00248869628700962471
			             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000003259828636
			             + sharedLLCMeasurements.accesses * -0.00003142225257298679
			             + privateLLCEstimates.hits * 0.00001975061098288294
			             + sharedLLCMeasurements.hits * 0.00000748923904206633
			             + 1.1682410403673815 ;
			         }
			     } else {
			         if ( privateLLCEstimates.writebacks <= 8.5 ) {
			             if ( privateLLCEstimates.hits <= 5248.0 ) {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 6.62789384232712297518
			                 + privateLLCEstimates.writebacks * 0.00430473864526433828
			                 + privateStallCycles * -0.00000020424123700677
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001455694598630
			                 + avgSharedLat * 0.00008401082413282614
			                 + commitCycles * -0.00000244629215532733
			                 + memoryIndependentStallCycles * -0.00000096040815399643
			                 + avgPrivateMemsysLat * -0.03236121779413347194
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000007981113600
			                 + sharedLLCMeasurements.accesses * -0.00001867264546377056
			                 + privateLLCEstimates.hits * 0.00006034407971013183
			                 + sharedLLCMeasurements.hits * -0.00002895454539638928
			                 + 1.563341897783894 ;
			             } else {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 6.96425470639859600652
			                 + privateLLCEstimates.writebacks * 0.02350980224061263030
			                 + privateStallCycles * -0.00000095808512243490
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000068780103227
			                 + avgSharedLat * 0.00015467418446783439
			                 + commitCycles * -0.00000257931956090414
			                 + memoryIndependentStallCycles * -0.00000163816598165777
			                 + avgPrivateMemsysLat * -0.00928155017755461463
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000003432349197
			                 + sharedLLCMeasurements.accesses * -0.00002776331107342362
			                 + privateLLCEstimates.hits * 0.00002934151772002666
			                 + sharedLLCMeasurements.hits * 0.00000020126515034387
			                 + 0.862225728316941 ;
			             }
			         } else {
			             if ( sharedLLCMeasurements.accesses <= 7212.5 ) {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 3.30239900091147609018
			                 + privateLLCEstimates.writebacks * -0.00005479543386411221
			                 + privateStallCycles * -0.00000056268723167970
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000446807806869
			                 + avgSharedLat * 0.00002582672523033202
			                 + commitCycles * -0.00000042812601077419
			                 + memoryIndependentStallCycles * -0.00000028888871639086
			                 + avgPrivateMemsysLat * -0.08794337337355941087
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000004253247978
			                 + sharedLLCMeasurements.accesses * -0.00004478271686249872
			                 + privateLLCEstimates.hits * 0.00003125597178719490
			                 + sharedLLCMeasurements.hits * 0.00000310931664886835
			                 + 3.6423692155084657 ;
			             } else {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 1.51068618944733157328
			                 + privateLLCEstimates.writebacks * -0.00000982823921302763
			                 + privateStallCycles * -0.00000052674329112023
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000616397605701
			                 + avgSharedLat * 0.00006343603072017655
			                 + commitCycles * 0.00000030468545969817
			                 + memoryIndependentStallCycles * 0.00000008511414755492
			                 + avgPrivateMemsysLat * -0.00781488687008754200
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000905689324
			                 + sharedLLCMeasurements.accesses * -0.00000486043823090163
			                 + privateLLCEstimates.hits * 0.00001207081509035486
			                 + sharedLLCMeasurements.hits * -0.00000757520603058233
			                 + 0.49828340731304166 ;
			             }
			         }
			     }
			 } else {
			     if ( sharedIPC <= 0.916 ) {
			         if ( privateLLCEstimates.writebacks <= 2561.5 ) {
			             aloneIPCEstimate = 0.0
			             + sharedIPC * 1.40548464371730186251
			             + privateLLCEstimates.writebacks * -0.00005792605506600614
			             + privateStallCycles * -0.00000048817387368792
			             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000016088889630
			             + avgSharedLat * 0.00007044937743954312
			             + commitCycles * -0.00000014036272658479
			             + memoryIndependentStallCycles * -0.00000023374484739734
			             + avgPrivateMemsysLat * -0.01454517710081586722
			             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000003130952018
			             + sharedLLCMeasurements.accesses * -0.00001111900458937061
			             + privateLLCEstimates.hits * 0.00001472943848290128
			             + sharedLLCMeasurements.hits * -0.00000426840762545502
			             + 1.2356703991425926 ;
			         } else {
			             aloneIPCEstimate = 0.0
			             + sharedIPC * 0.40313356391629223685
			             + privateLLCEstimates.writebacks * -0.00001298989043790454
			             + privateStallCycles * -0.00000014908840045040
			             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001064797441806
			             + avgSharedLat * 0.00004673042237623654
			             + commitCycles * 0.00000006582328864848
			             + memoryIndependentStallCycles * 0.00000002599142400573
			             + avgPrivateMemsysLat * -0.00745450660141851792
			             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000003173133580
			             + sharedLLCMeasurements.accesses * -0.00000684782014356458
			             + privateLLCEstimates.hits * 0.00000525076598297988
			             + sharedLLCMeasurements.hits * 0.00000207567619487386
			             + 0.9830293979098383 ;
			         }
			     } else {
			         aloneIPCEstimate = 0.0
			         + sharedIPC * 0.80640462295572468143
			         + privateLLCEstimates.writebacks * -0.00006549802905913126
			         + privateStallCycles * -0.00000014754486434524
			         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000062815007710
			         + avgSharedLat * 0.00004727657407580891
			         + commitCycles * -0.00000006769874613084
			         + memoryIndependentStallCycles * -0.00000009932471844974
			         + avgPrivateMemsysLat * 0.00049329092593352513
			         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000988625298
			         + sharedLLCMeasurements.accesses * 0.00000737555875668508
			         + privateLLCEstimates.hits * 0.00000234461948203558
			         + sharedLLCMeasurements.hits * -0.00001032047708812683
			         + 0.6814175178388526 ;
			     }
			 }
			 if (aloneIPCEstimate < 0.0) {
				 aloneIPCEstimate = 0.0;
			 }
		}
		else if (perfEstMethod == LIN_TREE_40){
			 if ( commitCycles <= 352755.0 ) {
			     if ( memoryIndependentStallCycles <= 9638.5 ) {
			         if ( comInstModelTraceCummulativeInst[cpuID] <= 340454880.0 ) {
			             if ( avgPrivateMemsysLat <= 34.475 ) {
			                 if ( sharedIPC <= 0.135 ) {
			                     if ( privateLLCEstimates.writebacks <= 64.5 ) {
			                         if ( comInstModelTraceCummulativeInst[cpuID] <= 111658564.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 16.28719564570976885420
			                             + privateLLCEstimates.writebacks * 0.00000103675359330699
			                             + privateStallCycles * -0.00000115631191264676
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001172318293543
			                             + avgSharedLat * 0.00027741476405235694
			                             + commitCycles * -0.00000526877671462125
			                             + memoryIndependentStallCycles * 0.00003756076842336324
			                             + avgPrivateMemsysLat * -0.00785347720411105205
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000050237563129
			                             + sharedLLCMeasurements.accesses * -0.00003555659186325228
			                             + privateLLCEstimates.hits * 0.00003704421766800133
			                             + sharedLLCMeasurements.hits * 0.00001145315865427923
			                             + 0.012957292543546606 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 11.46403997311641909107
			                             + privateLLCEstimates.writebacks * 0.00001969554470138410
			                             + privateStallCycles * 0.00000221415020744189
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000011782135370607
			                             + avgSharedLat * -0.00010241636026201726
			                             + commitCycles * -0.00001077710232411082
			                             + memoryIndependentStallCycles * -0.00000876293310216172
			                             + avgPrivateMemsysLat * -0.02060408546025581031
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000017217644669
			                             + sharedLLCMeasurements.accesses * -0.00005032560714662296
			                             + privateLLCEstimates.hits * 0.00004790334839577273
			                             + sharedLLCMeasurements.hits * 0.00001608812529365453
			                             + 1.3140768599137052 ;
			                         }
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 13.11916114534743371678
			                         + privateLLCEstimates.writebacks * -0.00002066140976864244
			                         + privateStallCycles * 0.00000839379928249424
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000429303283332
			                         + avgSharedLat * 0.00002525760036869242
			                         + commitCycles * -0.00000812175118189252
			                         + memoryIndependentStallCycles * 0.00000780746900343218
			                         + avgPrivateMemsysLat * -0.01022052765972785229
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000011457269571
			                         + sharedLLCMeasurements.accesses * -0.00000578681316262435
			                         + privateLLCEstimates.hits * 0.00001239091707462398
			                         + sharedLLCMeasurements.hits * -0.00000663490746943287
			                         + 0.5813875289461052 ;
			                     }
			                 } else {
			                     if ( privateLLCEstimates.writebacks <= 4752.5 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * -6.68081765180679720828
			                         + privateLLCEstimates.writebacks * -0.00007361014182612061
			                         + privateStallCycles * -0.00000330715534226984
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000010589813234509
			                         + avgSharedLat * 0.00058640341081643739
			                         + commitCycles * 0.00000988185486896277
			                         + memoryIndependentStallCycles * 0.00000902410005282542
			                         + avgPrivateMemsysLat * -0.06060386202054755117
			                         + comInstModelTraceCummulativeInst[cpuID] * -0.00000000010265752467
			                         + sharedLLCMeasurements.accesses * -0.00005503236414745511
			                         + privateLLCEstimates.hits * 0.00002399072697193433
			                         + sharedLLCMeasurements.hits * 0.00002477105728086053
			                         + 0.9940355143942493 ;
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 6.57078669476055754473
			                         + privateLLCEstimates.writebacks * -0.00002313449247403444
			                         + privateStallCycles * 0.00000080895300975723
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001666067944029
			                         + avgSharedLat * 0.00008504628349805385
			                         + commitCycles * -0.00000414577248594164
			                         + memoryIndependentStallCycles * 0.00001022417649131154
			                         + avgPrivateMemsysLat * -0.00286442110804163838
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000007807243738
			                         + sharedLLCMeasurements.accesses * 0.00001262613175011833
			                         + privateLLCEstimates.hits * -0.00001424605311857743
			                         + sharedLLCMeasurements.hits * -0.00000712050930505533
			                         + 0.4408257516047156 ;
			                     }
			                 }
			             } else {
			                 if ( sharedIPC <= 0.044 ) {
			                     aloneIPCEstimate = 0.0
			                     + sharedIPC * 7.48686680455214847996
			                     + privateLLCEstimates.writebacks * -0.00000536391625299057
			                     + privateStallCycles * -0.00000060621683491010
			                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000059639881096
			                     + avgSharedLat * 0.00000554924348000798
			                     + commitCycles * -0.00000195876777938719
			                     + memoryIndependentStallCycles * 0.00000351790327094048
			                     + avgPrivateMemsysLat * 0.00740396164673329249
			                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000023225430597
			                     + sharedLLCMeasurements.accesses * -0.00000661871364666899
			                     + privateLLCEstimates.hits * 0.00000575432865418213
			                     + sharedLLCMeasurements.hits * -0.00000005723881939648
			                     + -0.19600624560516988 ;
			                 } else {
			                     if ( avgPrivateMemsysLat <= 34.914 ) {
			                         if ( privateLLCEstimates.writebacks <= 128.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 26.86153265085807007040
			                             + privateLLCEstimates.writebacks * 0.00000965045934508894
			                             + privateStallCycles * 0.00000793761867056356
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000937110740300
			                             + avgSharedLat * -0.00005619079604642308
			                             + commitCycles * -0.00001967232790421835
			                             + memoryIndependentStallCycles * -0.00002386017521436233
			                             + avgPrivateMemsysLat * 0.07924031779865443881
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000039771002251
			                             + sharedLLCMeasurements.accesses * -0.00001483937073686090
			                             + privateLLCEstimates.hits * 0.00000865807491833840
			                             + sharedLLCMeasurements.hits * 0.00001138358357467626
			                             + -1.2598747162520945 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 1.13026524355496671426
			                             + privateLLCEstimates.writebacks * -0.00000914725058750164
			                             + privateStallCycles * -0.00000284298006128302
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000938676019186
			                             + avgSharedLat * 0.00001640189550711577
			                             + commitCycles * -0.00000034368826323266
			                             + memoryIndependentStallCycles * -0.00000028647589510720
			                             + avgPrivateMemsysLat * -0.04197267764775082510
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000037627363991
			                             + sharedLLCMeasurements.accesses * -0.00000004479529297424
			                             + privateLLCEstimates.hits * 0.00000869985699693765
			                             + sharedLLCMeasurements.hits * -0.00000124270476709196
			                             + 1.877169752410697 ;
			                         }
			                     } else {
			                         if ( memoryIndependentStallCycles <= 43.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 2.77430724148045371180
			                             + privateLLCEstimates.writebacks * -0.00001118889468950087
			                             + privateStallCycles * 0.00000336990957257329
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000674299455078
			                             + avgSharedLat * 0.00002828422153593376
			                             + commitCycles * -0.00000041147610204332
			                             + memoryIndependentStallCycles * -0.00062332334126023996
			                             + avgPrivateMemsysLat * 0.02476513832554189110
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000012269028363
			                             + sharedLLCMeasurements.accesses * -0.00000948795136046326
			                             + privateLLCEstimates.hits * 0.00003814363053465643
			                             + sharedLLCMeasurements.hits * -0.00002210267455049624
			                             + -0.6386059475395982 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 4.47377374209896494506
			                             + privateLLCEstimates.writebacks * -0.00000613183796693101
			                             + privateStallCycles * 0.00000003217533095876
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000056633461957
			                             + avgSharedLat * -0.00000014376975324634
			                             + commitCycles * -0.00000226997241629975
			                             + memoryIndependentStallCycles * 0.00000675106013308546
			                             + avgPrivateMemsysLat * 0.01280453402945931976
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000008220172542
			                             + sharedLLCMeasurements.accesses * -0.00000588002333233316
			                             + privateLLCEstimates.hits * 0.00000843769317590591
			                             + sharedLLCMeasurements.hits * 0.00000003782910882020
			                             + -0.31670751959462184 ;
			                         }
			                     }
			                 }
			             }
			         } else {
			             if ( privateLLCEstimates.writebacks <= 1792.0 ) {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 15.22608159036144748200
			                 + privateLLCEstimates.writebacks * -0.00003165389878963858
			                 + privateStallCycles * 0.00000242750767612715
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000001331452201017
			                 + avgSharedLat * -0.00009652144434897658
			                 + commitCycles * -0.00001149287367690315
			                 + memoryIndependentStallCycles * -0.00000633640524926112
			                 + avgPrivateMemsysLat * 0.00024382718017727911
			                 + comInstModelTraceCummulativeInst[cpuID] * -0.00000000000080088070
			                 + sharedLLCMeasurements.accesses * -0.00001672917168418359
			                 + privateLLCEstimates.hits * 0.00000416324010158601
			                 + sharedLLCMeasurements.hits * 0.00000908960013436283
			                 + 1.3751839640242571 ;
			             } else {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 4.63842323095066078054
			                 + privateLLCEstimates.writebacks * -0.00000491955433472024
			                 + privateStallCycles * 0.00000063579894771534
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000368590772114
			                 + avgSharedLat * -0.00001696319887291796
			                 + commitCycles * -0.00000390926762204837
			                 + memoryIndependentStallCycles * 0.00000250317142527844
			                 + avgPrivateMemsysLat * -0.00469263300469139645
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000425834597
			                 + sharedLLCMeasurements.accesses * 0.00000138058532242088
			                 + privateLLCEstimates.hits * 0.00000669863559729391
			                 + sharedLLCMeasurements.hits * -0.00000365320323815043
			                 + 0.3303744485708626 ;
			             }
			         }
			     } else {
			         if ( privateLLCEstimates.writebacks <= 8.5 ) {
			             if ( privateLLCEstimates.hits <= 5248.0 ) {
			                 if ( privateLLCEstimates.accesses <= 5504.0 ) {
			                     if ( numStores <= 295.0 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 2.83436937650413378620
			                         + privateLLCEstimates.writebacks * -0.00001242802943924370
			                         + privateStallCycles * -0.00000328569505037333
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000002242353468408
			                         + avgSharedLat * 0.00006153576141791562
			                         + commitCycles * 0.00000216737332614033
			                         + memoryIndependentStallCycles * -0.00000359568268805511
			                         + avgPrivateMemsysLat * -0.03086051534511085465
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000003559301311
			                         + sharedLLCMeasurements.accesses * -0.00006112674274133977
			                         + privateLLCEstimates.hits * 0.00007316837963733626
			                         + sharedLLCMeasurements.hits * -0.00000626716320682119
			                         + 1.8332402516196304 ;
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 25.47785113624887642914
			                         + privateLLCEstimates.writebacks * 0.01287491052788742457
			                         + privateStallCycles * 0.00000265880527139156
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000305502538545
			                         + avgSharedLat * 0.00003778085139919653
			                         + commitCycles * -0.00001358592458341546
			                         + memoryIndependentStallCycles * -0.00000021287912054159
			                         + avgPrivateMemsysLat * -0.07462480802123143409
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000010927553046
			                         + sharedLLCMeasurements.accesses * -0.00005794898188997125
			                         + privateLLCEstimates.hits * 0.00007236906681075521
			                         + sharedLLCMeasurements.hits * 0.00000797264492805739
			                         + 2.9486798496806688 ;
			                     }
			                 } else {
			                     aloneIPCEstimate = 0.0
			                     + sharedIPC * 4.74372615299671895883
			                     + privateLLCEstimates.writebacks * -0.00000064580277035966
			                     + privateStallCycles * -0.00000063786486609164
			                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001107030683260
			                     + avgSharedLat * 0.00008822290416000812
			                     + commitCycles * -0.00000166488574779547
			                     + memoryIndependentStallCycles * -0.00000064664185888730
			                     + avgPrivateMemsysLat * -0.02190009132997440544
			                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000013564188744
			                     + sharedLLCMeasurements.accesses * -0.00000796768697255741
			                     + privateLLCEstimates.hits * 0.00003449724452186741
			                     + sharedLLCMeasurements.hits * -0.00002586968099028059
			                     + 1.1420379776947298 ;
			                 }
			             } else {
			                 if ( sharedLLCMeasurements.hits <= 5002.5 ) {
			                     if ( sharedIPC <= 0.068 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * -1.15158986929778750508
			                         + privateLLCEstimates.writebacks * -0.00000062525765791461
			                         + privateStallCycles * -0.00000844643509196495
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000002613799121303
			                         + avgSharedLat * -0.00001856875357147709
			                         + commitCycles * 0.00000547840393461182
			                         + memoryIndependentStallCycles * -0.00000024901677981043
			                         + avgPrivateMemsysLat * -0.04017880527436720750
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000002105931708
			                         + sharedLLCMeasurements.accesses * -0.00009367714102743454
			                         + privateLLCEstimates.hits * 0.00005474281735769995
			                         + sharedLLCMeasurements.hits * 0.00009607054988149570
			                         + 1.8365946213026436 ;
			                     } else {
			                         if ( comInstModelTraceCummulativeInst[cpuID] <= 9450266.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 16.08762914075800409819
			                             + privateLLCEstimates.writebacks * 0.00000026286301504562
			                             + privateStallCycles * 0.00000007690372749596
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000006038813496344
			                             + avgSharedLat * -0.00008051513071956370
			                             + commitCycles * -0.00000895031192951842
			                             + memoryIndependentStallCycles * -0.00000021404126518906
			                             + avgPrivateMemsysLat * 0.01956187354310316706
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000001335259502985
			                             + sharedLLCMeasurements.accesses * -0.00006165629154114006
			                             + privateLLCEstimates.hits * -0.00000055037539751923
			                             + sharedLLCMeasurements.hits * 0.00002029185293827375
			                             + 1.0082825267815925 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 8.82521944519533896312
			                             + privateLLCEstimates.writebacks * 0.02010427429511766478
			                             + privateStallCycles * -0.00000008113935142538
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000948179413731
			                             + avgSharedLat * 0.00009974130701358600
			                             + commitCycles * -0.00000471508880109647
			                             + memoryIndependentStallCycles * -0.00000108777156984333
			                             + avgPrivateMemsysLat * -0.03243304104793014175
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000001093693192
			                             + sharedLLCMeasurements.accesses * -0.00004658396676930477
			                             + privateLLCEstimates.hits * 0.00003982601276235331
			                             + sharedLLCMeasurements.hits * -0.00000883211388962071
			                             + 1.9229234497943941 ;
			                         }
			                     }
			                 } else {
			                     if ( comInstModelTraceCummulativeInst[cpuID] <= 107268392.0 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 9.86530398433641408928
			                         + privateLLCEstimates.writebacks * 0.00000037926106543069
			                         + privateStallCycles * 0.00000010367960708348
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000205817582414
			                         + avgSharedLat * 0.00002040794877543616
			                         + commitCycles * -0.00000531181334202850
			                         + memoryIndependentStallCycles * 0.00000078058854548670
			                         + avgPrivateMemsysLat * 0.00586152279019327201
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000005334434105
			                         + sharedLLCMeasurements.accesses * -0.00001426883721833124
			                         + privateLLCEstimates.hits * 0.00001986356575030105
			                         + sharedLLCMeasurements.hits * -0.00000449656008199945
			                         + 0.20202361893533427 ;
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 5.28152003477947395993
			                         + privateLLCEstimates.writebacks * 0.00000005572745468496
			                         + privateStallCycles * -0.00000022344603297165
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000314484768079
			                         + avgSharedLat * 0.00007227396711143884
			                         + commitCycles * -0.00000239883267102689
			                         + memoryIndependentStallCycles * -0.00000088694295819739
			                         + avgPrivateMemsysLat * -0.01928524751123063263
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000001014293390
			                         + sharedLLCMeasurements.accesses * -0.00000761806187444436
			                         + privateLLCEstimates.hits * 0.00000949761831183914
			                         + sharedLLCMeasurements.hits * -0.00000087060958518339
			                         + 1.4979304652715433 ;
			                     }
			                 }
			             }
			         } else {
			             if ( sharedLLCMeasurements.accesses <= 7212.5 ) {
			                 if ( avgPrivateMemsysLat <= 34.559 ) {
			                     aloneIPCEstimate = 0.0
			                     + sharedIPC * 2.13636695850648861139
			                     + privateLLCEstimates.writebacks * -0.00005453520278584195
			                     + privateStallCycles * -0.00000025558181288726
			                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001292366637174
			                     + avgSharedLat * 0.00001263929150493864
			                     + commitCycles * -0.00000048783115551357
			                     + memoryIndependentStallCycles * 0.00000021608744901154
			                     + avgPrivateMemsysLat * -0.15756308175571304142
			                     + comInstModelTraceCummulativeInst[cpuID] * -0.00000000000743377241
			                     + sharedLLCMeasurements.accesses * -0.00003820759463020708
			                     + privateLLCEstimates.hits * 0.00002886159837632160
			                     + sharedLLCMeasurements.hits * 0.00000988123596201310
			                     + 6.227972362702463 ;
			                 } else {
			                     aloneIPCEstimate = 0.0
			                     + sharedIPC * 5.22768989844751530427
			                     + privateLLCEstimates.writebacks * -0.00005210637715948315
			                     + privateStallCycles * 0.00000028021473418524
			                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000168547813440
			                     + avgSharedLat * 0.00004137329446268026
			                     + commitCycles * -0.00000140185002848144
			                     + memoryIndependentStallCycles * -0.00000010614658398405
			                     + avgPrivateMemsysLat * -0.03844915765825111104
			                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000021317447247
			                     + sharedLLCMeasurements.accesses * -0.00001819596883118789
			                     + privateLLCEstimates.hits * 0.00003082650522551001
			                     + sharedLLCMeasurements.hits * -0.00000889448708795644
			                     + 1.5821870103034588 ;
			                 }
			             } else {
			                 if ( reqs*(avgSharedLat+avgPrivateMemsysLat) <= 17806292.0 ) {
			                     if ( memoryIndependentStallCycles <= 26153.0 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 6.20439720867801014492
			                         + privateLLCEstimates.writebacks * -0.00002119749754937375
			                         + privateStallCycles * 0.00000390344075661937
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000459246721479
			                         + avgSharedLat * 0.00007221688255740835
			                         + commitCycles * -0.00000372817387826663
			                         + memoryIndependentStallCycles * 0.00000080870712868178
			                         + avgPrivateMemsysLat * -0.00611853001192113298
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000013462065444
			                         + sharedLLCMeasurements.accesses * 0.00000419989236505575
			                         + privateLLCEstimates.hits * -0.00000341499406743311
			                         + sharedLLCMeasurements.hits * -0.00000445683691710621
			                         + 0.41412199707242425 ;
			                     } else {
			                         if ( avgPrivateMemsysLat <= 34.218 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 5.57178257688615907739
			                             + privateLLCEstimates.writebacks * -0.00004574788584011854
			                             + privateStallCycles * 0.00000106335754131118
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001791022274503
			                             + avgSharedLat * 0.00008359505069348324
			                             + commitCycles * -0.00000196676099897275
			                             + memoryIndependentStallCycles * 0.00000014165728020222
			                             + avgPrivateMemsysLat * -0.11150566272167136694
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000007190613816
			                             + sharedLLCMeasurements.accesses * 0.00000020832183900976
			                             + privateLLCEstimates.hits * 0.00001565793331386226
			                             + sharedLLCMeasurements.hits * -0.00001827111017211048
			                             + 4.092000492788916 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 4.87244065633914757996
			                             + privateLLCEstimates.writebacks * -0.00001743266057197032
			                             + privateStallCycles * 0.00000007761066323347
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001310283034351
			                             + avgSharedLat * 0.00007654751499694905
			                             + commitCycles * -0.00000196995186200204
			                             + memoryIndependentStallCycles * 0.00000007385705956556
			                             + avgPrivateMemsysLat * -0.00480852566900915809
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000640271160
			                             + sharedLLCMeasurements.accesses * -0.00000277294699045442
			                             + privateLLCEstimates.hits * 0.00000688832763377741
			                             + sharedLLCMeasurements.hits * -0.00000367951588440132
			                             + 0.45044464591430927 ;
			                         }
			                     }
			                 } else {
			                     if ( sharedIPC <= 0.12 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 4.98528694027139529510
			                         + privateLLCEstimates.writebacks * -0.00000585719667800199
			                         + privateStallCycles * 0.00000020057508318771
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000068892780901
			                         + avgSharedLat * -0.00001811862004937366
			                         + commitCycles * -0.00000258681215597420
			                         + memoryIndependentStallCycles * 0.00000147746338469802
			                         + avgPrivateMemsysLat * 0.00212452221844375982
			                         + comInstModelTraceCummulativeInst[cpuID] * -0.00000000001916513528
			                         + sharedLLCMeasurements.accesses * -0.00000562340822517259
			                         + privateLLCEstimates.hits * 0.00000283418049519345
			                         + sharedLLCMeasurements.hits * 0.00000429361524664518
			                         + 0.13787944961425264 ;
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 0.11570246539568958266
			                         + privateLLCEstimates.writebacks * -0.00000331761609280002
			                         + privateStallCycles * -0.00000125768862178512
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000206880084892
			                         + avgSharedLat * -0.00007335773699605064
			                         + commitCycles * 0.00000142071101187086
			                         + memoryIndependentStallCycles * -0.00000181268799728050
			                         + avgPrivateMemsysLat * -0.00228675499964875028
			                         + comInstModelTraceCummulativeInst[cpuID] * -0.00000000011867653752
			                         + sharedLLCMeasurements.accesses * -0.00001767705213619842
			                         + privateLLCEstimates.hits * 0.00002748219790485737
			                         + sharedLLCMeasurements.hits * -0.00001547006361929919
			                         + 0.6918078109701662 ;
			                     }
			                 }
			             }
			         }
			     }
			 } else {
			     if ( sharedIPC <= 0.916 ) {
			         if ( privateLLCEstimates.writebacks <= 2561.5 ) {
			             if ( emptyROBStallCycles <= 88927.5 ) {
			                 if ( avgPrivateMemsysLat <= 38.403 ) {
			                     if ( privateLLCEstimates.writebacks <= 510.0 ) {
			                         if ( reqs <= 657.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -0.58786798852177868380
			                             + privateLLCEstimates.writebacks * -0.00022384627096680269
			                             + privateStallCycles * 0.00000011899876534011
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000006808962293784
			                             + avgSharedLat * 0.00000359636567269729
			                             + commitCycles * 0.00000013095979819093
			                             + memoryIndependentStallCycles * 0.00000017897331063252
			                             + avgPrivateMemsysLat * 0.00755719783551861857
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000188402093
			                             + sharedLLCMeasurements.accesses * -0.00001559594818287600
			                             + privateLLCEstimates.hits * 0.00001822285869401708
			                             + sharedLLCMeasurements.hits * 0.00000251979139420487
			                             + 1.0955723306767895 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 0.46888488523355870141
			                             + privateLLCEstimates.writebacks * -0.00012196697957878243
			                             + privateStallCycles * -0.00000021951895056933
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000112885417687
			                             + avgSharedLat * 0.00002018107146653200
			                             + commitCycles * 0.00000007080515074348
			                             + memoryIndependentStallCycles * -0.00000020983287289345
			                             + avgPrivateMemsysLat * -0.03152405719625851754
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000001421891152
			                             + sharedLLCMeasurements.accesses * -0.00001477944289879711
			                             + privateLLCEstimates.hits * 0.00001414319605438388
			                             + sharedLLCMeasurements.hits * 0.00000028409499795518
			                             + 2.121817007715151 ;
			                         }
			                     } else {
			                         if ( stallCycles <= 3979735.5 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 0.45392046906261601658
			                             + privateLLCEstimates.writebacks * -0.00001298369529789114
			                             + privateStallCycles * -0.00000017979914216339
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001164351330108
			                             + avgSharedLat * 0.00007997299687009853
			                             + commitCycles * 0.00000013461144175421
			                             + memoryIndependentStallCycles * -0.00000037998492397979
			                             + avgPrivateMemsysLat * -0.03172539139762626387
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000004132244834
			                             + sharedLLCMeasurements.accesses * -0.00000978959069482913
			                             + privateLLCEstimates.hits * 0.00001653797876286909
			                             + sharedLLCMeasurements.hits * -0.00000958234426861469
			                             + 2.0420877650519946 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -0.55386348777034499768
			                             + privateLLCEstimates.writebacks * -0.00007226100198535537
			                             + privateStallCycles * -0.00000064269011400622
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001228360065843
			                             + avgSharedLat * 0.00007686323227115347
			                             + commitCycles * 0.00000109822856308364
			                             + memoryIndependentStallCycles * -0.00000027418021014536
			                             + avgPrivateMemsysLat * -0.19986252283488981085
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000000746825361
			                             + sharedLLCMeasurements.accesses * -0.00000981887756259446
			                             + privateLLCEstimates.hits * 0.00002516757106113961
			                             + sharedLLCMeasurements.hits * -0.00000663987080723936
			                             + 7.590035783460197 ;
			                         }
			                     }
			                 } else {
			                     if ( stallCycles + privateStallCycles <= 4513635.0 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * -0.39192601025395751302
			                         + privateLLCEstimates.writebacks * -0.00018973425834487484
			                         + privateStallCycles * -0.00000020851897071831
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000002770003519
			                         + avgSharedLat * 0.00000135096524997669
			                         + commitCycles * 0.00000022215151211101
			                         + memoryIndependentStallCycles * 0.00000087068962967363
			                         + avgPrivateMemsysLat * -0.00491668393791670853
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000721079660
			                         + sharedLLCMeasurements.accesses * -0.00000852875255873644
			                         + privateLLCEstimates.hits * 0.00000712062635324307
			                         + sharedLLCMeasurements.hits * 0.00000079487182973249
			                         + 1.1074379573909718 ;
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * -0.05281852321503621023
			                         + privateLLCEstimates.writebacks * -0.00000337846699824029
			                         + privateStallCycles * 0.00000000918140203930
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000927999632285
			                         + avgSharedLat * 0.00015082296493302961
			                         + commitCycles * 0.00000045261200792900
			                         + memoryIndependentStallCycles * 0.00000029940564465235
			                         + avgPrivateMemsysLat * -0.00274928666298356007
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000107564676
			                         + sharedLLCMeasurements.accesses * 0.00000308890974855093
			                         + privateLLCEstimates.hits * -0.00000038476355702315
			                         + sharedLLCMeasurements.hits * -0.00000329447302683335
			                         + 0.2105732743837766 ;
			                     }
			                 }
			             } else {
			                 if ( avgPrivateMemsysLat <= 34.771 ) {
			                     aloneIPCEstimate = 0.0
			                     + sharedIPC * 2.27939348619344750446
			                     + privateLLCEstimates.writebacks * -0.00001671450500430813
			                     + privateStallCycles * -0.00000022330595109784
			                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000885714749460
			                     + avgSharedLat * 0.00002728983643895400
			                     + commitCycles * -0.00000066693474181929
			                     + memoryIndependentStallCycles * -0.00000038447760098844
			                     + avgPrivateMemsysLat * 0.00551170857358301673
			                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000001378394599
			                     + sharedLLCMeasurements.accesses * -0.00001407147072538140
			                     + privateLLCEstimates.hits * 0.00000494765299363218
			                     + sharedLLCMeasurements.hits * 0.00000021973248737264
			                     + 0.5670353199963708 ;
			                 } else {
			                     aloneIPCEstimate = 0.0
			                     + sharedIPC * 1.38962558694203708320
			                     + privateLLCEstimates.writebacks * -0.00014691078394050140
			                     + privateStallCycles * -0.00000024320293918057
			                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000038554741207
			                     + avgSharedLat * 0.00003296478416200532
			                     + commitCycles * -0.00000033416400143763
			                     + memoryIndependentStallCycles * -0.00000030006220185701
			                     + avgPrivateMemsysLat * 0.01791103538743956877
			                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000001126273513
			                     + sharedLLCMeasurements.accesses * -0.00002475242504997235
			                     + privateLLCEstimates.hits * 0.00002118542451488079
			                     + sharedLLCMeasurements.hits * 0.00000152229120912018
			                     + 0.2763635514733367 ;
			                 }
			             }
			         } else {
			             if ( privateLLCEstimates.writebacks <= 7812.0 ) {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 0.67320155228848421736
			                 + privateLLCEstimates.writebacks * -0.00002817184581204663
			                 + privateStallCycles * -0.00000026488440319482
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001060005540157
			                 + avgSharedLat * 0.00005899595393235624
			                 + commitCycles * 0.00000005530764422155
			                 + memoryIndependentStallCycles * -0.00000008736831958663
			                 + avgPrivateMemsysLat * -0.00741903989291633949
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000003607133808
			                 + sharedLLCMeasurements.accesses * -0.00000891298614870711
			                 + privateLLCEstimates.hits * 0.00001376895218650330
			                 + sharedLLCMeasurements.hits * -0.00000293113607999909
			                 + 0.9838703876542209 ;
			             } else {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 0.23215954074889680325
			                 + privateLLCEstimates.writebacks * -0.00000573964672299320
			                 + privateStallCycles * 0.00000025107311143732
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000534883978380
			                 + avgSharedLat * 0.00003332603226437621
			                 + commitCycles * 0.00000017750732259264
			                 + memoryIndependentStallCycles * -0.00000009373774483893
			                 + avgPrivateMemsysLat * -0.00725717584874307087
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000002663852813
			                 + sharedLLCMeasurements.accesses * -0.00000381899456453306
			                 + privateLLCEstimates.hits * 0.00000327722753104490
			                 + sharedLLCMeasurements.hits * 0.00000363766805247423
			                 + 0.7004763574813981 ;
			             }
			         }
			     } else {
			         if ( emptyROBStallCycles <= 31927.5 ) {
			             if ( reqs <= 28182.5 ) {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * -0.04752990092436758934
			                 + privateLLCEstimates.writebacks * -0.00011146694811208421
			                 + privateStallCycles * 0.00000002169965049265
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000138062464515
			                 + avgSharedLat * -0.00000718285802737168
			                 + commitCycles * 0.00000000163578998408
			                 + memoryIndependentStallCycles * 0.00000000841178872734
			                 + avgPrivateMemsysLat * -0.00240149366345838262
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000124569863
			                 + sharedLLCMeasurements.accesses * -0.00000374820974731197
			                 + privateLLCEstimates.hits * 0.00000207404800214285
			                 + sharedLLCMeasurements.hits * 0.00000230170258222302
			                 + 1.421784268000453 ;
			             } else {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 1.04475868622452661150
			                 + privateLLCEstimates.writebacks * 0.00000058556617402688
			                 + privateStallCycles * -0.00000004547518000335
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000001099392222216
			                 + avgSharedLat * 0.00165653398782415187
			                 + commitCycles * -0.00000002220393123571
			                 + memoryIndependentStallCycles * 0.00000005197844786869
			                 + avgPrivateMemsysLat * 0.00192472935271691559
			                 + comInstModelTraceCummulativeInst[cpuID] * -0.00000000000097276901
			                 + sharedLLCMeasurements.accesses * -0.00001083528543253553
			                 + privateLLCEstimates.hits * -0.00000057016772451476
			                 + sharedLLCMeasurements.hits * 0.00001045950137832121
			                 + -0.11561607303710397 ;
			             }
			         } else {
			             aloneIPCEstimate = 0.0
			             + sharedIPC * 1.03776649103207874170
			             + privateLLCEstimates.writebacks * -0.00001274823698329622
			             + privateStallCycles * -0.00000012675115680274
			             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000003029779334251
			             + avgSharedLat * 0.00003346118556334896
			             + commitCycles * -0.00000014617289967803
			             + memoryIndependentStallCycles * -0.00000011527798390005
			             + avgPrivateMemsysLat * -0.00029872326764515905
			             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000583414126
			             + sharedLLCMeasurements.accesses * -0.00000284618292574053
			             + privateLLCEstimates.hits * 0.00000033158663840145
			             + sharedLLCMeasurements.hits * -0.00000158965850232889
			             + 0.6008987325110197 ;
			         }
			     }
			 }
			 if (aloneIPCEstimate < 0.0) {
				 aloneIPCEstimate = 0.0;
			 }
		}
		else if (perfEstMethod == LIN_TREE_80) {
			if ( commitCycles <= 352755.0 ) {
			     if ( memoryIndependentStallCycles <= 9638.5 ) {
			         if ( comInstModelTraceCummulativeInst[cpuID] <= 340454880.0 ) {
			             if ( avgPrivateMemsysLat <= 34.475 ) {
			                 if ( sharedIPC <= 0.135 ) {
			                     if ( privateLLCEstimates.writebacks <= 64.5 ) {
			                         if ( comInstModelTraceCummulativeInst[cpuID] <= 111658564.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 16.28719564570976885420
			                             + privateLLCEstimates.writebacks * 0.00000103675359330699
			                             + privateStallCycles * -0.00000115631191264676
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001172318293543
			                             + avgSharedLat * 0.00027741476405235694
			                             + commitCycles * -0.00000526877671462125
			                             + memoryIndependentStallCycles * 0.00003756076842336324
			                             + avgPrivateMemsysLat * -0.00785347720411105205
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000050237563129
			                             + sharedLLCMeasurements.accesses * -0.00003555659186325228
			                             + privateLLCEstimates.hits * 0.00003704421766800133
			                             + sharedLLCMeasurements.hits * 0.00001145315865427923
			                             + 0.012957292543546606 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 11.46403997311641909107
			                             + privateLLCEstimates.writebacks * 0.00001969554470138410
			                             + privateStallCycles * 0.00000221415020744189
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000011782135370607
			                             + avgSharedLat * -0.00010241636026201726
			                             + commitCycles * -0.00001077710232411082
			                             + memoryIndependentStallCycles * -0.00000876293310216172
			                             + avgPrivateMemsysLat * -0.02060408546025581031
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000017217644669
			                             + sharedLLCMeasurements.accesses * -0.00005032560714662296
			                             + privateLLCEstimates.hits * 0.00004790334839577273
			                             + sharedLLCMeasurements.hits * 0.00001608812529365453
			                             + 1.3140768599137052 ;
			                         }
			                     } else {
			                         if ( hiddenLoads <= 6.0 ) {
			                             if ( privateLLCEstimates.writebacks <= 5743.5 ) {
			                                 if ( numWriteStalls <= 2603.5 ) {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * 18.41692819688914539711
			                                     + privateLLCEstimates.writebacks * -0.00003899879628787165
			                                     + privateStallCycles * 0.00000050427588704016
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000008512210668
			                                     + avgSharedLat * -0.00000223301068730705
			                                     + commitCycles * -0.00001211255801406265
			                                     + memoryIndependentStallCycles * 0.00000831288439477682
			                                     + avgPrivateMemsysLat * -0.00526576955217124883
			                                     + comInstModelTraceCummulativeInst[cpuID] * -0.00000000004202633757
			                                     + sharedLLCMeasurements.accesses * -0.00001521804947329406
			                                     + privateLLCEstimates.hits * 0.00000034527656108154
			                                     + sharedLLCMeasurements.hits * 0.00001980228031219498
			                                     + 0.5680656166331999 ;
			                                 } else {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * 345.16682044971668119615
			                                     + privateLLCEstimates.writebacks * -0.00013399191996790375
			                                     + privateStallCycles * -0.00084658041835227664
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000360621417330958
			                                     + avgSharedLat * -0.00000019990587062158
			                                     + commitCycles * -0.00027162839470335940
			                                     + memoryIndependentStallCycles * 0.00021251270939507934
			                                     + avgPrivateMemsysLat * 0.00000001618645760322
			                                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000084701836880
			                                     + sharedLLCMeasurements.accesses * -0.00000013552820840110
			                                     + privateLLCEstimates.hits * -0.00002039687654448508
			                                     + sharedLLCMeasurements.hits * 0.00000375140544928500
			                                     + 0.8905270967457687 ;
			                                 }
			                             } else {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 6.14883688954695450946
			                                 + privateLLCEstimates.writebacks * -0.00000697554956338138
			                                 + privateStallCycles * -0.00000105677115766805
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000890936772588
			                                 + avgSharedLat * 0.00007085779145681003
			                                 + commitCycles * -0.00000269453516119262
			                                 + memoryIndependentStallCycles * 0.00000063602956138913
			                                 + avgPrivateMemsysLat * -0.00259023437352378554
			                                 + comInstModelTraceCummulativeInst[cpuID] * -0.00000000003397157821
			                                 + sharedLLCMeasurements.accesses * -0.00000285203243217421
			                                 + privateLLCEstimates.hits * -0.00001253043970695311
			                                 + sharedLLCMeasurements.hits * 0.00001278815274779684
			                                 + 0.2169570203507042 ;
			                             }
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -15.92043841561481976044
			                             + privateLLCEstimates.writebacks * -0.00000683824302031809
			                             + privateStallCycles * -0.00000187707069798885
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000909035255945
			                             + avgSharedLat * -0.00004790271433776604
			                             + commitCycles * 0.00000753392621118782
			                             + memoryIndependentStallCycles * 0.00000771264514648195
			                             + avgPrivateMemsysLat * 0.02833857470698406336
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000141229583685
			                             + sharedLLCMeasurements.accesses * 0.00000343099582987636
			                             + privateLLCEstimates.hits * 0.00000450613989119622
			                             + sharedLLCMeasurements.hits * -0.00000226759593015951
			                             + -0.9680334072138024 ;
			                         }
			                     }
			                 } else {
			                     if ( privateLLCEstimates.writebacks <= 4752.5 ) {
			                         if ( avgPrivateMemsysLat <= 34.14 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -32.58942700334159070508
			                             + privateLLCEstimates.writebacks * -0.00008306578235665431
			                             + privateStallCycles * -0.00001959738957887997
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000002158172164822
			                             + avgSharedLat * 0.00006978863153263458
			                             + commitCycles * 0.00002992970662650310
			                             + memoryIndependentStallCycles * 0.00004389296777132677
			                             + avgPrivateMemsysLat * -0.00566303995381057645
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000011339236874
			                             + sharedLLCMeasurements.accesses * -0.00005694815578261695
			                             + privateLLCEstimates.hits * 0.00002673524719071739
			                             + sharedLLCMeasurements.hits * 0.00002727460391523919
			                             + 1.0665169382206325 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -30.05776233157306620569
			                             + privateLLCEstimates.writebacks * -0.00006084869230519343
			                             + privateStallCycles * -0.00000296386002579132
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000002480533367359
			                             + avgSharedLat * 0.00003751130300558408
			                             + commitCycles * 0.00002400935915121683
			                             + memoryIndependentStallCycles * 0.00002730316778339836
			                             + avgPrivateMemsysLat * -0.69092863769734547130
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000027636542430
			                             + sharedLLCMeasurements.accesses * -0.00000620271888020836
			                             + privateLLCEstimates.hits * 0.00000997044076642115
			                             + sharedLLCMeasurements.hits * 0.00000072597204310306
			                             + 24.539549361082102 ;
			                         }
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 6.57078669476055754473
			                         + privateLLCEstimates.writebacks * -0.00002313449247403444
			                         + privateStallCycles * 0.00000080895300975723
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001666067944029
			                         + avgSharedLat * 0.00008504628349805385
			                         + commitCycles * -0.00000414577248594164
			                         + memoryIndependentStallCycles * 0.00001022417649131154
			                         + avgPrivateMemsysLat * -0.00286442110804163838
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000007807243738
			                         + sharedLLCMeasurements.accesses * 0.00001262613175011833
			                         + privateLLCEstimates.hits * -0.00001424605311857743
			                         + sharedLLCMeasurements.hits * -0.00000712050930505533
			                         + 0.4408257516047156 ;
			                     }
			                 }
			             } else {
			                 if ( sharedIPC <= 0.044 ) {
			                     aloneIPCEstimate = 0.0
			                     + sharedIPC * 7.48686680455214847996
			                     + privateLLCEstimates.writebacks * -0.00000536391625299057
			                     + privateStallCycles * -0.00000060621683491010
			                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000059639881096
			                     + avgSharedLat * 0.00000554924348000798
			                     + commitCycles * -0.00000195876777938719
			                     + memoryIndependentStallCycles * 0.00000351790327094048
			                     + avgPrivateMemsysLat * 0.00740396164673329249
			                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000023225430597
			                     + sharedLLCMeasurements.accesses * -0.00000661871364666899
			                     + privateLLCEstimates.hits * 0.00000575432865418213
			                     + sharedLLCMeasurements.hits * -0.00000005723881939648
			                     + -0.19600624560516988 ;
			                 } else {
			                     if ( avgPrivateMemsysLat <= 34.914 ) {
			                         if ( privateLLCEstimates.writebacks <= 128.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 26.86153265085807007040
			                             + privateLLCEstimates.writebacks * 0.00000965045934508894
			                             + privateStallCycles * 0.00000793761867056356
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000937110740300
			                             + avgSharedLat * -0.00005619079604642308
			                             + commitCycles * -0.00001967232790421835
			                             + memoryIndependentStallCycles * -0.00002386017521436233
			                             + avgPrivateMemsysLat * 0.07924031779865443881
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000039771002251
			                             + sharedLLCMeasurements.accesses * -0.00001483937073686090
			                             + privateLLCEstimates.hits * 0.00000865807491833840
			                             + sharedLLCMeasurements.hits * 0.00001138358357467626
			                             + -1.2598747162520945 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 1.13026524355496671426
			                             + privateLLCEstimates.writebacks * -0.00000914725058750164
			                             + privateStallCycles * -0.00000284298006128302
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000938676019186
			                             + avgSharedLat * 0.00001640189550711577
			                             + commitCycles * -0.00000034368826323266
			                             + memoryIndependentStallCycles * -0.00000028647589510720
			                             + avgPrivateMemsysLat * -0.04197267764775082510
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000037627363991
			                             + sharedLLCMeasurements.accesses * -0.00000004479529297424
			                             + privateLLCEstimates.hits * 0.00000869985699693765
			                             + sharedLLCMeasurements.hits * -0.00000124270476709196
			                             + 1.877169752410697 ;
			                         }
			                     } else {
			                         if ( memoryIndependentStallCycles <= 43.0 ) {
			                             if ( privateLLCEstimates.writebacks <= 1009.5 ) {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * -3.48289221613651811538
			                                 + privateLLCEstimates.writebacks * -0.00021094982351157500
			                                 + privateStallCycles * 0.00000431079391634979
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000685345902858
			                                 + avgSharedLat * -0.00044973666783171441
			                                 + commitCycles * -0.00000245227227922576
			                                 + memoryIndependentStallCycles * -0.01677619477292462483
			                                 + avgPrivateMemsysLat * -0.02778030159401715174
			                                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000062913984656
			                                 + sharedLLCMeasurements.accesses * 0.00005893075590382118
			                                 + privateLLCEstimates.hits * 0.00000805602561426774
			                                 + sharedLLCMeasurements.hits * -0.00013738047242365243
			                                 + 3.410627432912685 ;
			                             } else {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 12.65046194737425899746
			                                 + privateLLCEstimates.writebacks * -0.00001078454881505791
			                                 + privateStallCycles * -0.00000093536248216612
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000199190009484
			                                 + avgSharedLat * 0.00002067611413444191
			                                 + commitCycles * -0.00000954418603855511
			                                 + memoryIndependentStallCycles * -0.00047738950770396426
			                                 + avgPrivateMemsysLat * -0.02071807091307611981
			                                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000008934668331
			                                 + sharedLLCMeasurements.accesses * 0.00000335268790509929
			                                 + privateLLCEstimates.hits * 0.00000105929477950278
			                                 + sharedLLCMeasurements.hits * -0.00000278594588762326
			                                 + 0.965131040367375 ;
			                             }
			                         } else {
			                             if ( reqs <= 16479.0 ) {
			                                 if ( reqs*(avgSharedLat+avgPrivateMemsysLat) <= 9883317.0 ) {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * 2.43857407201680675968
			                                     + privateLLCEstimates.writebacks * -0.00001572336205674392
			                                     + privateStallCycles * 0.00000206271629863271
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000376812982028
			                                     + avgSharedLat * -0.00003318351440696452
			                                     + commitCycles * -0.00000020987747683688
			                                     + memoryIndependentStallCycles * -0.00000029225501756959
			                                     + avgPrivateMemsysLat * 0.00464377828519254091
			                                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000009649572306
			                                     + sharedLLCMeasurements.accesses * -0.00002599833017831111
			                                     + privateLLCEstimates.hits * 0.00002788669252421866
			                                     + sharedLLCMeasurements.hits * 0.00001407311892765856
			                                     + 0.18623749292261177 ;
			                                 } else {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * 5.23129667323576619253
			                                     + privateLLCEstimates.writebacks * -0.00001011536094279845
			                                     + privateStallCycles * 0.00000053617186125849
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000227091039112
			                                     + avgSharedLat * 0.00001097629943982668
			                                     + commitCycles * -0.00000346281930203948
			                                     + memoryIndependentStallCycles * 0.00000928734773369951
			                                     + avgPrivateMemsysLat * -0.00311857668516343084
			                                     + comInstModelTraceCummulativeInst[cpuID] * -0.00000000004347231057
			                                     + sharedLLCMeasurements.accesses * -0.00000378082520689040
			                                     + privateLLCEstimates.hits * 0.00001641170621447456
			                                     + sharedLLCMeasurements.hits * -0.00000474628700585518
			                                     + 0.2845950603960523 ;
			                                 }
			                             } else {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 7.31460643402463173146
			                                 + privateLLCEstimates.writebacks * -0.00000775769365227606
			                                 + privateStallCycles * 0.00000040448433374771
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000231668238683
			                                 + avgSharedLat * 0.00001055870622699233
			                                 + commitCycles * -0.00000354127171473881
			                                 + memoryIndependentStallCycles * 0.00000480416272582578
			                                 + avgPrivateMemsysLat * 0.02334550848557867195
			                                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000008865397440
			                                 + sharedLLCMeasurements.accesses * -0.00000202639100605749
			                                 + privateLLCEstimates.hits * 0.00000763471235290924
			                                 + sharedLLCMeasurements.hits * -0.00000471829227496536
			                                 + -0.7463521181020247 ;
			                             }
			                         }
			                     }
			                 }
			             }
			         } else {
			             if ( privateLLCEstimates.writebacks <= 1792.0 ) {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 15.22608159036144748200
			                 + privateLLCEstimates.writebacks * -0.00003165389878963858
			                 + privateStallCycles * 0.00000242750767612715
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000001331452201017
			                 + avgSharedLat * -0.00009652144434897658
			                 + commitCycles * -0.00001149287367690315
			                 + memoryIndependentStallCycles * -0.00000633640524926112
			                 + avgPrivateMemsysLat * 0.00024382718017727911
			                 + comInstModelTraceCummulativeInst[cpuID] * -0.00000000000080088070
			                 + sharedLLCMeasurements.accesses * -0.00001672917168418359
			                 + privateLLCEstimates.hits * 0.00000416324010158601
			                 + sharedLLCMeasurements.hits * 0.00000908960013436283
			                 + 1.3751839640242571 ;
			             } else {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 4.63842323095066078054
			                 + privateLLCEstimates.writebacks * -0.00000491955433472024
			                 + privateStallCycles * 0.00000063579894771534
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000368590772114
			                 + avgSharedLat * -0.00001696319887291796
			                 + commitCycles * -0.00000390926762204837
			                 + memoryIndependentStallCycles * 0.00000250317142527844
			                 + avgPrivateMemsysLat * -0.00469263300469139645
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000425834597
			                 + sharedLLCMeasurements.accesses * 0.00000138058532242088
			                 + privateLLCEstimates.hits * 0.00000669863559729391
			                 + sharedLLCMeasurements.hits * -0.00000365320323815043
			                 + 0.3303744485708626 ;
			             }
			         }
			     } else {
			         if ( privateLLCEstimates.writebacks <= 8.5 ) {
			             if ( privateLLCEstimates.hits <= 5248.0 ) {
			                 if ( privateLLCEstimates.accesses <= 5504.0 ) {
			                     if ( numStores <= 295.0 ) {
			                         if ( privateLLCEstimates.hits <= 1152.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 12.70350451790384305184
			                             + privateLLCEstimates.writebacks * 0.00001187478455165803
			                             + privateStallCycles * 0.00000156881744136956
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000453294590328
			                             + avgSharedLat * 0.00004807820057966668
			                             + commitCycles * -0.00000995760341162034
			                             + memoryIndependentStallCycles * 0.00001753898902414488
			                             + avgPrivateMemsysLat * 0.00737343454997731879
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000017511935546
			                             + sharedLLCMeasurements.accesses * -0.00001619832546665041
			                             + privateLLCEstimates.hits * 0.00000624512991084757
			                             + sharedLLCMeasurements.hits * 0.00000413903128933591
			                             + 0.05025928887703024 ;
			                         } else {
			                             if ( emptyROBStallCycles <= 17955.5 ) {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 3.08725017852400940299
			                                 + privateLLCEstimates.writebacks * -0.00000746083428156663
			                                 + privateStallCycles * -0.00000238293068927560
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000001686341064548
			                                 + avgSharedLat * 0.00001885934090995648
			                                 + commitCycles * 0.00000075208130425336
			                                 + memoryIndependentStallCycles * -0.00000247702691041972
			                                 + avgPrivateMemsysLat * -0.03236417220912268800
			                                 + comInstModelTraceCummulativeInst[cpuID] * -0.00000000002523511257
			                                 + sharedLLCMeasurements.accesses * -0.00005686091839699960
			                                 + privateLLCEstimates.hits * 0.00001754370280572281
			                                 + sharedLLCMeasurements.hits * -0.00002026092554407660
			                                 + 2.0241816628750033 ;
			                             } else {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 11.39977088928745452279
			                                 + privateLLCEstimates.writebacks * 0.00001679574310578691
			                                 + privateStallCycles * -0.00000029646529402201
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000830584733067
			                                 + avgSharedLat * 0.00015311061996263294
			                                 + commitCycles * -0.00000335649568850108
			                                 + memoryIndependentStallCycles * -0.00000433687324270374
			                                 + avgPrivateMemsysLat * 0.00143084410826613837
			                                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000374073438
			                                 + sharedLLCMeasurements.accesses * 0.00003430343122493347
			                                 + privateLLCEstimates.hits * 0.00001543266066316383
			                                 + sharedLLCMeasurements.hits * 0.00009844327189671072
			                                 + -0.0650620232218585 ;
			                             }
			                         }
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 25.47785113624887642914
			                         + privateLLCEstimates.writebacks * 0.01287491052788742457
			                         + privateStallCycles * 0.00000265880527139156
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000305502538545
			                         + avgSharedLat * 0.00003778085139919653
			                         + commitCycles * -0.00001358592458341546
			                         + memoryIndependentStallCycles * -0.00000021287912054159
			                         + avgPrivateMemsysLat * -0.07462480802123143409
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000010927553046
			                         + sharedLLCMeasurements.accesses * -0.00005794898188997125
			                         + privateLLCEstimates.hits * 0.00007236906681075521
			                         + sharedLLCMeasurements.hits * 0.00000797264492805739
			                         + 2.9486798496806688 ;
			                     }
			                 } else {
			                     if ( avgPrivateMemsysLat <= 35.954 ) {
			                         if ( avgPrivateMemsysLat <= 34.111 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 3.39164938051544195829
			                             + privateLLCEstimates.writebacks * -0.00000285302445662043
			                             + privateStallCycles * 0.00000466901772713729
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000008113758608521
			                             + avgSharedLat * 0.00026538444366234018
			                             + commitCycles * -0.00000012648722311667
			                             + memoryIndependentStallCycles * -0.00000001875217343858
			                             + avgPrivateMemsysLat * 0.13600299711534941105
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000701393089320
			                             + sharedLLCMeasurements.accesses * -0.00000731277177796760
			                             + privateLLCEstimates.hits * 0.00001332497449249681
			                             + sharedLLCMeasurements.hits * -0.00002528937234481122
			                             + -4.218633635119465 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 3.27113777898743629535
			                             + privateLLCEstimates.writebacks * -0.00000000043209736309
			                             + privateStallCycles * -0.00000000857502204081
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000397240494390
			                             + avgSharedLat * 0.00004467178580265184
			                             + commitCycles * -0.00000136504402265389
			                             + memoryIndependentStallCycles * -0.00000075157198525641
			                             + avgPrivateMemsysLat * 0.05547314047690054400
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000022772116411
			                             + sharedLLCMeasurements.accesses * -0.00000239188271303093
			                             + privateLLCEstimates.hits * 0.00004664278511771123
			                             + sharedLLCMeasurements.hits * -0.00004994583259879037
			                             + -1.548587899193142 ;
			                         }
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 1.72277455472003948600
			                         + privateLLCEstimates.writebacks * 0.00000022139438759212
			                         + privateStallCycles * -0.00000077633376086602
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000397260883260
			                         + avgSharedLat * 0.00003522810815946788
			                         + commitCycles * 0.00000037569897767392
			                         + memoryIndependentStallCycles * -0.00000027668322003981
			                         + avgPrivateMemsysLat * -0.00580232895099290057
			                         + comInstModelTraceCummulativeInst[cpuID] * -0.00000000003117593753
			                         + sharedLLCMeasurements.accesses * -0.00001284382803866232
			                         + privateLLCEstimates.hits * 0.00000433236853468806
			                         + sharedLLCMeasurements.hits * 0.00001685051535506727
			                         + 0.522588850604226 ;
			                     }
			                 }
			             } else {
			                 if ( sharedLLCMeasurements.hits <= 5002.5 ) {
			                     if ( sharedIPC <= 0.068 ) {
			                         if ( numStores <= 169.5 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 10.37125223331039336472
			                             + privateLLCEstimates.writebacks * -0.00008111131266078606
			                             + privateStallCycles * 0.00000518669908050810
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000001480451711246
			                             + avgSharedLat * -0.00000615232254701025
			                             + commitCycles * -0.00000673112341887423
			                             + memoryIndependentStallCycles * 0.00000205536014159664
			                             + avgPrivateMemsysLat * -0.05133840346915896635
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000000057306074
			                             + sharedLLCMeasurements.accesses * -0.00009121570988581562
			                             + privateLLCEstimates.hits * 0.00005898753852401575
			                             + sharedLLCMeasurements.hits * 0.00011449847402464965
			                             + 2.4750329922627765 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -22.67567841061140043735
			                             + privateLLCEstimates.writebacks * 0.00000433345089672837
			                             + privateStallCycles * 0.00000057891501792797
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000002215632264585
			                             + avgSharedLat * -0.00014477110423656255
			                             + commitCycles * 0.00001022574872989488
			                             + memoryIndependentStallCycles * -0.00000007831787834473
			                             + avgPrivateMemsysLat * -0.00223396210944163450
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000104560974661
			                             + sharedLLCMeasurements.accesses * -0.00003057536679075651
			                             + privateLLCEstimates.hits * 0.00001464172643502418
			                             + sharedLLCMeasurements.hits * 0.00001213742275046390
			                             + 0.9497229526703412 ;
			                         }
			                     } else {
			                         if ( comInstModelTraceCummulativeInst[cpuID] <= 9450266.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 16.08762914075800409819
			                             + privateLLCEstimates.writebacks * 0.00000026286301504562
			                             + privateStallCycles * 0.00000007690372749596
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000006038813496344
			                             + avgSharedLat * -0.00008051513071956370
			                             + commitCycles * -0.00000895031192951842
			                             + memoryIndependentStallCycles * -0.00000021404126518906
			                             + avgPrivateMemsysLat * 0.01956187354310316706
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000001335259502985
			                             + sharedLLCMeasurements.accesses * -0.00006165629154114006
			                             + privateLLCEstimates.hits * -0.00000055037539751923
			                             + sharedLLCMeasurements.hits * 0.00002029185293827375
			                             + 1.0082825267815925 ;
			                         } else {
			                             if ( memoryIndependentStallCycles <= 159933.0 ) {
			                                 if ( privateStallCycles <= 28849.5 ) {
			                                     if ( numStores <= 1.5 ) {
			                                         aloneIPCEstimate = 0.0
			                                         + sharedIPC * -26.99583970062311522042
			                                         + privateLLCEstimates.writebacks * -0.00008980347400304095
			                                         + privateStallCycles * -0.00000824766055329567
			                                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001972325366033
			                                         + avgSharedLat * 0.00010255039246853728
			                                         + commitCycles * 0.00002660091188904793
			                                         + memoryIndependentStallCycles * -0.00002661097675931120
			                                         + avgPrivateMemsysLat * 0.03172725387885037829
			                                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000203182240
			                                         + sharedLLCMeasurements.accesses * -0.00004588698683884006
			                                         + privateLLCEstimates.hits * 0.00004314935337144086
			                                         + sharedLLCMeasurements.hits * -0.00001597678818907677
			                                         + -0.18723654554578428 ;
			                                     } else {
			                                         aloneIPCEstimate = 0.0
			                                         + sharedIPC * -2.31506429481724440578
			                                         + privateLLCEstimates.writebacks * 0.00653567322872535862
			                                         + privateStallCycles * -0.00000615033453467013
			                                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000433723843234
			                                         + avgSharedLat * 0.00002535283066677867
			                                         + commitCycles * 0.00000221149350606114
			                                         + memoryIndependentStallCycles * -0.00000007423490947583
			                                         + avgPrivateMemsysLat * -0.03220202953837159737
			                                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000599698329
			                                         + sharedLLCMeasurements.accesses * -0.00003715515136281241
			                                         + privateLLCEstimates.hits * 0.00003261996041886477
			                                         + sharedLLCMeasurements.hits * -0.00000056909704230905
			                                         + 2.378713648232151 ;
			                                     }
			                                 } else {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * 8.57775816777497190913
			                                     + privateLLCEstimates.writebacks * 0.00000197381567930671
			                                     + privateStallCycles * -0.00000017946064338855
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000377667785540
			                                     + avgSharedLat * 0.00014313157336804289
			                                     + commitCycles * -0.00000367933606462533
			                                     + memoryIndependentStallCycles * -0.00000155893992008147
			                                     + avgPrivateMemsysLat * -0.01457425715565393090
			                                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000001640517043
			                                     + sharedLLCMeasurements.accesses * -0.00002930282882289105
			                                     + privateLLCEstimates.hits * 0.00002170528930395208
			                                     + sharedLLCMeasurements.hits * 0.00000190470251550513
			                                     + 1.0527347886638485 ;
			                                 }
			                             } else {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * -21.55658579930236129485
			                                 + privateLLCEstimates.writebacks * 0.00013307447303540824
			                                 + privateStallCycles * 0.00000157978178616699
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000004620626252019
			                                 + avgSharedLat * 0.00005375454025749161
			                                 + commitCycles * 0.00001475543281085501
			                                 + memoryIndependentStallCycles * 0.00000003404591636893
			                                 + avgPrivateMemsysLat * -0.15036321109334516155
			                                 + comInstModelTraceCummulativeInst[cpuID] * -0.00000000010007189334
			                                 + sharedLLCMeasurements.accesses * -0.00003524480760615154
			                                 + privateLLCEstimates.hits * 0.00002188869523677282
			                                 + sharedLLCMeasurements.hits * 0.00001090499312670260
			                                 + 5.163379045435253 ;
			                             }
			                         }
			                     }
			                 } else {
			                     if ( comInstModelTraceCummulativeInst[cpuID] <= 107268392.0 ) {
			                         if ( emptyROBStallCycles <= 12162.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 8.44470834763140132395
			                             + privateLLCEstimates.writebacks * -0.00000102316232956315
			                             + privateStallCycles * 0.00000080655639504455
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000199756065354
			                             + avgSharedLat * 0.00003494947996998766
			                             + commitCycles * -0.00000453392912259251
			                             + memoryIndependentStallCycles * 0.00000092139119146936
			                             + avgPrivateMemsysLat * 0.00515175130469137411
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000004353133769
			                             + sharedLLCMeasurements.accesses * -0.00001330231814962269
			                             + privateLLCEstimates.hits * 0.00001717421746299370
			                             + sharedLLCMeasurements.hits * -0.00000299056206123009
			                             + 0.2333898305282091 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 12.31114940188443718228
			                             + privateLLCEstimates.writebacks * 0.00000177299607616681
			                             + privateStallCycles * 0.00000054697999013367
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000360684789084
			                             + avgSharedLat * -0.00011207869072122496
			                             + commitCycles * -0.00000612201099567898
			                             + memoryIndependentStallCycles * 0.00000037924005114528
			                             + avgPrivateMemsysLat * 0.00874171378964274520
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000032527627635
			                             + sharedLLCMeasurements.accesses * -0.00001676231039005411
			                             + privateLLCEstimates.hits * 0.00001915633846256286
			                             + sharedLLCMeasurements.hits * -0.00000174604334102567
			                             + 0.024042423908268717 ;
			                         }
			                     } else {
			                         if ( avgPrivateMemsysLat <= 37.226 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 4.69894883628212589599
			                             + privateLLCEstimates.writebacks * 0.00000232390366412343
			                             + privateStallCycles * -0.00000138565246145255
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000292942072055
			                             + avgSharedLat * 0.00013097731086363974
			                             + commitCycles * -0.00000074721728037033
			                             + memoryIndependentStallCycles * -0.00000563867270113030
			                             + avgPrivateMemsysLat * 0.08812272887185698678
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000000118586057
			                             + sharedLLCMeasurements.accesses * -0.00000517241582304711
			                             + privateLLCEstimates.hits * 0.00000299810310870963
			                             + sharedLLCMeasurements.hits * -0.00000174801832089417
			                             + -2.296526139309252 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 10.00480490051903004201
			                             + privateLLCEstimates.writebacks * -0.00000435725660786727
			                             + privateStallCycles * 0.00000009355097871719
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000073080320982
			                             + avgSharedLat * 0.00000590080585906231
			                             + commitCycles * -0.00000653991674618379
			                             + memoryIndependentStallCycles * 0.00000336709937809380
			                             + avgPrivateMemsysLat * -0.01276142550824944412
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000000385236527
			                             + sharedLLCMeasurements.accesses * -0.00000429694043431480
			                             + privateLLCEstimates.hits * 0.00000117524558786195
			                             + sharedLLCMeasurements.hits * 0.00000264354555468408
			                             + 1.3363776016882674 ;
			                         }
			                     }
			                 }
			             }
			         } else {
			             if ( sharedLLCMeasurements.accesses <= 7212.5 ) {
			                 if ( avgPrivateMemsysLat <= 34.559 ) {
			                     if ( avgPrivateMemsysLat <= 34.07 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 0.96696212985076090973
			                         + privateLLCEstimates.writebacks * -0.00007599788872256591
			                         + privateStallCycles * -0.00000090871503702834
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000949123939754
			                         + avgSharedLat * 0.00001232546196929706
			                         + commitCycles * 0.00000046694899585231
			                         + memoryIndependentStallCycles * -0.00000024781675941605
			                         + avgPrivateMemsysLat * 0.08781572728125627836
			                         + comInstModelTraceCummulativeInst[cpuID] * -0.00000000009429005379
			                         + sharedLLCMeasurements.accesses * -0.00003553995506189044
			                         + privateLLCEstimates.hits * 0.00002589085072218675
			                         + sharedLLCMeasurements.hits * -0.00001708687649988021
			                         + -2.0690575048779722 ;
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 3.03677406186814069500
			                         + privateLLCEstimates.writebacks * -0.00003354074919132919
			                         + privateStallCycles * 0.00000043906159315923
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001525953321783
			                         + avgSharedLat * 0.00002411136183275210
			                         + commitCycles * -0.00000122243622555677
			                         + memoryIndependentStallCycles * 0.00000066482376377126
			                         + avgPrivateMemsysLat * -0.33825851956223174088
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000002383775496
			                         + sharedLLCMeasurements.accesses * -0.00004543119939815718
			                         + privateLLCEstimates.hits * 0.00003299991186518605
			                         + sharedLLCMeasurements.hits * 0.00002730190596037374
			                         + 12.348102379601041 ;
			                     }
			                 } else {
			                     aloneIPCEstimate = 0.0
			                     + sharedIPC * 5.22768989844751530427
			                     + privateLLCEstimates.writebacks * -0.00005210637715948315
			                     + privateStallCycles * 0.00000028021473418524
			                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000168547813440
			                     + avgSharedLat * 0.00004137329446268026
			                     + commitCycles * -0.00000140185002848144
			                     + memoryIndependentStallCycles * -0.00000010614658398405
			                     + avgPrivateMemsysLat * -0.03844915765825111104
			                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000021317447247
			                     + sharedLLCMeasurements.accesses * -0.00001819596883118789
			                     + privateLLCEstimates.hits * 0.00003082650522551001
			                     + sharedLLCMeasurements.hits * -0.00000889448708795644
			                     + 1.5821870103034588 ;
			                 }
			             } else {
			                 if ( reqs*(avgSharedLat+avgPrivateMemsysLat) <= 17806292.0 ) {
			                     if ( memoryIndependentStallCycles <= 26153.0 ) {
			                         if ( avgPrivateMemsysLat <= 35.562 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 9.77326096057773163750
			                             + privateLLCEstimates.writebacks * -0.00002028146991535737
			                             + privateStallCycles * 0.00000451332576749429
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000149939006655
			                             + avgSharedLat * 0.00005356227473835965
			                             + commitCycles * -0.00000562392537827461
			                             + memoryIndependentStallCycles * -0.00000083336731522497
			                             + avgPrivateMemsysLat * -0.00553474153271075325
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000012920650273
			                             + sharedLLCMeasurements.accesses * -0.00000282463334006308
			                             + privateLLCEstimates.hits * 0.00000187624759582010
			                             + sharedLLCMeasurements.hits * -0.00000349514658901934
			                             + 0.3940912998665384 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -2.73068481340508295574
			                             + privateLLCEstimates.writebacks * -0.00001298263500793447
			                             + privateStallCycles * 0.00000081225968376890
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000510300891540
			                             + avgSharedLat * 0.00003161001422950971
			                             + commitCycles * 0.00000286636270557038
			                             + memoryIndependentStallCycles * -0.00000068027729260470
			                             + avgPrivateMemsysLat * 0.00193562648502779589
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000002352614556
			                             + sharedLLCMeasurements.accesses * -0.00000028532169903695
			                             + privateLLCEstimates.hits * 0.00000726635226904165
			                             + sharedLLCMeasurements.hits * -0.00000628085513552765
			                             + 0.15021047343849048 ;
			                         }
			                     } else {
			                         if ( avgPrivateMemsysLat <= 34.218 ) {
			                             if ( reqs*(avgSharedLat+avgPrivateMemsysLat) <= 5144828.0 ) {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 14.58442834369535923145
			                                 + privateLLCEstimates.writebacks * -0.00001298466233324045
			                                 + privateStallCycles * 0.00000124032995796478
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000010689229742210
			                                 + avgSharedLat * -0.00017572179645713830
			                                 + commitCycles * -0.00001165779020648718
			                                 + memoryIndependentStallCycles * 0.00000228961395959400
			                                 + avgPrivateMemsysLat * 0.05758010360882282602
			                                 + comInstModelTraceCummulativeInst[cpuID] * -0.00000000007681406707
			                                 + sharedLLCMeasurements.accesses * -0.00000196472432328629
			                                 + privateLLCEstimates.hits * -0.00001047463097099751
			                                 + sharedLLCMeasurements.hits * -0.00001463242494263658
			                                 + 0.03289576714881559 ;
			                             } else {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 7.66792530052164522658
			                                 + privateLLCEstimates.writebacks * -0.00004492941566892246
			                                 + privateStallCycles * 0.00000166359304432597
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001270450706777
			                                 + avgSharedLat * 0.00001821918928049806
			                                 + commitCycles * -0.00000409794369685941
			                                 + memoryIndependentStallCycles * 0.00000060020376068787
			                                 + avgPrivateMemsysLat * 0.61743859703731207578
			                                 + comInstModelTraceCummulativeInst[cpuID] * -0.00000000018148647105
			                                 + sharedLLCMeasurements.accesses * -0.00001377806875934855
			                                 + privateLLCEstimates.hits * 0.00001393024492385829
			                                 + sharedLLCMeasurements.hits * -0.00000314512151788073
			                                 + -20.464681479567922 ;
			                             }
			                         } else {
			                             if ( numStores <= 819.0 ) {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 4.29106960192381592378
			                                 + privateLLCEstimates.writebacks * -0.00002000231073065358
			                                 + privateStallCycles * 0.00000094267802013772
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000062295205770
			                                 + avgSharedLat * 0.00000980357042313211
			                                 + commitCycles * -0.00000198047998485788
			                                 + memoryIndependentStallCycles * 0.00000011737454615052
			                                 + avgPrivateMemsysLat * -0.03519795907152480779
			                                 + comInstModelTraceCummulativeInst[cpuID] * -0.00000000006673016972
			                                 + sharedLLCMeasurements.accesses * -0.00000340165484273122
			                                 + privateLLCEstimates.hits * 0.00001278831329261185
			                                 + sharedLLCMeasurements.hits * -0.00001242438840302813
			                                 + 1.476584894018103 ;
			                             } else {
			                                 if ( avgPrivateMemsysLat <= 35.116 ) {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * 7.00573248414881355473
			                                     + privateLLCEstimates.writebacks * -0.00001842325877338151
			                                     + privateStallCycles * -0.00000059822186130632
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000527184556094
			                                     + avgSharedLat * 0.00004989443082535337
			                                     + commitCycles * -0.00000314716578826309
			                                     + memoryIndependentStallCycles * -0.00000018371094718030
			                                     + avgPrivateMemsysLat * -0.08838457193003122769
			                                     + comInstModelTraceCummulativeInst[cpuID] * -0.00000000002142034069
			                                     + sharedLLCMeasurements.accesses * -0.00001828760029099059
			                                     + privateLLCEstimates.hits * 0.00003712221612108906
			                                     + sharedLLCMeasurements.hits * -0.00001135861085399124
			                                     + 3.464471946555044 ;
			                                 } else {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * 2.13287283099874436232
			                                     + privateLLCEstimates.writebacks * -0.00001370562329672662
			                                     + privateStallCycles * -0.00000061122249154743
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000604417603338
			                                     + avgSharedLat * 0.00005033744166704483
			                                     + commitCycles * -0.00000043546374280782
			                                     + memoryIndependentStallCycles * 0.00000004100687609785
			                                     + avgPrivateMemsysLat * -0.00302481120552759979
			                                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000029011368
			                                     + sharedLLCMeasurements.accesses * -0.00000367473712189563
			                                     + privateLLCEstimates.hits * 0.00000532490354131816
			                                     + sharedLLCMeasurements.hits * -0.00000164213553759492
			                                     + 0.4312426242274928 ;
			                                 }
			                             }
			                         }
			                     }
			                 } else {
			                     if ( sharedIPC <= 0.12 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 4.98528694027139529510
			                         + privateLLCEstimates.writebacks * -0.00000585719667800199
			                         + privateStallCycles * 0.00000020057508318771
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000068892780901
			                         + avgSharedLat * -0.00001811862004937366
			                         + commitCycles * -0.00000258681215597420
			                         + memoryIndependentStallCycles * 0.00000147746338469802
			                         + avgPrivateMemsysLat * 0.00212452221844375982
			                         + comInstModelTraceCummulativeInst[cpuID] * -0.00000000001916513528
			                         + sharedLLCMeasurements.accesses * -0.00000562340822517259
			                         + privateLLCEstimates.hits * 0.00000283418049519345
			                         + sharedLLCMeasurements.hits * 0.00000429361524664518
			                         + 0.13787944961425264 ;
			                     } else {
			                         if ( numStores <= 35.5 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -6.20121639990928397168
			                             + privateLLCEstimates.writebacks * -0.00002016402706748889
			                             + privateStallCycles * -0.00000082387489540878
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001052439787208
			                             + avgSharedLat * -0.00010824668422893392
			                             + commitCycles * 0.00000417643847286999
			                             + memoryIndependentStallCycles * -0.00000097707149305252
			                             + avgPrivateMemsysLat * 0.00102301546670548107
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000008274788322
			                             + sharedLLCMeasurements.accesses * 0.00000503772369594282
			                             + privateLLCEstimates.hits * 0.00000275100509651104
			                             + sharedLLCMeasurements.hits * -0.00000959442963155493
			                             + 1.0672811510056093 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 0.29908449051297320009
			                             + privateLLCEstimates.writebacks * -0.00000473032218206586
			                             + privateStallCycles * -0.00000052852398524964
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000363167961629
			                             + avgSharedLat * 0.00004099922881971883
			                             + commitCycles * 0.00000108277575069668
			                             + memoryIndependentStallCycles * -0.00000058454155951791
			                             + avgPrivateMemsysLat * -0.00523953791464627609
			                             + comInstModelTraceCummulativeInst[cpuID] * -0.00000000003233186440
			                             + sharedLLCMeasurements.accesses * -0.00000687040579369645
			                             + privateLLCEstimates.hits * -0.00000002563215697968
			                             + sharedLLCMeasurements.hits * 0.00000564747956137500
			                             + 0.4159513298951719 ;
			                         }
			                     }
			                 }
			             }
			         }
			     }
			 } else {
			     if ( sharedIPC <= 0.916 ) {
			         if ( privateLLCEstimates.writebacks <= 2561.5 ) {
			             if ( emptyROBStallCycles <= 88927.5 ) {
			                 if ( avgPrivateMemsysLat <= 38.403 ) {
			                     if ( privateLLCEstimates.writebacks <= 510.0 ) {
			                         if ( reqs <= 657.0 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -0.58786798852177868380
			                             + privateLLCEstimates.writebacks * -0.00022384627096680269
			                             + privateStallCycles * 0.00000011899876534011
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000006808962293784
			                             + avgSharedLat * 0.00000359636567269729
			                             + commitCycles * 0.00000013095979819093
			                             + memoryIndependentStallCycles * 0.00000017897331063252
			                             + avgPrivateMemsysLat * 0.00755719783551861857
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000188402093
			                             + sharedLLCMeasurements.accesses * -0.00001559594818287600
			                             + privateLLCEstimates.hits * 0.00001822285869401708
			                             + sharedLLCMeasurements.hits * 0.00000251979139420487
			                             + 1.0955723306767895 ;
			                         } else {
			                             if ( comInstModelTraceCummulativeInst[cpuID] <= 6028275.0 ) {
			                                 if ( avgPrivateMemsysLat <= 34.191 ) {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * -1.87282021032848255970
			                                     + privateLLCEstimates.writebacks * -0.00000000492611709940
			                                     + privateStallCycles * -0.00000086538157949716
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001722501717312
			                                     + avgSharedLat * 0.00006519679660757024
			                                     + commitCycles * 0.00000167818125878340
			                                     + memoryIndependentStallCycles * -0.00000022153011190704
			                                     + avgPrivateMemsysLat * -0.25585400805173874605
			                                     + comInstModelTraceCummulativeInst[cpuID] * -0.00000001084910195158
			                                     + sharedLLCMeasurements.accesses * -0.00001672029417135609
			                                     + privateLLCEstimates.hits * 0.00000693139884038323
			                                     + sharedLLCMeasurements.hits * 0.00002518169235164337
			                                     + 9.901033477646127 ;
			                                 } else {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * 1.80404575225752594747
			                                     + privateLLCEstimates.writebacks * -0.00012856060261744363
			                                     + privateStallCycles * 0.00000099850927283296
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001217931420883
			                                     + avgSharedLat * 0.00020151846867919442
			                                     + commitCycles * -0.00000083990557721263
			                                     + memoryIndependentStallCycles * -0.00000020379170908961
			                                     + avgPrivateMemsysLat * -0.04870635451254758108
			                                     + comInstModelTraceCummulativeInst[cpuID] * -0.00000003395644327268
			                                     + sharedLLCMeasurements.accesses * -0.00000082623871370130
			                                     + privateLLCEstimates.hits * -0.00007165057732766330
			                                     + sharedLLCMeasurements.hits * 0.00008140665451541027
			                                     + 2.1545390571295715 ;
			                                 }
			                             } else {
			                                 if ( comInstModelTraceCummulativeInst[cpuID] <= 136391256.0 ) {
			                                     if ( reqs*(avgSharedLat+avgPrivateMemsysLat) <= 44609800.0 ) {
			                                         aloneIPCEstimate = 0.0
			                                         + sharedIPC * 0.42597924063762193647
			                                         + privateLLCEstimates.writebacks * -0.00018158130716439328
			                                         + privateStallCycles * -0.00000017844968901862
			                                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000223364273196
			                                         + avgSharedLat * 0.00001623094974933628
			                                         + commitCycles * 0.00000002818191317188
			                                         + memoryIndependentStallCycles * -0.00000024476256272567
			                                         + avgPrivateMemsysLat * -0.06692200898063457859
			                                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000052333472012
			                                         + sharedLLCMeasurements.accesses * -0.00001410240012938453
			                                         + privateLLCEstimates.hits * 0.00000860690458506241
			                                         + sharedLLCMeasurements.hits * 0.00000511545044255295
			                                         + 3.4129927487885796 ;
			                                     } else {
			                                         aloneIPCEstimate = 0.0
			                                         + sharedIPC * 0.63744641321050388161
			                                         + privateLLCEstimates.writebacks * 0.00000102406855593581
			                                         + privateStallCycles * 0.00000033809403742748
			                                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000079520467475
			                                         + avgSharedLat * -0.00000149864599107104
			                                         + commitCycles * 0.00000041037569922601
			                                         + memoryIndependentStallCycles * -0.00000143527058032668
			                                         + avgPrivateMemsysLat * -0.01783959899723127143
			                                         + comInstModelTraceCummulativeInst[cpuID] * -0.00000000078500889868
			                                         + sharedLLCMeasurements.accesses * -0.00000270503705564544
			                                         + privateLLCEstimates.hits * 0.00000317601611160768
			                                         + sharedLLCMeasurements.hits * -0.00000113283338084029
			                                         + 1.1146446747572718 ;
			                                     }
			                                 } else {
			                                     if ( emptyROBStallCycles <= 19717.0 ) {
			                                         if ( privateLLCEstimates.hits <= 11904.0 ) {
			                                             aloneIPCEstimate = 0.0
			                                             + sharedIPC * 0.90842985818078014759
			                                             + privateLLCEstimates.writebacks * -0.00009151457477717698
			                                             + privateStallCycles * -0.00000013913598702405
			                                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000098451800764
			                                             + avgSharedLat * 0.00002350986031228299
			                                             + commitCycles * -0.00000019133894685924
			                                             + memoryIndependentStallCycles * -0.00000025044447879203
			                                             + avgPrivateMemsysLat * -0.03147102694364184439
			                                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000842522734
			                                             + sharedLLCMeasurements.accesses * -0.00002877014323080553
			                                             + privateLLCEstimates.hits * 0.00001841685066762127
			                                             + sharedLLCMeasurements.hits * 0.00000979065877091519
			                                             + 2.226186062922075 ;
			                                         } else {
			                                             aloneIPCEstimate = 0.0
			                                             + sharedIPC * 0.00068206031986089077
			                                             + privateLLCEstimates.writebacks * 0.00001910365058804671
			                                             + privateStallCycles * -0.00000031314240617876
			                                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000017951954828
			                                             + avgSharedLat * 0.00001733909842932383
			                                             + commitCycles * 0.00000032610712649692
			                                             + memoryIndependentStallCycles * -0.00000029197865065326
			                                             + avgPrivateMemsysLat * 0.00439260675597041243
			                                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000104918300
			                                             + sharedLLCMeasurements.accesses * 0.00000630668537396855
			                                             + privateLLCEstimates.hits * -0.00000556762301284749
			                                             + sharedLLCMeasurements.hits * -0.00000322558857724451
			                                             + 1.00594214035448 ;
			                                         }
			                                     } else {
			                                         aloneIPCEstimate = 0.0
			                                         + sharedIPC * 1.07071966589491185395
			                                         + privateLLCEstimates.writebacks * 0.00036912315534472147
			                                         + privateStallCycles * -0.00000000145776139420
			                                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000634262497821
			                                         + avgSharedLat * -0.00001193462914364554
			                                         + commitCycles * -0.00000039381308064454
			                                         + memoryIndependentStallCycles * -0.00000009161655827366
			                                         + avgPrivateMemsysLat * -0.01084901952352012276
			                                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000055008683
			                                         + sharedLLCMeasurements.accesses * -0.00000898011600381340
			                                         + privateLLCEstimates.hits * 0.00000192539616106306
			                                         + sharedLLCMeasurements.hits * 0.00000337830720312819
			                                         + 1.4459215355342496 ;
			                                     }
			                                 }
			                             }
			                         }
			                     } else {
			                         if ( stallCycles <= 3979735.5 ) {
			                             if ( avgPrivateMemsysLat <= 34.528 ) {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 0.45688858414208433079
			                                 + privateLLCEstimates.writebacks * -0.00004242991744559341
			                                 + privateStallCycles * 0.00000010378368637459
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000272501697216
			                                 + avgSharedLat * 0.00006581258747238122
			                                 + commitCycles * -0.00000016064234190112
			                                 + memoryIndependentStallCycles * 0.00000011517391634866
			                                 + avgPrivateMemsysLat * -0.09940261834056750290
			                                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000547668319
			                                 + sharedLLCMeasurements.accesses * -0.00000695111891431658
			                                 + privateLLCEstimates.hits * 0.00001862267071341143
			                                 + sharedLLCMeasurements.hits * -0.00000979332700494020
			                                 + 4.256845003738314 ;
			                             } else {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 0.72809868884145712009
			                                 + privateLLCEstimates.writebacks * -0.00000105483653085476
			                                 + privateStallCycles * 0.00000028376215528733
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001765128044772
			                                 + avgSharedLat * 0.00003415147198430169
			                                 + commitCycles * -0.00000044516511674681
			                                 + memoryIndependentStallCycles * 0.00000007366875578157
			                                 + avgPrivateMemsysLat * -0.05293972423926007059
			                                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000003093729612
			                                 + sharedLLCMeasurements.accesses * -0.00000488585629264604
			                                 + privateLLCEstimates.hits * 0.00000614076107607778
			                                 + sharedLLCMeasurements.hits * -0.00000433994722749055
			                                 + 3.0298720710448404 ;
			                             }
			                         } else {
			                             if ( reqs <= 10762.0 ) {
			                                 if ( emptyROBStallCycles <= 38.5 ) {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * 2.49207492622940529614
			                                     + privateLLCEstimates.writebacks * -0.00003299027958029858
			                                     + privateStallCycles * 0.00000143200125761880
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000001275257583793
			                                     + avgSharedLat * 0.00002365728819980053
			                                     + commitCycles * -0.00000165225735225771
			                                     + memoryIndependentStallCycles * -0.00000000001471289468
			                                     + avgPrivateMemsysLat * -0.07765822427785186444
			                                     + comInstModelTraceCummulativeInst[cpuID] * -0.00000000003854667676
			                                     + sharedLLCMeasurements.accesses * -0.00002027628377213570
			                                     + privateLLCEstimates.hits * 0.00002377804885872955
			                                     + sharedLLCMeasurements.hits * 0.00001973998040256618
			                                     + 3.54138880599635 ;
			                                 } else {
			                                     aloneIPCEstimate = 0.0
			                                     + sharedIPC * -0.88581493410816303236
			                                     + privateLLCEstimates.writebacks * -0.00005729290890365334
			                                     + privateStallCycles * -0.00000098200134903446
			                                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000491232265275
			                                     + avgSharedLat * 0.00003772959284022793
			                                     + commitCycles * 0.00000130107649688347
			                                     + memoryIndependentStallCycles * -0.00000028818058377166
			                                     + avgPrivateMemsysLat * 0.05121070225151053146
			                                     + comInstModelTraceCummulativeInst[cpuID] * -0.00000000005344617159
			                                     + sharedLLCMeasurements.accesses * -0.00002923839070058927
			                                     + privateLLCEstimates.hits * 0.00003263665908680439
			                                     + sharedLLCMeasurements.hits * 0.00000025857596461154
			                                     + -0.8430007526618057 ;
			                                 }
			                             } else {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * -0.42448432164702049052
			                                 + privateLLCEstimates.writebacks * -0.00000937420692556325
			                                 + privateStallCycles * -0.00000120986236911957
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000166462349621
			                                 + avgSharedLat * 0.00028277334410455979
			                                 + commitCycles * 0.00000102264372949106
			                                 + memoryIndependentStallCycles * 0.00000009416903010958
			                                 + avgPrivateMemsysLat * -0.19930213447341571520
			                                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000004274987668
			                                 + sharedLLCMeasurements.accesses * -0.00000660840131203802
			                                 + privateLLCEstimates.hits * -0.00000252080206583599
			                                 + sharedLLCMeasurements.hits * 0.00002507837788996082
			                                 + 7.113245156612974 ;
			                             }
			                         }
			                     }
			                 } else {
			                     if ( stallCycles + privateStallCycles <= 4513635.0 ) {
			                         if ( reqs*(avgSharedLat+avgPrivateMemsysLat) <= 7161830.25 ) {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -0.08073718291332700192
			                             + privateLLCEstimates.writebacks * 0.00001167716486326945
			                             + privateStallCycles * -0.00000011423904810849
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000286149357093
			                             + avgSharedLat * -0.00000814665604087455
			                             + commitCycles * 0.00000038991564831917
			                             + memoryIndependentStallCycles * -0.00000054787928326739
			                             + avgPrivateMemsysLat * -0.00016463254309211740
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000049997746
			                             + sharedLLCMeasurements.accesses * -0.00001986280297216974
			                             + privateLLCEstimates.hits * 0.00001518193323119321
			                             + sharedLLCMeasurements.hits * 0.00000115930514129331
			                             + 1.3402550830644093 ;
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * -1.16468290653735806472
			                             + privateLLCEstimates.writebacks * -0.00009403371049568685
			                             + privateStallCycles * -0.00000021989655742839
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000450353794270
			                             + avgSharedLat * -0.00007244967834676252
			                             + commitCycles * 0.00000092667671373301
			                             + memoryIndependentStallCycles * -0.00000010195975541622
			                             + avgPrivateMemsysLat * -0.00250487399961747291
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000520196442
			                             + sharedLLCMeasurements.accesses * -0.00000884699070010234
			                             + privateLLCEstimates.hits * 0.00000211653702827180
			                             + sharedLLCMeasurements.hits * 0.00000588986407578028
			                             + 1.00207199758306 ;
			                         }
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * -0.05281852321503621023
			                         + privateLLCEstimates.writebacks * -0.00000337846699824029
			                         + privateStallCycles * 0.00000000918140203930
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000927999632285
			                         + avgSharedLat * 0.00015082296493302961
			                         + commitCycles * 0.00000045261200792900
			                         + memoryIndependentStallCycles * 0.00000029940564465235
			                         + avgPrivateMemsysLat * -0.00274928666298356007
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000107564676
			                         + sharedLLCMeasurements.accesses * 0.00000308890974855093
			                         + privateLLCEstimates.hits * -0.00000038476355702315
			                         + sharedLLCMeasurements.hits * -0.00000329447302683335
			                         + 0.2105732743837766 ;
			                     }
			                 }
			             } else {
			                 if ( avgPrivateMemsysLat <= 34.771 ) {
			                     if ( memoryIndependentStallCycles <= 72330.0 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 1.00681104408902788450
			                         + privateLLCEstimates.writebacks * -0.00000038752961589432
			                         + privateStallCycles * -0.00000005918989441321
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000456006855916
			                         + avgSharedLat * -0.00000391813300431359
			                         + commitCycles * -0.00000025010297803775
			                         + memoryIndependentStallCycles * -0.00000347882441498128
			                         + avgPrivateMemsysLat * 0.14016670296142894059
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000243800690
			                         + sharedLLCMeasurements.accesses * -0.00000586485246930732
			                         + privateLLCEstimates.hits * 0.00000655718163725021
			                         + sharedLLCMeasurements.hits * -0.00001016046697127256
			                         + -3.7503952700355176 ;
			                     } else {
			                         if ( hiddenLoads <= 198.0 ) {
			                             if ( writeStall <= 345827.0 ) {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 1.97119607478729763628
			                                 + privateLLCEstimates.writebacks * -0.00000917031777934410
			                                 + privateStallCycles * -0.00000020113957011275
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000002325994474551
			                                 + avgSharedLat * 0.00000346786261784207
			                                 + commitCycles * -0.00000057169096658277
			                                 + memoryIndependentStallCycles * -0.00000027570906583002
			                                 + avgPrivateMemsysLat * 0.00149079631058822589
			                                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000260107271
			                                 + sharedLLCMeasurements.accesses * -0.00001939050790120889
			                                 + privateLLCEstimates.hits * 0.00000704714853782583
			                                 + sharedLLCMeasurements.hits * 0.00000435410679410321
			                                 + 0.7088186541558279 ;
			                             } else {
			                                 aloneIPCEstimate = 0.0
			                                 + sharedIPC * 0.61722414791163882075
			                                 + privateLLCEstimates.writebacks * -0.00003251887205399963
			                                 + privateStallCycles * -0.00000006400641412433
			                                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000000593883679524
			                                 + avgSharedLat * -0.00000095508034446056
			                                 + commitCycles * -0.00000020448109760091
			                                 + memoryIndependentStallCycles * -0.00000005797305807267
			                                 + avgPrivateMemsysLat * -0.09947907231714955834
			                                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000271090773
			                                 + sharedLLCMeasurements.accesses * 0.00000038692901957493
			                                 + privateLLCEstimates.hits * 0.00000157977466566590
			                                 + sharedLLCMeasurements.hits * -0.00000920144755552762
			                                 + 4.539732137713866 ;
			                             }
			                         } else {
			                             aloneIPCEstimate = 0.0
			                             + sharedIPC * 1.24609392856689704665
			                             + privateLLCEstimates.writebacks * -0.00005096638920861243
			                             + privateStallCycles * 0.00000005881266296771
			                             + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000007411561992863
			                             + avgSharedLat * 0.00004507166696689380
			                             + commitCycles * -0.00000053740518886257
			                             + memoryIndependentStallCycles * -0.00000026514382488105
			                             + avgPrivateMemsysLat * 0.00642193948619253132
			                             + comInstModelTraceCummulativeInst[cpuID] * 0.00000000001265924796
			                             + sharedLLCMeasurements.accesses * -0.00001851915811180021
			                             + privateLLCEstimates.hits * 0.00000308790102774596
			                             + sharedLLCMeasurements.hits * 0.00000192670023483451
			                             + 0.9453172088369198 ;
			                         }
			                     }
			                 } else {
			                     if ( emptyROBStallCycles <= 393324.0 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 1.05932198097122820535
			                         + privateLLCEstimates.writebacks * -0.00015155357911011028
			                         + privateStallCycles * -0.00000026953138544074
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000363472229301
			                         + avgSharedLat * 0.00002728740078371892
			                         + commitCycles * -0.00000016477673625265
			                         + memoryIndependentStallCycles * -0.00000029316969228972
			                         + avgPrivateMemsysLat * 0.03499645985165922291
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000510388902
			                         + sharedLLCMeasurements.accesses * -0.00002494310207855438
			                         + privateLLCEstimates.hits * 0.00002192852231076074
			                         + sharedLLCMeasurements.hits * 0.00000053419903736019
			                         + -0.2870333348323886 ;
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 2.17733775885502112146
			                         + privateLLCEstimates.writebacks * -0.00000177751179812512
			                         + privateStallCycles * -0.00000023554447598921
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000002436247932753
			                         + avgSharedLat * 0.00005595198714555811
			                         + commitCycles * -0.00000044838340542710
			                         + memoryIndependentStallCycles * -0.00000003645600688923
			                         + avgPrivateMemsysLat * -0.00040403517747750022
			                         + comInstModelTraceCummulativeInst[cpuID] * -0.00000000003101006579
			                         + sharedLLCMeasurements.accesses * -0.00001253653477057605
			                         + privateLLCEstimates.hits * 0.00001993458411362044
			                         + sharedLLCMeasurements.hits * -0.00001160859802888740
			                         + 0.28562120459841445 ;
			                     }
			                 }
			             }
			         } else {
			             if ( privateLLCEstimates.writebacks <= 7812.0 ) {
			                 if ( reqs*(avgSharedLat+avgPrivateMemsysLat) <= 8211880.0 ) {
			                     if ( memoryIndependentStallCycles <= 343017.5 ) {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 0.66594966571483427131
			                         + privateLLCEstimates.writebacks * -0.00001956988365981576
			                         + privateStallCycles * -0.00000025532344256613
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000002739078773702
			                         + avgSharedLat * 0.00006765865122520203
			                         + commitCycles * 0.00000000552855947500
			                         + memoryIndependentStallCycles * 0.00000006666113513948
			                         + avgPrivateMemsysLat * -0.00507599583094077086
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000002891085197
			                         + sharedLLCMeasurements.accesses * -0.00000812047936760734
			                         + privateLLCEstimates.hits * 0.00000972673250982814
			                         + sharedLLCMeasurements.hits * -0.00000036575474405956
			                         + 0.948550974984346 ;
			                     } else {
			                         aloneIPCEstimate = 0.0
			                         + sharedIPC * 0.89555641462680934950
			                         + privateLLCEstimates.writebacks * -0.00003461264996791106
			                         + privateStallCycles * -0.00000017935575361779
			                         + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000048742561482
			                         + avgSharedLat * 0.00010582021260562756
			                         + commitCycles * -0.00000006241531306399
			                         + memoryIndependentStallCycles * -0.00000018045271772041
			                         + avgPrivateMemsysLat * -0.00105302673442860635
			                         + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000289923750
			                         + sharedLLCMeasurements.accesses * -0.00001089747899412851
			                         + privateLLCEstimates.hits * 0.00001130215128745552
			                         + sharedLLCMeasurements.hits * 0.00000048339788128245
			                         + 0.7680661142140309 ;
			                     }
			                 } else {
			                     aloneIPCEstimate = 0.0
			                     + sharedIPC * 1.26292115857422926339
			                     + privateLLCEstimates.writebacks * -0.00002618216087957951
			                     + privateStallCycles * -0.00000013277214406906
			                     + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000442334678367
			                     + avgSharedLat * 0.00002857507611259245
			                     + commitCycles * -0.00000002302355670479
			                     + memoryIndependentStallCycles * -0.00000044135738012881
			                     + avgPrivateMemsysLat * -0.01881249720280470453
			                     + comInstModelTraceCummulativeInst[cpuID] * 0.00000000006716056344
			                     + sharedLLCMeasurements.accesses * -0.00000998533815355527
			                     + privateLLCEstimates.hits * 0.00001663916138138576
			                     + sharedLLCMeasurements.hits * -0.00000619337111234765
			                     + 1.2716603556027608 ;
			                 }
			             } else {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 0.23215954074889680325
			                 + privateLLCEstimates.writebacks * -0.00000573964672299320
			                 + privateStallCycles * 0.00000025107311143732
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000534883978380
			                 + avgSharedLat * 0.00003332603226437621
			                 + commitCycles * 0.00000017750732259264
			                 + memoryIndependentStallCycles * -0.00000009373774483893
			                 + avgPrivateMemsysLat * -0.00725717584874307087
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000002663852813
			                 + sharedLLCMeasurements.accesses * -0.00000381899456453306
			                 + privateLLCEstimates.hits * 0.00000327722753104490
			                 + sharedLLCMeasurements.hits * 0.00000363766805247423
			                 + 0.7004763574813981 ;
			             }
			         }
			     } else {
			         if ( emptyROBStallCycles <= 31927.5 ) {
			             if ( reqs <= 28182.5 ) {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * -0.04752990092436758934
			                 + privateLLCEstimates.writebacks * -0.00011146694811208421
			                 + privateStallCycles * 0.00000002169965049265
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * -0.00000000138062464515
			                 + avgSharedLat * -0.00000718285802737168
			                 + commitCycles * 0.00000000163578998408
			                 + memoryIndependentStallCycles * 0.00000000841178872734
			                 + avgPrivateMemsysLat * -0.00240149366345838262
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000124569863
			                 + sharedLLCMeasurements.accesses * -0.00000374820974731197
			                 + privateLLCEstimates.hits * 0.00000207404800214285
			                 + sharedLLCMeasurements.hits * 0.00000230170258222302
			                 + 1.421784268000453 ;
			             } else {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 1.04475868622452661150
			                 + privateLLCEstimates.writebacks * 0.00000058556617402688
			                 + privateStallCycles * -0.00000004547518000335
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000001099392222216
			                 + avgSharedLat * 0.00165653398782415187
			                 + commitCycles * -0.00000002220393123571
			                 + memoryIndependentStallCycles * 0.00000005197844786869
			                 + avgPrivateMemsysLat * 0.00192472935271691559
			                 + comInstModelTraceCummulativeInst[cpuID] * -0.00000000000097276901
			                 + sharedLLCMeasurements.accesses * -0.00001083528543253553
			                 + privateLLCEstimates.hits * -0.00000057016772451476
			                 + sharedLLCMeasurements.hits * 0.00001045950137832121
			                 + -0.11561607303710397 ;
			             }
			         } else {
			             if ( emptyROBStallCycles <= 329016.5 ) {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 0.86352036277522192798
			                 + privateLLCEstimates.writebacks * -0.00003138825713275411
			                 + privateStallCycles * -0.00000013205764948321
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000001551259072748
			                 + avgSharedLat * 0.00001824195038372440
			                 + commitCycles * -0.00000016239713155209
			                 + memoryIndependentStallCycles * -0.00000012972100078951
			                 + avgPrivateMemsysLat * 0.00007462136725828630
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000361294632
			                 + sharedLLCMeasurements.accesses * -0.00000499203462915123
			                 + privateLLCEstimates.hits * -0.00000009480553619791
			                 + sharedLLCMeasurements.hits * 0.00000099491408210858
			                 + 0.8768897780166185 ;
			             } else {
			                 aloneIPCEstimate = 0.0
			                 + sharedIPC * 1.04432254477110064705
			                 + privateLLCEstimates.writebacks * -0.00000003219849399530
			                 + privateStallCycles * -0.00000005281604360439
			                 + reqs*(avgSharedLat+avgPrivateMemsysLat) * 0.00000005060954926763
			                 + avgSharedLat * 0.00003494979559988334
			                 + commitCycles * -0.00000006246875595567
			                 + memoryIndependentStallCycles * -0.00000006729796205395
			                 + avgPrivateMemsysLat * -0.00030396013393444701
			                 + comInstModelTraceCummulativeInst[cpuID] * 0.00000000000671488093
			                 + sharedLLCMeasurements.accesses * -0.00000296405501508614
			                 + privateLLCEstimates.hits * 0.00001609643911179856
			                 + sharedLLCMeasurements.hits * -0.00001612831683991226
			                 + 0.21961649573183217 ;
			             }
			         }
			     }
			 }
			 if (aloneIPCEstimate < 0.0) {
				 aloneIPCEstimate = 0.0;
			 }
		}

		data.push_back(avgSharedLat);
		data.push_back(avgPrivateMemsysLat);
		data.push_back(avgPrivateLatEstimate);
		data.push_back(ols.cptMeasurements.privateLatencyEstimate());
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
		data.push_back(writeStallEstimate);
		data.push_back(alonePrivBlockedStallEstimate);
		data.push_back(avgSharedStoreLat);
		data.push_back(avgPrivmodeStoreLat);
		data.push_back(numStores);
		data.push_back(aloneROBStallEstimate);
		data.push_back(boisAloneStallEst);
		data.push_back(lastModelError[cpuID]);
		data.push_back(lastModelErrorWithCutoff[cpuID]);
		data.push_back(lastCPLPolicyDesicion[cpuID]);
		data.push_back(getHybridAverageError(cpuID));
		data.push_back(ols.itcaAccountedCycles.accountedCycles);
		data.push_back(privateLLCEstimates.hits);
		data.push_back(privateLLCEstimates.accesses);
		data.push_back(privateLLCEstimates.writebacks);
		data.push_back(privateLLCEstimates.interferenceMisses);
		data.push_back(sharedLLCMeasurements.hits);
		data.push_back(sharedLLCMeasurements.accesses);
		data.push_back(sharedLLCMeasurements.writebacks);

		// Update values needed for cache partitioning policy
		aloneIPCEstimates[cpuID] = aloneIPCEstimate;
		sharedCPLMeasurements[cpuID] = ols.tableCPL;
		sharedNonSharedLoadCycles[cpuID] = cyclesInSample-stallCycles;
		sharedLoadStallCycles[cpuID] = stallCycles;
		privateMemsysAvgLatency[cpuID] = avgPrivateMemsysLat;
		sharedMemsysAvgLatency[cpuID] = avgSharedLat;


		DPRINTF(MissBWPolicy, "CPU %d - Setting alone IPC estimate to %f, CPL to %f, non memory cycles %f, memory stall cycles %f, avg private memsys latency %f, avg shared memsys latency %f\n",
				cpuID,
				aloneIPCEstimates[cpuID],
				sharedCPLMeasurements[cpuID],
				sharedNonSharedLoadCycles[cpuID],
				sharedLoadStallCycles[cpuID],
				privateMemsysAvgLatency[cpuID],
				sharedMemsysAvgLatency[cpuID]);
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
		data.push_back(ols.cptMeasurements.averageCPLatency());
		data.push_back(aloneIPC);
		data.push_back(aloneOverlap);
		data.push_back(privateMissRate);
		data.push_back(stallCycles+privateStallCycles);

		// CPL models with global data
		data.push_back(((avgSharedLat+avgPrivateMemsysLat)*ols.tableCPL)+privateStallCycles);
		data.push_back(((avgSharedLat+avgPrivateMemsysLat-cwp)*ols.tableCPL)+privateStallCycles);

		// CPL models with critical path data (latency is round trip and thus includes both the private and shared memsys)
		data.push_back(((ols.cptMeasurements.averageCPLatency())*ols.tableCPL)+privateStallCycles);
		data.push_back(((ols.cptMeasurements.averageCPLatency()-ols.cptMeasurements.averageCPCWP())*ols.tableCPL)+privateStallCycles);

		data.push_back(sharedLLCMeasurements.hits);
		data.push_back(sharedLLCMeasurements.accesses);
		data.push_back(sharedLLCMeasurements.writebacks);
	}

	comInstModelTraces[cpuID].addTrace(data);
}

double
BasePolicy::estimateWriteStallCycles(double writeStall, double avgPrivmodeLat, int numWriteStalls, double avgSharedmodeLat){
	if(writeStallTech == WS_NONE){
		return 0.0;
	}
	if(writeStallTech == WS_SHARED){
		return writeStall;
	}
	if(writeStallTech == WS_LATENCY){
		return avgPrivmodeLat*numWriteStalls;
	}
	if(writeStallTech == WS_RATIO){
		if(avgSharedmodeLat > 0){
			return writeStall * (avgPrivmodeLat / avgSharedmodeLat);
		}
		return 0;
	}

	fatal("unknown write stall technique");
	return 0.0;
}

double
BasePolicy::estimatePrivateBlockedStall(double privBlocked, double avgPrivmodeLat, double avgSharedmodeLat){
	if(privBlockedStallTech == PBS_NONE){
		return 0.0;
	}
	if(privBlockedStallTech  == PBS_SHARED){
		return privBlocked;
	}
	if(privBlockedStallTech == PBS_RATIO){
		if(avgSharedmodeLat > 0){
			double ratio = avgPrivmodeLat / avgSharedmodeLat;
			return privBlocked * ratio;
		}
		return 0;
	}
	fatal("unknown pbs technique");
	return 0.0;
}

double
BasePolicy::estimatePrivateROBStall(double sharedROBStall, double avgPrivmodeLat, double avgSharedmodeLat){
	if(emptyROBStallTech == RST_NONE){
		return 0.0;
	}
	if(emptyROBStallTech == RST_SHARED){
		return sharedROBStall;
	}
	if(emptyROBStallTech == RST_RATIO){
		if(avgSharedmodeLat > 0){
			return sharedROBStall * (avgPrivmodeLat / avgSharedmodeLat);
		}
		return 0;
	}

	fatal("unknown empty ROB stall technique");
	return 0.0;
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
	if(methodName == "cpl-cwp") return CPL_CWP;
	if(methodName == "cpl-damp") return CPL_DAMP;
	if(methodName == "cpl-cwp-damp") return CPL_CWP_DAMP;
	if(methodName == "cpl-hybrid") return CPL_HYBRID;
	if(methodName == "cpl-hybrid-damp") return CPL_HYBRID_DAMP;
	if(methodName == "cpl-cwp-ser") return CPL_CWP_SER;
	if(methodName == "bois") return BOIS;
	if(methodName == "ITCA") return ITCA;
	if(methodName == "private-latency-only") return PRIVATE_LATENCY_ONLY;
	if(methodName == "shared-stall") return SHARED_STALL;
	if(methodName == "zero-stall") return ZERO_STALL;
	if(methodName == "const-1") return CONST_1;
	if(methodName == "ASR") return ASR;
	if(methodName == "lin-tree-10") return LIN_TREE_10;
	if(methodName == "lin-tree-40") return LIN_TREE_40;
	if(methodName == "lin-tree-80") return LIN_TREE_80;

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

BasePolicy::WriteStallTechnique
BasePolicy::parseWriteStallTech(std::string techName){
	if(techName == "ws-none") return WS_NONE;
	if(techName == "ws-shared") return WS_SHARED;
	if(techName == "ws-latency") return WS_LATENCY;
	if(techName == "ws-ratio") return WS_RATIO;

	fatal("unknown write stall technique");
	return WS_NONE;
}

BasePolicy::EmptyROBStallTechnique
BasePolicy::parseEmptyROBStallTech(std::string techName){
	if(techName == "rst-none") return RST_NONE;
	if(techName == "rst-shared") return RST_SHARED;
	if(techName == "rst-ratio") return RST_RATIO;

	fatal("unknown ROB stall technique");
	return RST_NONE;
}

BasePolicy::PrivBlockedStallTechnique
BasePolicy::parsePrivBlockedStallTech(std::string techName){
	if(techName == "pbs-none") return PBS_NONE;
	if(techName == "pbs-shared") return PBS_SHARED;
	if(techName == "pbs-ratio") return PBS_RATIO;

	fatal("unknown pbs technique");
	return PBS_NONE;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("BasePolicy", BasePolicy);

#endif

