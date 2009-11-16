/*
 * miss_bandwidth_policy.hh
 *
 *  Created on: Oct 28, 2009
 *      Author: jahre
 */

#include "sim/sim_object.hh"
#include "sim/eventq.hh"
#include "mem/interference_manager.hh"
#include "mem/requesttrace.hh"
#include "mem/cache/base_cache.hh"
#include "mem/cache/miss/policy/performance_measurement.hh"

#ifndef MISS_BANDWIDTH_POLICY_HH_
#define MISS_BANDWIDTH_POLICY_HH_

class MissBandwidthPolicyEvent;
class InterferenceManager;
class BaseCache;

class MissBandwidthPolicy : public SimObject{

protected:
	InterferenceManager* intManager;
	Tick period;
	MissBandwidthPolicyEvent* policyEvent;

	RequestTrace measurementTrace;
	std::vector<BaseCache* > caches;

	PerformanceMeasurement* currentMeasurements;

	int maxMSHRs;
	int cpuCount;

	double requestCountThreshold;
	double busUtilizationThreshold;

	void getAverageMemoryLatency(std::vector<int>* currentMHA,
							     std::vector<double>* estimatedSharedLatencies,
							     std::vector<double>* estimatedNewRequestCount);

	int level;
	double maxMetricValue;
	std::vector<int> maxMHAConfiguration;
	std::vector<int> exhaustiveSearch();
	void recursiveExhaustiveSearch(std::vector<int>* value, int k);

	std::vector<int> relocateMHA(std::vector<int>* mhaConfig);

	double evaluateMHA(std::vector<int>* mhaConfig);

	template<class T>
	T computeSum(std::vector<T>* values);

	template<class T>
	std::vector<double> computePercetages(std::vector<T>* values);

public:
	MissBandwidthPolicy(std::string _name, InterferenceManager* _intManager, Tick _period, int _cpuCount, double _busUtilThreshold, double _cutoffReqInt);

	~MissBandwidthPolicy();

	void handlePolicyEvent();

	void registerCache(BaseCache* _cache, int _cpuID, int _maxMSHRs);

	void addTraceEntry(PerformanceMeasurement* measurement);

	void runPolicy(PerformanceMeasurement measurements);

	virtual double computeMetric(std::vector<double>* speedups) = 0;


};

class MissBandwidthPolicyEvent : public Event{
private:
	MissBandwidthPolicy* policy;
	Tick period;

public:
	MissBandwidthPolicyEvent(MissBandwidthPolicy* _policy, Tick _period):
	Event(&mainEventQueue), policy(_policy), period(_period){

	}
	void process() {
		policy->handlePolicyEvent();
		schedule(curTick + period);
	}

	virtual const char *description() {
		return "Miss Bandwidth Policy Event";
	}

};


#endif /* MISS_BANDWIDTH_POLICY_HH_ */
