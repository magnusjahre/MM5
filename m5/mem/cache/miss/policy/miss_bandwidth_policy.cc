/*
 * miss_bandwidth_policy.cc
 *
 *  Created on: Oct 28, 2009
 *      Author: jahre
 */

#include "miss_bandwidth_policy.hh"

#include <cmath>

using namespace std;

MissBandwidthPolicy::MissBandwidthPolicy(string _name,
										 InterferenceManager* _intManager,
									     Tick _period,
									     int _cpuCount,
									     double _busUtilThreshold,
									     double _cutoffReqInt,
									     RequestEstimationMethod _reqEstMethod,
									     PerformanceEstimationMethod _perfEstMethod,
									     bool _persistentAllocations,
									     bool _enforcePolicy)
: SimObject(_name){

	intManager = _intManager;

	perfEstMethod = _perfEstMethod;
	reqEstMethod = _reqEstMethod;

	busUtilizationThreshold = _busUtilThreshold;
	requestCountThreshold = _cutoffReqInt * _period;

	acceptanceThreshold = 1.05; // TODO: parameterize

	renewMeasurementsThreshold = 10; // TODO: parameterize
	renewMeasurementsCounter = 0;

	usePersistentAllocations = _persistentAllocations;

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

	cpuCount = _cpuCount;
	caches.resize(cpuCount, 0);

	cummulativeMemoryRequests.resize(_cpuCount, 0);
	cummulativeCommittedInsts.resize(_cpuCount, 0);

	aloneIPCEstimates.resize(_cpuCount, 0.0);
	avgLatencyAloneIPCModel.resize(_cpuCount, 0.0);

	level = 0;
	maxMetricValue = 0;

	currentMeasurements = NULL;

	mostRecentMWSEstimate.resize(cpuCount, vector<double>());
	mostRecentMLPEstimate.resize(cpuCount, vector<double>());

	comInstModelTraceCummulativeInst.resize(cpuCount, 0);

	initProjectionTrace(_cpuCount);
	initAloneIPCTrace(_cpuCount, _enforcePolicy);
	initNumMSHRsTrace(_cpuCount);
	initComInstModelTrace(_cpuCount);
}

MissBandwidthPolicy::~MissBandwidthPolicy(){
	if(policyEvent != NULL){
		assert(!policyEvent->scheduled());
		delete policyEvent;
	}
}

