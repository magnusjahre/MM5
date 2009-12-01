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
									     bool _enforcePolicy)
: SimObject(_name){

	intManager = _intManager;

	busUtilizationThreshold = _busUtilThreshold;
	requestCountThreshold = _cutoffReqInt * _period;

	acceptanceThreshold = 0.0; // TODO: parameterize

	renewMeasurementsThreshold = 10; // TODO: parameterize
	renewMeasurementsCounter = 0;

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
	partialMeasurementTrace = RequestTrace(_name, "PartialMeasurementTrace", 1);
	aloneIPCTrace = RequestTrace(_name, "AloneIPCTrace", 1);
	numMSHRsTrace = RequestTrace(_name, "NumMSHRsTrace", 1);

	cpuCount = _cpuCount;
	caches.resize(cpuCount, 0);

	cummulativeMemoryRequests.resize(_cpuCount, 0);

	aloneIPCEstimates.resize(_cpuCount, 0.0);
	avgLatencyAloneIPCModel.resize(_cpuCount, 0.0);

	level = 0;
	maxMetricValue = 0;

	currentMeasurements = NULL;

	mostRecentMWSEstimate.resize(cpuCount, vector<double>());
	mostRecentMLPEstimate.resize(cpuCount, vector<double>());

	initProjectionTrace(_cpuCount);
	initPartialMeasurementTrace(_cpuCount);
	initAloneIPCTrace(_cpuCount, _enforcePolicy);
	initNumMSHRsTrace(_cpuCount);
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
	PerformanceMeasurement curMeasurement = intManager->buildInterferenceMeasurement();
	runPolicy(curMeasurement);
}

void
MissBandwidthPolicy::handleTraceEvent(){
	PerformanceMeasurement curMeasurement = intManager->buildInterferenceMeasurement();
	vector<double> measuredIPC(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++){
		measuredIPC[i] = (double) curMeasurement.committedInstructions[i] / (double) period;
	}
	traceAloneIPC(curMeasurement.requestsInSample, measuredIPC);
}

void
MissBandwidthPolicy::initAloneIPCTrace(int cpuCount, bool policyEnforced){
	vector<string> headers;

	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Requests", i));

	if(cpuCount > 1){
		if(policyEnforced){
			for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Alone IPC Estimate", i));
		}
		else{
			for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Shared IPC Measurement", i));
		}
		for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Avg Latency Based Alone IPC Estimate", i));
	}
	else{
		for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Alone IPC Measurement", i));
	}

	aloneIPCTrace.initalizeTrace(headers);
}

