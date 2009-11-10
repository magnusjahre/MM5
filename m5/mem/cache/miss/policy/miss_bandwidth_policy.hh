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

#ifndef MISS_BANDWIDTH_POLICY_HH_
#define MISS_BANDWIDTH_POLICY_HH_

class MissBandwidthPolicyEvent;
class InterferenceManager;
class PerformanceMeasurement;
class BaseCache;

class MissBandwidthPolicy : public SimObject{

protected:
	InterferenceManager* intManager;
	Tick period;
	MissBandwidthPolicyEvent* policyEvent;

	RequestTrace measurementTrace;
	std::vector<BaseCache* > caches;

	int maxMSHRs;
	int cpuCount;

	double getAverageMemoryLatency(int cpuID, int numMSHRs, PerformanceMeasurement* measurements);

	int level;
	double maxMetricValue;
	std::vector<int> maxMHAConfiguration;
	std::vector<int> exhaustiveSearch(std::vector<std::vector<double> >* speedups);
	void recursiveExhaustiveSearch(std::vector<int>* value, int k, std::vector<std::vector<double> >* speedups);

	std::vector<int> relocateMHA(std::vector<int>* mhaConfig);
	std::vector<double> retrieveSpeedups(std::vector<int>* mhaConfig, std::vector<std::vector<double> >* speedups);

public:
	MissBandwidthPolicy(std::string _name, InterferenceManager* _intManager, Tick _period, int _cpuCount);

	~MissBandwidthPolicy();

	void handlePolicyEvent();

	void registerCache(BaseCache* _cache, int _cpuID, int _maxMSHRs);

	void addTraceEntry(PerformanceMeasurement* measurement);

	void runPolicy(PerformanceMeasurement measurements);


  virtual double computeMetric(std::vector<int>* mhaConfig, std::vector<std::vector<double> >* speedups) = 0;


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
