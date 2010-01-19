/*
 * miss_bandwidth_policy.cc
 *
 *  Created on: Oct 28, 2009
 *      Author: jahre
 */

#include "miss_bandwidth_policy.hh"
#include "base/intmath.hh"

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
										 double _acceptanceThreshold,
										 double _reqVariationThreshold,
										 int _renewMeasurementsThreshold,
										 SearchAlgorithm _searchAlgorithm,
										 int _iterationLatency,
										 double _busRequestThresholdIntensity,
										 bool _enforcePolicy)
: SimObject(_name){

	intManager = _intManager;

	perfEstMethod = _perfEstMethod;
	reqEstMethod = _reqEstMethod;

	busUtilizationThreshold = _busUtilThreshold;
	requestCountThreshold = _cutoffReqInt * _period;

//	dumpInitalized = false;
//	dumpSearchSpaceAt = 0; // set this to zero to turn off

	busRequestThreshold = _busRequestThresholdIntensity * _period;

	acceptanceThreshold = _acceptanceThreshold;
	requestVariationThreshold = _reqVariationThreshold;
	renewMeasurementsThreshold = _renewMeasurementsThreshold;

	renewMeasurementsCounter = 0;

	usePersistentAllocations = _persistentAllocations;

	iterationLatency = _iterationLatency;
	searchAlgorithm = _searchAlgorithm;

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

	bestRequestProjection.resize(cpuCount, 0.0);
	measurementsValid = false;

	requestAccumulator.resize(cpuCount, 0.0);
	requestSqAccumulator.resize(cpuCount, 0.0);
	avgReqsPerSample.resize(cpuCount, 0.0);
	reqsPerSampleStdDev.resize(cpuCount, 0.0);

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
MissBandwidthPolicy::tracePerformance(std::vector<double>& sharedIPCEstimate,
		                              std::vector<double>& speedups){
	traceVerboseVector("Shared IPC Estimate: ", sharedIPCEstimate);
	traceVerboseVector("Alone IPC Estimate: ", aloneIPCEstimates);
	traceVerboseVector("Estimated Speedup: ", speedups);
}

bool
MissBandwidthPolicy::doMHAEvaluation(int cpuID){

	if(currentMeasurements->requestsInSample[cpuID] < requestCountThreshold){
		return false;
	}

	double relativeVariation = reqsPerSampleStdDev[cpuID] / avgReqsPerSample[cpuID];
	if(relativeVariation > requestVariationThreshold){
		return false;
	}

	return true;
}

double
MissBandwidthPolicy::evaluateMHA(std::vector<int> currentMHA){

	// 1. Prune search space
	for(int i=0;i<currentMHA.size();i++){
		if(currentMHA[i] < maxMSHRs){
			if(!doMHAEvaluation(i)){
				traceVerboseVector("Pruning MHA: ", currentMHA);
				return 0.0;
			}
		}
	}

	traceVerboseVector("--- Evaluating MHA: ", currentMHA);

	traceVerboseVector("Request Count Measurement: ", currentMeasurements->requestsInSample);
	traceVerboseVector("Shared Latency Measurement: ", currentMeasurements->sharedLatencies);
	traceVerboseVector("Bus Request Measurement: ", currentMeasurements->busAccessesPerCore);
	traceVerboseVector("Bus Read Measurement: ", currentMeasurements->busReadsPerCore);

	// 2. Estimate new shared memory latencies
	vector<double> sharedLatencyEstimates(cpuCount, 0.0);
	getAverageMemoryLatency(&currentMHA, &sharedLatencyEstimates);
	currentLatencyProjection = sharedLatencyEstimates;

	traceVerboseVector("Shared Latency Projection: ", sharedLatencyEstimates);

	// 3. Compute speedups
	vector<double> speedups(cpuCount, 0.0);
	vector<double> sharedIPCEstimates(cpuCount, 0.0);

	for(int i=0;i<cpuCount;i++){

		double newStallEstimate = estimateStallCycles(currentMeasurements->cpuStallCycles[i],
				                                      mostRecentMWSEstimate[i][caches[i]->getCurrentMSHRCount(true)],
				                                      mostRecentMLPEstimate[i][caches[i]->getCurrentMSHRCount(true)],
													  currentMeasurements->sharedLatencies[i],
													  currentMeasurements->requestsInSample[i],
													  mostRecentMWSEstimate[i][currentMHA[i]],
													  mostRecentMLPEstimate[i][currentMHA[i]],
													  sharedLatencyEstimates[i],
													  currentMeasurements->requestsInSample[i],
													  currentMeasurements->responsesWhileStalled[i]);

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
	tracePerformance(sharedIPCEstimates, speedups);

	// 4. Compute metric
	double metricValue = computeMetric(&speedups);

//	if(dumpSearchSpaceAt == curTick){
//		dumpSearchSpace(mhaConfig, metricValue);
//	}

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

		double tmpFreeBusSlots = currentMeasurements->busAccessesPerCore[i] * (1 - mlpRatio);
		freeBusSlots += tmpFreeBusSlots;

		DPRINTFR(MissBWPolicyExtra, "Request change for CPU %i, MLP ratio %f, request estimate %f, freed bus slots %f\n", i, mlpRatio, newRequestCountEstimates[i], tmpFreeBusSlots);
	}

	// 2. Estimate the response in request demand
	vector<double> additionalBusRequests(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++){

		if(currentMeasurements->busReadsPerCore[i] > busRequestThreshold
		   && currentMHA->at(i) == maxMSHRs){

			double queueRatio = 1.0;
			if(currentMeasurements->privateLatencyBreakdown[i][InterferenceManager::MemoryBusQueue] > 0){
				queueRatio = currentMeasurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue] / currentMeasurements->privateLatencyBreakdown[i][InterferenceManager::MemoryBusQueue];
			}

			additionalBusRequests[i] = ((double) currentMeasurements->busReadsPerCore[i]) * queueRatio;
			DPRINTFR(MissBWPolicyExtra, "CPU %d has queue ratio %f, estimating %f additional bus reqs\n", i, queueRatio, additionalBusRequests[i]);
		}
		else{
			DPRINTFR(MissBWPolicyExtra, "CPU %d has too few requests (%i < %i) or num MSHRs is not increased (new count %d < old count %d)\n",
					i,
					currentMeasurements->busReadsPerCore[i],
					busRequestThreshold,
					currentMHA->at(i),
					caches[i]->getCurrentMSHRCount(true));
		}
	}

	if(currentMeasurements->actualBusUtilization > busUtilizationThreshold && freeBusSlots > 0){
		DPRINTFR(MissBWPolicyExtra, "Bus utilzation is larger than threshold (%f > %f)\n", currentMeasurements->actualBusUtilization, busUtilizationThreshold);
		double addBusReqSum = computeSum(&additionalBusRequests);
		DPRINTFR(MissBWPolicyExtra, "Sum of additional requests of cpus without MSHR reduction is %d\n", addBusReqSum);

		if(addBusReqSum > freeBusSlots){
			for(int i=0;i<cpuCount;i++){
				double additionalReads = freeBusSlots * ((double) additionalBusRequests[i]) / ((double) addBusReqSum);
				newRequestCountEstimates[i] += additionalReads;

				DPRINTFR(MissBWPolicyExtra, "Estimating %f additional reads for CPU %d\n",
						additionalReads,
						i);

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
	for(int i=0;i<cpuCount;i++){
		double currentAvgBusLat = currentMeasurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue];
		double newRequestRatio = ((double) currentMeasurements->requestsInSample[i]) / newRequestCountEstimates[i];
		DPRINTFR(MissBWPolicyExtra, "CPU %d: New request ratio is %f, estimated new request count %f, old request count %d\n", i, newRequestRatio, newRequestCountEstimates[i], currentMeasurements->requestsInSample[i]);
		double newAvgBusLat = newRequestRatio * currentAvgBusLat;
		(*estimatedSharedLatencies)[i] = (currentMeasurements->sharedLatencies[i] - currentAvgBusLat) + newAvgBusLat;
		DPRINTFR(MissBWPolicyExtra, "CPU %i, estimating bus lat to %f, new bus lat %f, new avg lat %f, request ratio %f\n", i, currentAvgBusLat, newAvgBusLat, estimatedSharedLatencies->at(i), newRequestRatio);
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
									     double newRequests,
									     double responsesWhileStalled){

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

//		if(newStallTime > period) return period;
		if(newStallTime < 0) return 0;

		return newStallTime;
	}
	else if(perfEstMethod == LATENCY_MLP_SREQ){

		DPRINTF(MissBWPolicyExtra, "Running stall-request latency MLP method with %f mlp, %f avg shared lat and %f responses while stalled\n",
				              currentMLP,
				              currentAvgSharedLat,
				              responsesWhileStalled);

		double curMLRProduct = currentMLP * currentAvgSharedLat * responsesWhileStalled;

		DPRINTF(MissBWPolicyExtra, "Estimated new values are %f mlp, %f avg shared lat and %f responses while stalled\n",
							  newMLP,
							  newAvgSharedLat,
							  responsesWhileStalled);


		double newMLRProduct = newMLP * newAvgSharedLat * responsesWhileStalled;

		double deltaStallCycles = newMLRProduct - curMLRProduct;
		double newStallTime = currentStallTime + deltaStallCycles;

		DPRINTF(MissBWPolicyExtra, "Current MLR product is %f, new MLR product is %f, current stall time is %f, new stall time %f\n",
							  curMLRProduct,
							  newMLRProduct,
							  currentStallTime,
							  newStallTime);

		if(newStallTime < 0){
			DPRINTF(MissBWPolicy, "Negative stall time (%f), returning 0\n", newStallTime);
			return 0;
		}

		DPRINTF(MissBWPolicyExtra, "Returning new stall time %f\n", newStallTime);
		return newStallTime;
	}
	else if(perfEstMethod == NO_MLP){

		if(currentAvgSharedLat == 0 || currentRequests == 0){
			DPRINTF(MissBWPolicyExtra, "Running no-MLP method, latency or num requests is 0, returning 0\n");
			return 0;
		}

		double computedConstMLP = currentStallTime / (currentAvgSharedLat * currentRequests);

		DPRINTF(MissBWPolicyExtra, "Running no-MLP method with shared lat %f, requests %f and %f stall cycles\n",
      					           currentAvgSharedLat,
      					           currentRequests,
      					           currentStallTime);

		DPRINTF(MissBWPolicyExtra, "Estimated MLP constant to be %f\n", computedConstMLP);


		double adjustedMLP = (computedConstMLP * newMLP) / currentMLP;

		DPRINTF(MissBWPolicyExtra, "Computed adjusted MLP to be %f with current MLP %f and new MLP %f\n",
								    adjustedMLP,
								    currentMLP,
								    newMLP);

		double newStallTime = adjustedMLP * newAvgSharedLat * newRequests;

		DPRINTF(MissBWPolicyExtra, "Estimated new stall time %f with new shared lat %f and new requests %f\n",
							        newStallTime,
							        newAvgSharedLat,
							        newRequests);

		if(newStallTime < 0){
			DPRINTF(MissBWPolicy, "Negative stall time (%f), returning 0\n", newStallTime);
			return 0;
		}

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
														  curReqs,
														  currentMeasurements->responsesWhileStalled[i]);
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

	assert(currentMeasurements == NULL);
	currentMeasurements = &measurements;

	DPRINTF(MissBWPolicy, "--- Running Miss Bandwidth Policy\n");

	for(int i=0;i<requestAccumulator.size();i++){
		requestAccumulator[i] += currentMeasurements->requestsInSample[i];

		double tmpCurMes = (double) currentMeasurements->requestsInSample[i];
		requestSqAccumulator[i] += tmpCurMes*tmpCurMes;
	}
	traceVector("Request accumulator: ", requestAccumulator);
	traceVector("Request square accumulator: ", requestSqAccumulator);

	renewMeasurementsCounter++;
	if(usePersistentAllocations && renewMeasurementsCounter < renewMeasurementsThreshold && renewMeasurementsCounter > 1){
		DPRINTF(MissBWPolicy, "Skipping Miss Bandwidth Policy due to Persistent Allocations, counter is %d, threshold %d\n",
							  renewMeasurementsCounter,
							  renewMeasurementsThreshold);
		traceNumMSHRs();

		if(renewMeasurementsCounter == 2){
			assert(!bestRequestProjection.empty());
			traceBestProjection();

			DPRINTF(MissBWPolicy, "Dumping accuracy information\n");
			traceVector("Request Count Measurement: ", currentMeasurements->requestsInSample);
			traceVector("Shared Latency Measurement: ", currentMeasurements->sharedLatencies);
		}

		currentMeasurements = NULL;
		return;
	}

	traceVector("Request Count Measurement: ", currentMeasurements->requestsInSample);
	traceVector("Shared Latency Measurement: ", currentMeasurements->sharedLatencies);
	traceVector("Committed Instructions: ", currentMeasurements->committedInstructions);
	traceVector("CPU Stall Cycles: ", currentMeasurements->cpuStallCycles);
	vector<double> actualIPC(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++) actualIPC[i] = (double) currentMeasurements->committedInstructions[i] / (double) period;
	traceVector("Measured IPC: ", actualIPC);

	if(measurementsValid){
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

		computeRequestStatistics();
	}
	else{

		if(measurementsValid && !usePersistentAllocations){
			traceBestProjection();
		}

		for(int i=0;i<requestAccumulator.size();i++){
			requestAccumulator[i] = 0;
			requestSqAccumulator[i] = 0;
		}
		traceVector("Request accumulator is now: ", requestAccumulator);
		traceVector("Request square accumulator is now: ", requestSqAccumulator);


		// initalize best-storage
		for(int i=0;i<bestRequestProjection.size();i++) bestRequestProjection[i] = (double) currentMeasurements->requestsInSample[i];
		bestLatencyProjection = currentMeasurements->sharedLatencies;

		vector<int> bestMHA;
		Tick desicionLatency = 0;
		if(searchAlgorithm == EXHAUSTIVE_SEARCH){
			double tmpLat = pow((double) maxMSHRs, (double) cpuCount);
			int tmpIntLat = (int) tmpLat;
			desicionLatency = tmpIntLat * iterationLatency;
			bestMHA = exhaustiveSearch();
		}
		else if(searchAlgorithm == BUS_SORTED){
			desicionLatency = maxMSHRs * cpuCount * iterationLatency;
			bestMHA = busSearch(false);
		}
		else if(searchAlgorithm == BUS_SORTED_LOG){
			desicionLatency = FloorLog2(maxMSHRs) * cpuCount * iterationLatency;
			bestMHA = busSearch(true);
		}
		else{
			fatal("Unknown search algorithm");
		}

		measurementsValid = true;
		if(bestMHA.size() != cpuCount){
			DPRINTF(MissBWPolicy, "All programs have to few requests, reverting to max MSHRs configuration\n");
			bestMHA.resize(cpuCount, maxMSHRs);
		}
		traceVector("Best MHA: ", bestMHA);
		DPRINTF(MissBWPolicy, "Best metric value is %f\n", maxMetricValue);

		double currentMetricValue = computeCurrentMetricValue();
		double benefit = maxMetricValue / currentMetricValue;
		if(benefit > acceptanceThreshold){
			DPRINTF(MissBWPolicy, "Scheduling implementation of new MHA, benefit is %d, acceptance threshold %d, latency %d\n", benefit, acceptanceThreshold, desicionLatency);

			MissBandwidthImplementMHAEvent* implEvent = new MissBandwidthImplementMHAEvent(this, bestMHA);
			implEvent->schedule(curTick + desicionLatency);

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
MissBandwidthPolicy::implementMHA(std::vector<int> bestMHA){
	DPRINTF(MissBWPolicy, "-- Implementing new MHA\n");
	for(int i=0;i<caches.size();i++){
		DPRINTF(MissBWPolicy, "Setting CPU%d MSHRs to %d\n", i , bestMHA[i]);
		caches[i]->setNumMSHRs(bestMHA[i]);
	}
}

double
MissBandwidthPolicy::squareRoot(double num){

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
MissBandwidthPolicy::computeRequestStatistics(){
	for(int i=0;i<requestAccumulator.size();i++){
		double mean = (double) requestAccumulator[i] / (double) (renewMeasurementsThreshold-1);
		avgReqsPerSample[i] = mean;

		double sqmean = (double) requestSqAccumulator[i] / (double) (renewMeasurementsThreshold-1);

		reqsPerSampleStdDev[i] = squareRoot(sqmean - mean*mean);
	}

	traceVector("Average Reqs per Sample: ", avgReqsPerSample);
	traceVector("Reqs per Sample standard deviation: ", reqsPerSampleStdDev);
}

void
MissBandwidthPolicy::initProjectionTrace(int cpuCount){
	vector<string> headers;

	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Estimated Num Requests", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Measured Num Requests", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Estimated Avg Shared Latency", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Measured Avg Shared Latency", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Average Requests per Sample", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Stdev Requests per Sample", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Estimated IPC", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Measured IPC", i));

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

	for(int i=0;i<cpuCount;i++){
		if(doMHAEvaluation(i)) data.push_back(bestRequestProjection[i]);
		else data.push_back(INT_MAX);
	}
	for(int i=0;i<cpuCount;i++){
		if(doMHAEvaluation(i)) data.push_back(currentMeasurements->requestsInSample[i]);
		else data.push_back(INT_MAX);
	}
	for(int i=0;i<cpuCount;i++){
		if(doMHAEvaluation(i)) data.push_back(bestLatencyProjection[i]);
		else data.push_back(INT_MAX);
	}
	for(int i=0;i<cpuCount;i++){
		if(doMHAEvaluation(i)) data.push_back(currentMeasurements->sharedLatencies[i]);
		else data.push_back(INT_MAX);
	}

	for(int i=0;i<cpuCount;i++) data.push_back(avgReqsPerSample[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(reqsPerSampleStdDev[i]);

	for(int i=0;i<cpuCount;i++){
		if(doMHAEvaluation(i)) data.push_back(bestIPCProjection[i]);
		else data.push_back(INT_MAX);
	}

	for(int i=0;i<cpuCount;i++){
		if(doMHAEvaluation(i)){
			double ipc = (double) currentMeasurements->committedInstructions[i] / (double) period;
			data.push_back(ipc);
		}
		else data.push_back(INT_MAX);
	}

	predictionTrace.addTrace(data);
}

std::vector<int>
MissBandwidthPolicy::busSearch(bool onlyPowerOfTwoMSHRs){

	traceVector("Bus accesses per core: ", currentMeasurements->busAccessesPerCore);

	vector<int> processorIDOrder;
	vector<bool> marked(cpuCount, false);
	for(int i=0;i<cpuCount;i++){
		int minval = 0;
		int minIndex = -1;
		for(int j=0;j<cpuCount;j++){
			int tmpReqs = currentMeasurements->busAccessesPerCore[j];
			if(tmpReqs >= minval && !marked[j]){
				minval = tmpReqs;
				minIndex = j;
			}
		}
		assert(minIndex != -1);

		processorIDOrder.push_back(minIndex);
		marked[minIndex] = true;
	}

	traceVector("Search order: ", processorIDOrder);

	vector<int> bestMHA(cpuCount, maxMSHRs);
	maxMetricValue = 0.0;

	for(int i=0;i<processorIDOrder.size();i++){
		int currentTestMSHRs = 1;
		while(currentTestMSHRs <= maxMSHRs){
			vector<int> testMHA = bestMHA;
			testMHA[processorIDOrder[i]] = currentTestMSHRs;

			traceVector("Evaluating MHA: ", testMHA);
			double curMetricValue = evaluateMHA(testMHA);
			if(curMetricValue >= maxMetricValue){
				DPRINTF(MissBWPolicy, "New best config (%f > %f)\n", curMetricValue, maxMetricValue);
				bestMHA[processorIDOrder[i]] = currentTestMSHRs;
				maxMetricValue = curMetricValue;

				updateBestProjections();
			}

			if(onlyPowerOfTwoMSHRs) currentTestMSHRs = currentTestMSHRs << 1;
			else currentTestMSHRs++;
		}
	}

	return bestMHA;
}

void
MissBandwidthPolicy::updateBestProjections(){
	bestRequestProjection = currentRequestProjection;
	bestLatencyProjection = currentLatencyProjection;
	bestIPCProjection = currentIPCProjection;
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
		double metricValue = evaluateMHA(relocateMHA(value));
		if(metricValue >= maxMetricValue){

			if(metricValue > 0) DPRINTFR(MissBWPolicyExtra, "Metric value %f is larger than previous best %f, new best MHA\n", metricValue, maxMetricValue);

			maxMetricValue = metricValue;
			maxMHAConfiguration = *value;

			updateBestProjections();
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
	headers.push_back("Total Requests");
	headers.push_back("Avg Misses while Stalled");
	headers.push_back("Responses while Stalled");

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
		                                         int committedInsts,
		                                         int responsesWhileStalled){

	vector<RequestTraceEntry> data;


	DPRINTF(MissBWPolicy, "-- Running alone IPC estimation trace for CPU %d, %d cycles since last, %d committed insts\n",
			              cpuID,
			              totalCycles,
			              committedInsts);

	comInstModelTraceCummulativeInst[cpuID] += committedInsts;

	data.push_back(comInstModelTraceCummulativeInst[cpuID]);
	data.push_back(totalCycles);
	data.push_back(stallCycles);
	data.push_back(reqs);
	data.push_back(mws);
	data.push_back(responsesWhileStalled);

	if(cpuCount > 1){
		double newStallEstimate = estimateStallCycles(stallCycles,
				                                      mws,
				                                      mlp,
				                                      avgSharedLat,
				                                      reqs,
				                                      mws,
				                                      mlp,
				                                      avgPrivateLatEstimate,
				                                      reqs,
				                                      responsesWhileStalled);

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
MissBandwidthPolicy::parsePerformanceMethod(std::string methodName){

	if(methodName == "latency-mlp") return LATENCY_MLP;
	if(methodName == "ratio-mws") return RATIO_MWS;
	if(methodName == "latency-mlp-sreq") return LATENCY_MLP_SREQ;
	if(methodName == "no-mlp") return NO_MLP;

	fatal("unknown performance estimation method");
	return LATENCY_MLP;
}



MissBandwidthPolicy::SearchAlgorithm
MissBandwidthPolicy::parseSearchAlgorithm(std::string methodName){
	if(methodName == "exhaustive") return EXHAUSTIVE_SEARCH;
	if(methodName == "bus-sorted") return BUS_SORTED;
	if(methodName == "bus-sorted-log") return BUS_SORTED_LOG;

	fatal("unknown search algorithm");
	return EXHAUSTIVE_SEARCH;
}


//void
//MissBandwidthPolicy::dumpSearchSpace(std::vector<int>* mhaConfig, double metricValue){
//
//	const char* filename = "searchSpaceDump.txt";
//
//	if(!dumpInitalized){
//
//		DPRINTF(MissBWPolicy, "-- Dumping search space!\n");
//
//		ofstream initfile(filename, ios_base::out);
//		initfile << "";
//		initfile.close();
//		dumpInitalized = true;
//	}
//
//	ofstream dumpfile(filename, ios_base::app);
//	for(int i=0;i<mhaConfig->size();i++){
//		dumpfile << mhaConfig->at(i)+1 << ";";
//	}
//	dumpfile << metricValue << "\n";
//	dumpfile.close();
//}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("MissBandwidthPolicy", MissBandwidthPolicy);

#endif

