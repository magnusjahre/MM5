/*
 * miss_bandwidth_policy.cc
 *
 *  Created on: Oct 28, 2009
 *      Author: jahre
 */

#include "miss_bandwidth_policy.hh"

using namespace std;

MissBandwidthPolicy::MissBandwidthPolicy(string _name, InterferenceManager* _intManager, Tick _period)
: SimObject(_name){

	intManager = _intManager;

	intManager->registerMissBandwidthPolicy(this);

	period = _period;
	policyEvent = new MissBandwidthPolicyEvent(this, period);
	policyEvent->schedule(period);

	measurementTrace = RequestTrace(_name, "MeasurementTrace");

	cpuCount = _intManager->getCPUCount();
	caches.resize(cpuCount, 0);
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

void
MissBandwidthPolicy::runPolicy(PerformanceMeasurement measurements){
	cout << curTick << ": run policy called but not implemented\n";
	addTraceEntry(&measurements);

//	caches[0]->setNumMSHRs(1);
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("MissBandwidthPolicy", MissBandwidthPolicy);

#endif

