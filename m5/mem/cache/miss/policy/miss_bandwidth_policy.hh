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

#ifndef MISS_BANDWIDTH_POLICY_HH_
#define MISS_BANDWIDTH_POLICY_HH_

class MissBandwidthPolicyEvent;
class InterferenceManager;
class PerformanceMeasurement;

class MissBandwidthPolicy : public SimObject{

protected:
	InterferenceManager* intManager;
	Tick period;
	MissBandwidthPolicyEvent* policyEvent;

	RequestTrace measurementTrace;

public:
	MissBandwidthPolicy(std::string _name, InterferenceManager* _intManager, Tick _period);

	~MissBandwidthPolicy();

	void handlePolicyEvent();

	void addTraceEntry(PerformanceMeasurement* measurement);

	virtual void runPolicy(PerformanceMeasurement measurements) = 0;


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