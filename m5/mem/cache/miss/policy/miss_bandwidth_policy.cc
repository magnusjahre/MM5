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

	cpuCount = _cpuCount;
	caches.resize(cpuCount, 0);

	cummulativeMemoryRequests.resize(_cpuCount, 0);

	aloneCycleEstimates.resize(_cpuCount, 0.0);

	level = 0;
	maxMetricValue = 0;

	currentMeasurements = NULL;

	initProjectionTrace(_cpuCount);
	initPartialMeasurementTrace(_cpuCount);
	initAloneIPCTrace(_cpuCount, _enforcePolicy);
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
	traceAloneIPC(curMeasurement.requestsInSample, curMeasurement.committedInstructions);
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
	}
	else{
		for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Alone IPC Measurement", i));
	}

	aloneIPCTrace.initalizeTrace(headers);
}

template<class T>
void
MissBandwidthPolicy::traceAloneIPC(std::vector<int> memoryRequests, std::vector<T> committedInsts){
	vector<RequestTraceEntry> data;

	assert(memoryRequests.size() == cpuCount);
	assert(committedInsts.size() == cpuCount);

	for(int i=0;i<cpuCount;i++){
		cummulativeMemoryRequests[i] += memoryRequests[i];
	}

	for(int i=0;i<cpuCount;i++) data.push_back(cummulativeMemoryRequests[i]);
	for(int i=0;i<cpuCount;i++) data.push_back((double) committedInsts[i] / (double) period);

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

double
MissBandwidthPolicy::evaluateMHA(std::vector<int>* mhaConfig){
	vector<int> currentMHA = relocateMHA(mhaConfig);

	// 1. Estimate new shared memory latencies
	vector<double> sharedLatencyEstimates(cpuCount, 0.0);
	vector<double> newRequestCountEstimates(cpuCount, 0.0);
	getAverageMemoryLatency(&currentMHA, &sharedLatencyEstimates, &newRequestCountEstimates);

	currentLatencyProjection = sharedLatencyEstimates;
	currentRequestProjection = newRequestCountEstimates;

	// 2. Compute speedups
	vector<double> speedups(cpuCount, 0.0);

	for(int i=0;i<cpuCount;i++){

		double totalLatencyEstimate = sharedLatencyEstimates[i] * newRequestCountEstimates[i];
		double memStallEstimate = currentMeasurements->mlpEstimate[i][currentMHA[i]] * totalLatencyEstimate;
		double sharedCycleEstimate = currentMeasurements->getNonMemoryCycles(i, caches[i]->getCurrentMSHRCount(true), period) + memStallEstimate;

		if(caches[i]->getCurrentMSHRCount(true) == maxMSHRs){
			double tmpAloneEstimate = currentMeasurements->getAloneCycles(i, period);
			if(tmpAloneEstimate > 0){
				aloneCycleEstimates[i] = tmpAloneEstimate;
			}
			else{
				aloneEstimationFailed[i]++;
			}
		}
		speedups[i] = aloneCycleEstimates[i] / sharedCycleEstimate;
	}

	currentSpeedupProjection = speedups;

	// 3. Compute metric
	double metricValue = computeMetric(&speedups);

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
											 vector<double>* estimatedSharedLatencies,
											 vector<double>* estimatedNewRequestCount){

	assert(estimatedSharedLatencies->size() == cpuCount);
	assert(estimatedNewRequestCount->size() == cpuCount);
	assert(currentMeasurements != NULL);

	// 1. Estimate change in request count due change in MLP
	double freeBusSlots = 0;
	for(int i=0;i<cpuCount;i++){
		double mlpRatio = 0.0;
		if(currentMeasurements->mlpEstimate[i][currentMHA->at(i)] == 0){
			mlpRatio = 1;
		}
		else{
			mlpRatio = currentMeasurements->mlpEstimate[i][caches[i]->getCurrentMSHRCount(true)] / currentMeasurements->mlpEstimate[i][currentMHA->at(i)];
		}
		(*estimatedNewRequestCount)[i] = ((double) currentMeasurements->requestsInSample[i]) - (((double) currentMeasurements->requestsInSample[i]) * mlpRatio);

		freeBusSlots += currentMeasurements->sharedCacheMissRate * (((double) currentMeasurements->requestsInSample[i]) - estimatedNewRequestCount->at(i));
	}

	// 2. Estimate the response in request demand
	vector<double> additionalBusRequests(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++){
		double busRequests = currentMeasurements->sharedCacheMissRate * (double) currentMeasurements->requestsInSample[i];
		if(busRequests > requestCountThreshold){
			double queueRatio = 1.0;
			if(currentMeasurements->privateLatencyBreakdown[i][InterferenceManager::MemoryBusQueue] > 0){
				queueRatio = currentMeasurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue] / currentMeasurements->privateLatencyBreakdown[i][InterferenceManager::MemoryBusQueue];
			}
			double additionalRequests = queueRatio * currentMeasurements->requestsInSample[i];
			additionalBusRequests[i] = additionalRequests * currentMeasurements->sharedCacheMissRate;
		}
	}

	vector<double> currentRequestDistribution = computePercetages(&(currentMeasurements->requestsInSample));
	if(currentMeasurements->actualBusUtilization > busUtilizationThreshold && freeBusSlots > 0){
		double addBusReqSum = computeSum(&additionalBusRequests);
		if(addBusReqSum > freeBusSlots){
			for(int i=0;i<cpuCount;i++){
				(*estimatedNewRequestCount)[i] += freeBusSlots * (double) currentRequestDistribution[i];
			}
		}
		else{
			for(int i=0;i<cpuCount;i++){
				(*estimatedNewRequestCount)[i] += additionalBusRequests[i];
			}
		}
	}

	// 3. Estimate the new average request latency
	double oldTotalRequests = 0.0;
	double newTotalRequests = 0.0;
	for(int i=0;i<cpuCount;i++){
		oldTotalRequests += currentMeasurements->sharedCacheMissRate * currentMeasurements->requestsInSample[i];
		newTotalRequests += currentMeasurements->sharedCacheMissRate * estimatedNewRequestCount->at(i);
	}

	double requestRatio = newTotalRequests / oldTotalRequests;

	for(int i=0;i<cpuCount;i++){
		double currentAvgBusLat = currentMeasurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue];
		double newAvgBusLat = requestRatio * currentAvgBusLat;
		(*estimatedSharedLatencies)[i] = (currentMeasurements->sharedLatencies[i] - currentAvgBusLat) + newAvgBusLat;
	}

}