void
MissBandwidthPolicy::traceAloneIPC(std::vector<int> memoryRequests, std::vector<double> ipcs){
	vector<RequestTraceEntry> data;

	assert(memoryRequests.size() == cpuCount);
	assert(ipcs.size() == cpuCount);

	for(int i=0;i<cpuCount;i++){
		cummulativeMemoryRequests[i] += memoryRequests[i];
	}

	for(int i=0;i<cpuCount;i++) data.push_back(cummulativeMemoryRequests[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(ipcs[i]);

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
	for(int i=0;i<cpuCount;i++){

		double newStallEstimate = estimateStallCycles(currentMeasurements->cpuStallCycles[i],
				                                      mostRecentMWSEstimate[i][caches[i]->getCurrentMSHRCount(true)],
													  currentMeasurements->sharedLatencies[i],
													  mostRecentMWSEstimate[i][currentMHA[i]],
													  sharedLatencyEstimates[i]);

		sharedIPCEstimates[i]= (double) currentMeasurements->committedInstructions[i] / (currentMeasurements->getNonStallCycles(i, period) + newStallEstimate);
		speedups[i] = computeSpeedup(sharedIPCEstimates[i], i);

		DPRINTFR(MissBWPolicyExtra, "CPU %d, estimating new stall time %f, new mws %f, current stalled %d\n",
									i,
									newStallEstimate,
									mostRecentMWSEstimate[i][currentMHA[i]],
									currentMeasurements->cpuStallCycles[i]);
	}

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


void
MissBandwidthPolicy::getAverageMemoryLatency(vector<int>* currentMHA,
											 vector<double>* estimatedSharedLatencies){

	assert(estimatedSharedLatencies->size() == cpuCount);
	assert(currentMeasurements != NULL);

	vector<double> newRequestCountEstimates(cpuCount, 0.0);

	// 1. Estimate change in request count due change in MLP
	double freeBusSlots = 0;
	for(int i=0;i<cpuCount;i++){
		double mlpRatio = 0.0;
		if(mostRecentMWSEstimate[i][currentMHA->at(i)] == 0){
			mlpRatio = 1;
		}
		else{
			assert(mostRecentMWSEstimate[i][caches[i]->getCurrentMSHRCount(true)] > 0);
			mlpRatio = mostRecentMWSEstimate[i][currentMHA->at(i)] / mostRecentMWSEstimate[i][caches[i]->getCurrentMSHRCount(true)];
		}

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

	double requestRatio = newTotalRequests / oldTotalRequests;

	DPRINTFR(MissBWPolicyExtra, "New request ratio is %f, estimated new request count %f, old request count %f\n", requestRatio, newTotalRequests, oldTotalRequests);

	for(int i=0;i<cpuCount;i++){
		double currentAvgBusLat = currentMeasurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue];
		double newAvgBusLat = requestRatio * currentAvgBusLat;
		(*estimatedSharedLatencies)[i] = (currentMeasurements->sharedLatencies[i] - currentAvgBusLat) + newAvgBusLat;
		DPRINTFR(MissBWPolicyExtra, "CPU %i, estimating bus lat to %f, new bus lat %f, new avg lat %f\n", i, currentAvgBusLat, newAvgBusLat, estimatedSharedLatencies->at(i));
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
											  double currentAvgSharedLat,
											  double newMWS,
											  double newAvgSharedLat){

	if(currentMWS == 0 || currentAvgSharedLat == 0 || newMWS == 0 || newAvgSharedLat == 0) return currentStallTime;

	double currentRatio = currentAvgSharedLat / currentMWS;
	double newRatio = newAvgSharedLat / newMWS;

	double stallTimeFactor = newRatio / currentRatio;

	return currentStallTime * stallTimeFactor;
}

void
MissBandwidthPolicy::updateAloneIPCEstimate(){
	for(int i=0;i<cpuCount;i++){
		if(caches[i]->getCurrentMSHRCount(true) == maxMSHRs){
			double stallParallelism = currentMeasurements->avgMissesWhileStalled[i][maxMSHRs];
			double newStallEstimate = estimateStallCycles(currentMeasurements->cpuStallCycles[i],
														  stallParallelism,
														  currentMeasurements->sharedLatencies[i],
														  stallParallelism,
														  currentMeasurements->estimatedPrivateLatencies[i]);
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

	traceVector("Request Count Measurement: ", currentMeasurements->requestsInSample);
	traceVector("Shared Latency Measurement: ", currentMeasurements->sharedLatencies);
	traceVector("Committed Instructions: ", currentMeasurements->committedInstructions);
	traceVector("CPU Stall Cycles: ", currentMeasurements->cpuStallCycles);
	vector<double> actualIPC(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++) actualIPC[i] = (double) currentMeasurements->committedInstructions[i] / (double) period;
	traceVector("Measured IPC: ", actualIPC);


	tracePartialMeasurements();

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
		renewMeasurementsCounter++;

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

		traceAloneIPC(measurements.requestsInSample, aloneIPCEstimates);

		traceBestProjection();
	}

	traceNumMSHRs();

	currentMeasurements = NULL;
}

void
MissBandwidthPolicy::initProjectionTrace(int cpuCount){
	vector<string> headers;

	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Speedup", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Avg Shared Latency", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Num Requests", i));

	predictionTrace.initalizeTrace(headers);
}

void
MissBandwidthPolicy::initPartialMeasurementTrace(int cpuCount){
	vector<string> headers;

	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("IPC", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Avg Shared Latency", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Num Requests", i));

	partialMeasurementTrace.initalizeTrace(headers);
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

	for(int i=0;i<cpuCount;i++) data.push_back(bestSpeedupProjection[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(bestLatencyProjection[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(bestRequestProjection[i]);

	predictionTrace.addTrace(data);
}

void
MissBandwidthPolicy::tracePartialMeasurements(){
	vector<RequestTraceEntry> data;

	for(int i=0;i<cpuCount;i++) data.push_back((double) currentMeasurements->committedInstructions[i] / (double) period);
	for(int i=0;i<cpuCount;i++) data.push_back(currentMeasurements->sharedLatencies[i]);
	for(int i=0;i<cpuCount;i++) data.push_back(currentMeasurements->requestsInSample[i]);

	partialMeasurementTrace.addTrace(data);
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

			bestSpeedupProjection = currentSpeedupProjection;
			bestLatencyProjection = currentLatencyProjection;
			bestRequestProjection = currentRequestProjection;
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("MissBandwidthPolicy", MissBandwidthPolicy);

#endif

