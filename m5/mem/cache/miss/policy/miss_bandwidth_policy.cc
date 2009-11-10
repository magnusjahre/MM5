/*
 * miss_bandwidth_policy.cc
 *
 *  Created on: Oct 28, 2009
 *      Author: jahre
 */

#include "miss_bandwidth_policy.hh"

#include <cmath>

using namespace std;

MissBandwidthPolicy::MissBandwidthPolicy(string _name, InterferenceManager* _intManager, Tick _period, int _cpuCount)
: SimObject(_name){

	intManager = _intManager;

	intManager->registerMissBandwidthPolicy(this);

	period = _period;
	policyEvent = new MissBandwidthPolicyEvent(this, period);
	policyEvent->schedule(period);

	measurementTrace = RequestTrace(_name, "MeasurementTrace");

	cpuCount = _cpuCount;
	caches.resize(cpuCount, 0);

	level = 0;
	maxMetricValue = 0;
}

MissBandwidthPolicy::~MissBandwidthPolicy(){
	assert(!policyEvent->scheduled());
	delete policyEvent;
}

void
MissBandwidthPolicy::registerCache(BaseCache* _cache, int _cpuID, int _maxMSHRs){
	assert(caches[_cpuID] == NULL);
	caches[_cpuID] = _cache;
	maxMSHRs = _maxMSHRs;
}

void
MissBandwidthPolicy::handlePolicyEvent(){
	intManager->buildInterferenceMeasurement();
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
MissBandwidthPolicy::getAverageMemoryLatency(int cpuID, int numMSHRs, PerformanceMeasurement* measurements){
	// TODO: implement estimation model
	return measurements->sharedLatencies[cpuID];
}

void
MissBandwidthPolicy::runPolicy(PerformanceMeasurement measurements){
	addTraceEntry(&measurements);

	vector<double> aloneCycles(cpuCount, 0);
	vector<double> nonMemoryCycles(cpuCount, 0);

	for(int i=0;i<cpuCount;i++){
		double interference = measurements.sharedLatencies[i] - measurements.estimatedPrivateLatencies[i];
		double totalInterferenceCycles = interference * measurements.requestsInSample[i];
		aloneCycles[i] = period - (measurements.mlpEstimate[i][maxMSHRs] * totalInterferenceCycles);

		double totalLatencyCycles = measurements.sharedLatencies[i] * measurements.requestsInSample[i];
		int currentMSHRs = caches[i]->getCurrentMSHRCount(true);
		nonMemoryCycles[i] = period - (measurements.mlpEstimate[i][currentMSHRs] * totalLatencyCycles);
	}

	vector<vector<double> > speedups(cpuCount, vector<double>(maxMSHRs+1, 0.0));

	for(int i=0;i<cpuCount;i++){
		for(int j=1;j<=maxMSHRs;j++){
			double totalLatencyEstimate = getAverageMemoryLatency(i,j, &measurements) * measurements.requestsInSample[i];
			double memStallEstimate = measurements.mlpEstimate[i][j] * totalLatencyEstimate;
			double sharedCycleEstimate = nonMemoryCycles[i] + memStallEstimate;

			speedups[i][j] = aloneCycles[i] / sharedCycleEstimate;
		}
	}

	vector<int> bestMHA = exhaustiveSearch(&speedups);

	for(int i=0;i<caches.size();i++) caches[i]->setNumMSHRs(bestMHA[i]);
}

std::vector<int>
MissBandwidthPolicy::exhaustiveSearch(std::vector<std::vector<double> >* speedups){

	vector<int> value(cpuCount, 0);
	level = -1;
	maxMetricValue = 0.0;

	recursiveExhaustiveSearch(&value, 0, speedups);

	return relocateMHA(&maxMHAConfiguration);
}

void
MissBandwidthPolicy::recursiveExhaustiveSearch(std::vector<int>* value, int k, std::vector<std::vector<double> >* speedups){

	assert(value->size() == cpuCount);

	level = level+1;

	if(level >= 1){
		assert(level-1 >= 0 && level-1 < value->size());
		(*value)[level-1] = k;
	}

	if(level == cpuCount){
		double metricValue = computeMetric(value, speedups);
		if(metricValue >= maxMetricValue){
			maxMetricValue = metricValue;
			maxMHAConfiguration = *value;
		}
	}
	else{
		for(int i=0;i<maxMSHRs;i++){
			recursiveExhaustiveSearch(value, i, speedups);
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

vector<double>
MissBandwidthPolicy::retrieveSpeedups(vector<int>* mhaConfig, vector<vector<double> >* speedups){
	vector<double> speedupsForMHA(mhaConfig->size(), 0.0);

	vector<int> configCopy = relocateMHA(mhaConfig);

 	for(int i=0;i<configCopy.size();i++){
		speedupsForMHA[i] = (*speedups)[i][configCopy[i]];
	}

	return speedupsForMHA;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("MissBandwidthPolicy", MissBandwidthPolicy);

#endif