void
MissBandwidthPolicy::regStats(){
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
MissBandwidthPolicy::registerCache(BaseCache* _cache, int _cpuID, int _maxMSHRs){
	assert(caches[_cpuID] == NULL);
	caches[_cpuID] = _cache;
	maxMSHRs = _maxMSHRs;
}

void
MissBandwidthPolicy::handlePolicyEvent(){
	PerformanceMeasurement curMeasurement = intManager->buildInterferenceMeasurement(period);
	runPolicy(curMeasurement);
}

void
MissBandwidthPolicy::handleTraceEvent(){
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
MissBandwidthPolicy::initAloneIPCTrace(int cpuCount, bool policyEnforced){
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
MissBandwidthPolicy::traceAloneIPC(std::vector<int> memoryRequests,
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
MissBandwidthPolicy::addTraceEntry(PerformanceMeasurement* measurement){

	if(!measurementTrace.isInitialized()){
		vector<string> header = measurement->getTraceHeader();
		measurementTrace.initalizeTrace(header);
	}
	vector<RequestTraceEntry> line = measurement->createTraceLine();
	measurementTrace.addTrace(line);

}

void
MissBandwidthPolicy::traceVerboseVector(const char* message, std::vector<int>& data){
	DPRINTFR(MissBWPolicyExtra, message);
	for(int i=0;i<data.size();i++) DPRINTFR(MissBWPolicyExtra, "%d:%d ", i, data[i]);
	DPRINTFR(MissBWPolicyExtra, "\n");
}

void
MissBandwidthPolicy::traceVerboseVector(const char* message, std::vector<double>& data){
	DPRINTFR(MissBWPolicyExtra, message);
	for(int i=0;i<data.size();i++) DPRINTFR(MissBWPolicyExtra, "%d:%f ", i, data[i]);
	DPRINTFR(MissBWPolicyExtra, "\n");
}

void
MissBandwidthPolicy::traceVector(const char* message, std::vector<int>& data){
	DPRINTF(MissBWPolicy, message);
	for(int i=0;i<data.size();i++) DPRINTFR(MissBWPolicy, "%d:%d ", i, data[i]);
	DPRINTFR(MissBWPolicy, "\n");
}

void
MissBandwidthPolicy::traceVector(const char* message, std::vector<double>& data){
	DPRINTF(MissBWPolicy, message);
	for(int i=0;i<data.size();i++) DPRINTFR(MissBWPolicy, "%d:%f ", i, data[i]);
	DPRINTFR(MissBWPolicy, "\n");
}

void
MissBandwidthPolicy::tracePerformance(std::vector<double>& sharedIPCEstimate){
	traceVerboseVector("Shared IPC Estimate: ", sharedIPCEstimate);
	traceVerboseVector("Alone IPC Estimate: ", aloneIPCEstimates);
	traceVerboseVector("Estimated Speedup: ", currentSpeedupProjection);
}

bool
MissBandwidthPolicy::doMHAEvaluation(std::vector<int>& currentMHA){
	for(int i=0;i<currentMHA.size();i++){
		if(currentMHA[i] < maxMSHRs && currentMeasurements->requestsInSample[i] < requestCountThreshold){
//			DPRINTFR(MissBWPolicyExtra, "Pruning MHA for CPU %d since number of requests %d < threshold %d\n", i, currentMeasurements->requestsInSample[i], requestCountThreshold);
			return false;
		}
	}

	return true;
}

double
MissBandwidthPolicy::evaluateMHA(std::vector<int>* mhaConfig){
	vector<int> currentMHA = relocateMHA(mhaConfig);

	// 1. Prune search space
	if(!doMHAEvaluation(currentMHA)) return 0.0;

	traceVerboseVector("--- Evaluating MHA: ", currentMHA);

	traceVerboseVector("Request Count Measurement: ", currentMeasurements->requestsInSample);
	traceVerboseVector("Shared Latency Measurement: ", currentMeasurements->sharedLatencies);

	// 2. Estimate new shared memory latencies
	vector<double> sharedLatencyEstimates(cpuCount, 0.0);
	getAverageMemoryLatency(&currentMHA, &sharedLatencyEstimates);
	currentLatencyProjection = sharedLatencyEstimates;

	traceVerboseVector("Shared Latency Projection: ", sharedLatencyEstimates);

	// 3. Compute speedups
	vector<double> speedups(cpuCount, 0.0);
	vector<double> sharedIPCEstimates(cpuCount, 0.0);
	currentMWSProjection.resize(cpuCount, 0.0);
	currentMLPProjection.resize(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++){

		currentMWSProjection[i] = mostRecentMWSEstimate[i][currentMHA[i]];
		currentMLPProjection[i] = mostRecentMLPEstimate[i][currentMHA[i]];

		double newStallEstimate = estimateStallCycles(currentMeasurements->cpuStallCycles[i],
				                                      mostRecentMWSEstimate[i][caches[i]->getCurrentMSHRCount(true)],
				                                      mostRecentMLPEstimate[i][caches[i]->getCurrentMSHRCount(true)],
													  currentMeasurements->sharedLatencies[i],
													  currentMeasurements->requestsInSample[i],
													  mostRecentMWSEstimate[i][currentMHA[i]],
													  mostRecentMLPEstimate[i][currentMHA[i]],
													  sharedLatencyEstimates[i],
													  currentMeasurements->requestsInSample[i]);

		sharedIPCEstimates[i]= (double) currentMeasurements->committedInstructions[i] / (currentMeasurements->getNonStallCycles(i, period) + newStallEstimate);
		speedups[i] = computeSpeedup(sharedIPCEstimates[i], i);

		DPRINTFR(MissBWPolicyExtra, "CPU %d, current stall %d, estimating new stall time %f, new mws %f, new mlp %f, current stalled %d\n",
									i,
									currentMeasurements->cpuStallCycles[i],
									newStallEstimate,
									mostRecentMWSEstimate[i][currentMHA[i]],
									mostRecentMLPEstimate[i][currentMHA[i]],
									currentMeasurements->cpuStallCycles[i]);
	}

	currentIPCProjection = sharedIPCEstimates;
	currentSpeedupProjection = speedups;
	tracePerformance(sharedIPCEstimates);

	// 4. Compute metric
	double metricValue = computeMetric(&speedups);

	DPRINTFR(MissBWPolicyExtra, "Returning metric value %f\n", metricValue);

	return metricValue;
}

template <class T>
T
MissBandwidthPolicy::computeSum(vector<T>* values){
	T sum = 0;
	for(int i=0;i<cpuCount;i++){
		sum += values->at(i);
	}
	return sum;
}

template <class T>
vector<double>
MissBandwidthPolicy::computePercetages(vector<T>* values){
	double sum = computeSum(values);

	vector<double> percentages(values->size(), 0.0);
	for(int i=0;i<cpuCount;i++){
		percentages[i] = values->at(i) / sum;
	}

	return percentages;
}


double
MissBandwidthPolicy::computeRequestScalingRatio(int cpuID, int newMSHRCount){

	int currentMSHRCount = caches[cpuID]->getCurrentMSHRCount(true);

	if(reqEstMethod == MWS){

		if(mostRecentMWSEstimate[cpuID][currentMSHRCount] == 0){
			return 1;
		}
		else{
			return mostRecentMWSEstimate[cpuID][newMSHRCount] / mostRecentMWSEstimate[cpuID][currentMSHRCount];
		}

	}
	else if(reqEstMethod == MLP){

		if(currentMeasurements->mlpEstimate[cpuID][newMSHRCount] == 0){
			return 1;
		}
		else{
			return currentMeasurements->mlpEstimate[cpuID][currentMSHRCount] / currentMeasurements->mlpEstimate[cpuID][newMSHRCount];
		}
	}
	fatal("unknown request scaling ratio");
	return 0.0;
}

void
MissBandwidthPolicy::getAverageMemoryLatency(vector<int>* currentMHA,
											 vector<double>* estimatedSharedLatencies){

	assert(estimatedSharedLatencies->size() == cpuCount);
	assert(currentMeasurements != NULL);

	vector<double> newRequestCountEstimates(cpuCount, 0.0);

	// 1. Estimate change in request count due change in MLP
	double freeBusSlots = 0;
	for(int i=0;i<cpuCount;i++){

		double mlpRatio = computeRequestScalingRatio(i, currentMHA->at(i));

		newRequestCountEstimates[i] = (double) currentMeasurements->requestsInSample[i] * mlpRatio;

		double tmpFreeBusSlots = currentMeasurements->sharedCacheMissRate * (((double) currentMeasurements->requestsInSample[i]) - newRequestCountEstimates[i]);
		freeBusSlots += tmpFreeBusSlots;

		DPRINTFR(MissBWPolicyExtra, "Request change for CPU %i, MLP ratio %f, request estimate %f, freed bus slots %f\n", i, mlpRatio, newRequestCountEstimates[i], tmpFreeBusSlots);
	}

	// 2. Estimate the response in request demand
	vector<double> additionalBusRequests(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++){
		double busRequests = currentMeasurements->sharedCacheMissRate * (double) currentMeasurements->requestsInSample[i];
		if(busRequests > requestCountThreshold && currentMHA->at(i) >= caches[i]->getCurrentMSHRCount(true)){
			double queueRatio = 1.0;
			if(currentMeasurements->privateLatencyBreakdown[i][InterferenceManager::MemoryBusQueue] > 0){
				queueRatio = currentMeasurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue] / currentMeasurements->privateLatencyBreakdown[i][InterferenceManager::MemoryBusQueue];
			}
			double additionalRequests = queueRatio * currentMeasurements->requestsInSample[i];
			additionalBusRequests[i] = additionalRequests * currentMeasurements->sharedCacheMissRate;
			DPRINTFR(MissBWPolicyExtra, "CPU %d has queue ratio %f, estimating %f reqs in next sample, %f bus reqs\n", i, queueRatio, additionalRequests, additionalBusRequests[i]);
		}
		else{
			DPRINTFR(MissBWPolicyExtra, "CPU %d has too few requests (%f < %i) or num MSHRs is not increased (new count %d < old count %d)\n", i, busRequests, requestCountThreshold, currentMHA->at(i), caches[i]->getCurrentMSHRCount(true));
		}
	}

	vector<double> currentRequestDistribution = computePercetages(&(currentMeasurements->requestsInSample));
	if(currentMeasurements->actualBusUtilization > busUtilizationThreshold && freeBusSlots > 0){
		DPRINTFR(MissBWPolicyExtra, "Bus utilzation is larger than threshold (%f > %f)\n", currentMeasurements->actualBusUtilization, busUtilizationThreshold);
		double addBusReqSum = computeSum(&additionalBusRequests);
		if(addBusReqSum > freeBusSlots){
			for(int i=0;i<cpuCount;i++){
				newRequestCountEstimates[i] += freeBusSlots * (double) currentRequestDistribution[i];
			}
		}
		else{
			for(int i=0;i<cpuCount;i++){
				newRequestCountEstimates[i] += additionalBusRequests[i];
			}
		}
	}
	else{
		DPRINTFR(MissBWPolicyExtra, "Bus utilzation smaller than threshold (%f < %f) or no free bus slots (%f)\n", currentMeasurements->actualBusUtilization, busUtilizationThreshold, freeBusSlots);
	}

	// 3. Estimate the new average request latency
	double oldTotalRequests = 0.0;
	double newTotalRequests = 0.0;
	for(int i=0;i<cpuCount;i++){
		oldTotalRequests += currentMeasurements->sharedCacheMissRate * currentMeasurements->requestsInSample[i];
		newTotalRequests += currentMeasurements->sharedCacheMissRate * newRequestCountEstimates[i];
	}

	double requestRatio = 1.0;
	if(oldTotalRequests != 0){
		requestRatio = newTotalRequests / oldTotalRequests;
	}

	DPRINTFR(MissBWPolicyExtra, "New request ratio is %f, estimated new request count %f, old request count %f\n", requestRatio, newTotalRequests, oldTotalRequests);

	for(int i=0;i<cpuCount;i++){
		double currentAvgBusLat = currentMeasurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue];
		double newAvgBusLat = requestRatio * currentAvgBusLat;
		(*estimatedSharedLatencies)[i] = (currentMeasurements->sharedLatencies[i] - currentAvgBusLat) + newAvgBusLat;
		DPRINTFR(MissBWPolicyExtra, "CPU %i, estimating bus lat to %f, new bus lat %f, new avg lat %f, request ratio %f\n", i, currentAvgBusLat, newAvgBusLat, estimatedSharedLatencies->at(i), requestRatio);
	}

	traceVerboseVector("Request Count Projection: ", newRequestCountEstimates);
	currentRequestProjection = newRequestCountEstimates;
}

double
MissBandwidthPolicy::computeError(double estimate, double actual){
	if(actual == 0) return 0;
	double relErr = (estimate - actual) / actual;
	return relErr*100;
}

double
MissBandwidthPolicy::computeSpeedup(double sharedIPCEstimate, int cpuID){
	if(aloneIPCEstimates[cpuID] == 0) return 1.0;
	return sharedIPCEstimate / aloneIPCEstimates[cpuID];
}

double
MissBandwidthPolicy::computeCurrentMetricValue(){

	vector<double> speedups(cpuCount, 0.0);

	for(int i=0;i<cpuCount;i++){
		double currentSharedIPC = (double) currentMeasurements->committedInstructions[i] / (double) period;
		speedups[i] = computeSpeedup(currentSharedIPC, i);
	}
	traceVector("Estimated Current Speedups: ", speedups);

	double metricValue = computeMetric(&speedups);
	DPRINTF(MissBWPolicy, "Estimated current metric value to be %f\n", metricValue);
	return metricValue;
}

double
MissBandwidthPolicy::estimateStallCycles(double currentStallTime,
									     double currentMWS,
									     double currentMLP,
									     double currentAvgSharedLat,
									     double currentRequests,
									     double newMWS,
									     double newMLP,
									     double newAvgSharedLat,
									     double newRequests){

	if(perfEstMethod == RATIO_MWS){

		if(currentMWS == 0 || currentAvgSharedLat == 0 || newMWS == 0 || newAvgSharedLat == 0) return currentStallTime;

		double currentRatio = currentAvgSharedLat / currentMWS;
		double newRatio = newAvgSharedLat / newMWS;

		double stallTimeFactor = newRatio / currentRatio;

		return currentStallTime * stallTimeFactor;
	}
	else if(perfEstMethod == LATENCY_MLP){

		double curMLRProduct = currentMLP * currentAvgSharedLat * currentRequests;
		double newMLRProduct = newMLP * newAvgSharedLat * newRequests;

		double deltaStallCycles = newMLRProduct - curMLRProduct;
		double newStallTime = currentStallTime + deltaStallCycles;

		if(newStallTime > period) return period;
		if(newStallTime < 0) return 0;

		return newStallTime;
	}

	fatal("Unknown performance estimation method");
	return 0.0;
}

void
MissBandwidthPolicy::updateAloneIPCEstimate(){
	for(int i=0;i<cpuCount;i++){
		if(caches[i]->getCurrentMSHRCount(true) == maxMSHRs){
			double stallParallelism = currentMeasurements->avgMissesWhileStalled[i][maxMSHRs];
			double curMLP = currentMeasurements->mlpEstimate[i][maxMSHRs];
			double curReqs = currentMeasurements->requestsInSample[i];
			double newStallEstimate = estimateStallCycles(currentMeasurements->cpuStallCycles[i],
														  stallParallelism,
														  curMLP,
														  currentMeasurements->sharedLatencies[i],
														  curReqs,
														  stallParallelism,
														  curMLP,
														  currentMeasurements->estimatedPrivateLatencies[i],
														  curReqs);
			aloneIPCEstimates[i] = currentMeasurements->committedInstructions[i] / (currentMeasurements->getNonStallCycles(i, period) + newStallEstimate);
			DPRINTF(MissBWPolicy, "Updating alone IPC estimate for cpu %i to %f, %d committed insts, %d non-stall cycles, %f new stall cycle estimate\n",
					              i,
					              aloneIPCEstimates[i],
					              currentMeasurements->committedInstructions[i],
					              currentMeasurements->getNonStallCycles(i, period),
								  newStallEstimate);


			double avgInterference = currentMeasurements->sharedLatencies[i] - currentMeasurements->estimatedPrivateLatencies[i];
			double totalInterferenceCycles = avgInterference * currentMeasurements->requestsInSample[i];
			double visibleIntCycles = mostRecentMLPEstimate[i][maxMSHRs] * totalInterferenceCycles;
			avgLatencyAloneIPCModel[i]= (float) currentMeasurements->committedInstructions[i] / (period - visibleIntCycles);
		}
	}
}

void
MissBandwidthPolicy::updateMWSEstimates(){

	for(int i=0;i<cpuCount;i++){
		if(caches[i]->getCurrentMSHRCount(true) == maxMSHRs){
			DPRINTF(MissBWPolicy, "Updating local MLP estimate for CPU %i\n", i);
			mostRecentMWSEstimate[i] = currentMeasurements->avgMissesWhileStalled[i];
			mostRecentMLPEstimate[i] = currentMeasurements->mlpEstimate[i];
		}
	}

}

void
MissBandwidthPolicy::runPolicy(PerformanceMeasurement measurements){
	addTraceEntry(&measurements);


	renewMeasurementsCounter++;
	if(usePersistentAllocations && renewMeasurementsCounter < renewMeasurementsThreshold && renewMeasurementsCounter > 1){
		DPRINTF(MissBWPolicy, "--- Skipping Miss Bandwidth Policy due to Persistent Allocations, counter is %d, threshold %d\n",
							  renewMeasurementsCounter,
							  renewMeasurementsThreshold);
		traceNumMSHRs();
		return;
	}


	assert(currentMeasurements == NULL);
	currentMeasurements = &measurements;

	DPRINTF(MissBWPolicy, "--- Running Miss Bandwidth Policy\n");

	traceVector("Request Count Measurement: ", currentMeasurements->requestsInSample);
	traceVector("Shared Latency Measurement: ", currentMeasurements->sharedLatencies);
	traceVector("Committed Instructions: ", currentMeasurements->committedInstructions);
	traceVector("CPU Stall Cycles: ", currentMeasurements->cpuStallCycles);
	vector<double> actualIPC(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++) actualIPC[i] = (double) currentMeasurements->committedInstructions[i] / (double) period;
	traceVector("Measured IPC: ", actualIPC);

	if(!bestRequestProjection.empty()){
		for(int i=0;i<cpuCount;i++){
			if(currentMeasurements->requestsInSample[i] > requestCountThreshold){
				double reqError = computeError(bestRequestProjection[i], measurements.requestsInSample[i]);
				requestRelError[i].sample(reqError);
				requestAbsError[i].sample(bestRequestProjection[i] - measurements.requestsInSample[i]);

				if(currentMeasurements->sharedLatencies[i] > 0){
					double latError = computeError(bestLatencyProjection[i], measurements.sharedLatencies[i]);
					sharedLatencyRelError[i].sample(latError);
					sharedLatencyAbsError[i].sample(bestLatencyProjection[i] - measurements.sharedLatencies[i]);
				}
			}
			else{
				DPRINTF(MissBWPolicy, "Skipping accuracy check for CPU %i, %d requests < threshold %f\n", i, currentMeasurements->requestsInSample[i], requestCountThreshold);
			}
		}
	}


	updateMWSEstimates();

	updateAloneIPCEstimate();
	traceVector("Alone IPC Estimates: ", aloneIPCEstimates);

	if(renewMeasurementsCounter >= renewMeasurementsThreshold){
		DPRINTF(MissBWPolicy, "Renew counter (%d) >= threshold (%d), increasing all MHAs to maximum (%d)\n",
				              renewMeasurementsCounter,
				              renewMeasurementsThreshold,
				              maxMSHRs);

		renewMeasurementsCounter = 0;

		for(int i=0;i<caches.size();i++) caches[i]->setNumMSHRs(maxMSHRs);
	}
	else{

		if(!bestRequestProjection.empty()){
			traceBestProjection();
		}

		vector<int> bestMHA = exhaustiveSearch();
		if(bestMHA.size() != cpuCount){
			DPRINTF(MissBWPolicy, "All programs have to few requests, reverting to max MSHRs configuration\n");
			bestMHA.resize(cpuCount, maxMSHRs);
		}

		traceVector("Best MHA: ", bestMHA);
		DPRINTF(MissBWPolicy, "Best metric value is %f\n", maxMetricValue);


		double currentMetricValue = computeCurrentMetricValue();
		double benefit = maxMetricValue / currentMetricValue;
		if(benefit > acceptanceThreshold){
			DPRINTF(MissBWPolicy, "Implementing new MHA, benefit is %d, acceptance threshold %d\n", benefit, acceptanceThreshold);
			for(int i=0;i<caches.size();i++) caches[i]->setNumMSHRs(bestMHA[i]);
		}
		else{
			DPRINTF(MissBWPolicy, "Benefit from new best MHA is too low (%f < %f), new MHA not chosen\n", benefit, acceptanceThreshold);
		}

		traceAloneIPC(measurements.requestsInSample,
				      aloneIPCEstimates,
				      measurements.committedInstructions,
				      measurements.cpuStallCycles,
				      measurements.sharedLatencies,
				      measurements.avgMissesWhileStalled);
	}

	traceNumMSHRs();

	currentMeasurements = NULL;
}

void
MissBandwidthPolicy::initProjectionTrace(int cpuCount){
	vector<string> headers;

	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Estimated Num Requests", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Measured Num Requests", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Estimated Avg Shared Latency", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Measured Avg Shared Latency", i));

	predictionTrace.initalizeTrace(headers);
}