double
MissBandwidthPolicy::computeError(double estimate, double actual){
	if(actual == 0) return 0;
	double relErr = (estimate - actual) / actual;
	return relErr*100;
}

void
MissBandwidthPolicy::runPolicy(PerformanceMeasurement measurements){
	addTraceEntry(&measurements);
	assert(currentMeasurements == NULL);
	currentMeasurements = &measurements;
	tracePartialMeasurements();

	if(!bestRequestProjection.empty()){
		for(int i=0;i<cpuCount;i++){
			double reqError = computeError(bestRequestProjection[i], measurements.requestsInSample[i]);
			requestRelError[i].sample(reqError);
			requestAbsError[i].sample(bestRequestProjection[i] - measurements.requestsInSample[i]);

			double latError = computeError(bestLatencyProjection[i], measurements.sharedLatencies[i]);
			sharedLatencyRelError[i].sample(latError);
			sharedLatencyAbsError[i].sample(bestLatencyProjection[i] - measurements.sharedLatencies[i]);
		}
	}

	vector<int> bestMHA = exhaustiveSearch();
	assert(bestMHA.size() == cpuCount);

	traceBestProjection(bestMHA);

	for(int i=0;i<caches.size();i++) caches[i]->setNumMSHRs(bestMHA[i]);

	traceAloneIPC(measurements.requestsInSample, aloneCycleEstimates);

	currentMeasurements = NULL;
}

void
MissBandwidthPolicy::initProjectionTrace(int cpuCount){
	vector<string> headers;

	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Num MSHRs", i));
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
MissBandwidthPolicy::traceBestProjection(vector<int> bestMHA){

	vector<RequestTraceEntry> data;

	for(int i=0;i<cpuCount;i++) data.push_back(bestMHA[i]);
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