void
MissBandwidthPolicy::initNumMSHRsTrace(int cpuCount){
	vector<string> headers;
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Num MSHRs", i));
	numMSHRsTrace.initalizeTrace(headers);
}

void
MissBandwidthPolicy::traceNumMSHRs(){
	vector<RequestTraceEntry> data;
	for(int i=0;i<cpuCount;i++) data.push_back(caches[i]->getCurrentMSHRCount(true));
	numMSHRsTrace.addTrace(data);
}

void
MissBandwidthPolicy::traceBestProjection(){

	vector<RequestTraceEntry> data;

	for(int i=0;i<cpuCount;i++) data.push_back(bestRequestProjection[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(currentMeasurements->requestsInSample[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(bestLatencyProjection[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(currentMeasurements->sharedLatencies[i]);

	predictionTrace.addTrace(data);
}

std::vector<int>
MissBandwidthPolicy::exhaustiveSearch(){

	vector<int> value(cpuCount, 0);
	level = -1;
	maxMetricValue = 0.0;

	recursiveExhaustiveSearch(&value, 0);

	return relocateMHA(&maxMHAConfiguration);
}

void
MissBandwidthPolicy::recursiveExhaustiveSearch(std::vector<int>* value, int k){

	assert(value->size() == cpuCount);

	level = level+1;

	if(level >= 1){
		assert(level-1 >= 0 && level-1 < value->size());
		(*value)[level-1] = k;
	}

	if(level == cpuCount){
		double metricValue = evaluateMHA(value);
		if(metricValue >= maxMetricValue){

			if(metricValue > 0) DPRINTFR(MissBWPolicyExtra, "Metric value %f is larger than previous best %f, new best MHA\n", metricValue, maxMetricValue);

			maxMetricValue = metricValue;
			maxMHAConfiguration = *value;

			bestRequestProjection = currentRequestProjection;
			bestLatencyProjection = currentLatencyProjection;
			bestMWSProjection = currentMWSProjection;
			bestMLPProjection = currentMLPProjection;
			bestIPCProjection = currentIPCProjection;
			bestSpeedupProjection = currentSpeedupProjection;
		}
	}
	else{
		for(int i=0;i<maxMSHRs;i++){
			recursiveExhaustiveSearch(value, i);
		}
	}

	level = level - 1;
}

vector<int>
MissBandwidthPolicy::relocateMHA(std::vector<int>* mhaConfig){

	vector<int> configCopy(*mhaConfig);

	for(int i=0;i<configCopy.size();i++){
		configCopy[i]++;
	}

	return configCopy;
}

void
MissBandwidthPolicy::initComInstModelTrace(int cpuCount){
	vector<string> headers;

	headers.push_back("Cummulative Committed Instructions");
	headers.push_back("Cycles in Sample");
	headers.push_back("Stall Cycles");
	headers.push_back("Misses while Stalled");

	if(cpuCount > 1){
		headers.push_back("Average Shared Latency");
		headers.push_back("Estimated Private Latency");
		headers.push_back("Shared IPC");
		headers.push_back("Estimated Alone IPC");
	}
	else{
		headers.push_back("Alone Memory Latency");
		headers.push_back("Measured Alone IPC");
	}

	comInstModelTraces.resize(cpuCount, RequestTrace());
	for(int i=0;i<cpuCount;i++){
		comInstModelTraces[i] = RequestTrace(name(), RequestTrace::buildFilename("CommittedInsts", i).c_str(), 1);
		comInstModelTraces[i].initalizeTrace(headers);
	}
}

void
MissBandwidthPolicy::doCommittedInstructionTrace(int cpuID,
		                                         double avgSharedLat,
		                                         double avgPrivateLatEstimate,
		                                         double mws,
		                                         double mlp,
		                                         int reqs,
		                                         int stallCycles,
		                                         int totalCycles,
		                                         int committedInsts){

	vector<RequestTraceEntry> data;

	comInstModelTraceCummulativeInst[cpuID] += committedInsts;

	data.push_back(comInstModelTraceCummulativeInst[cpuID]);
	data.push_back(totalCycles);
	data.push_back(stallCycles);
	data.push_back(mws);

	if(cpuCount > 1){
		double newStallEstimate = estimateStallCycles(stallCycles,
				                                      mws,
				                                      mlp,
				                                      avgSharedLat,
				                                      reqs,
				                                      mws,
				                                      mlp,
				                                      avgPrivateLatEstimate,
				                                      reqs);

		double nonStallCycles = (double) totalCycles - (double) stallCycles;

		double sharedIPC = (double) committedInsts / (double) totalCycles;
		double aloneIPCEstimate = (double) committedInsts / (nonStallCycles + newStallEstimate);

		data.push_back(avgSharedLat);
		data.push_back(avgPrivateLatEstimate);
		data.push_back(sharedIPC);
		data.push_back(aloneIPCEstimate);
	}
	else{
		double aloneIPC = (double) committedInsts / (double) totalCycles;

		data.push_back(avgSharedLat);
		data.push_back(aloneIPC);
	}

	comInstModelTraces[cpuID].addTrace(data);
}


MissBandwidthPolicy::RequestEstimationMethod
MissBandwidthPolicy::parseRequestMethod(std::string methodName){

	if(methodName == "MWS") return MWS;
	if(methodName == "MLP") return MLP;

	fatal("unknown request estimation method");
	return MWS;
}

MissBandwidthPolicy::PerformanceEstimationMethod
MissBandwidthPolicy::parsePerfrormanceMethod(std::string methodName){

	if(methodName == "latency-mlp") return LATENCY_MLP;
	if(methodName == "ratio-mws") return RATIO_MWS;

	fatal("unknown performance estimation method");
	return LATENCY_MLP;
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("MissBandwidthPolicy", MissBandwidthPolicy);

#endif

