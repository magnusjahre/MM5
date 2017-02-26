/*
 * asr.hh
 *
 *  Created on: Feb 24, 2017
 *      Author: jahre
 */

#ifndef MEM_POLICY_ASR_POLICY_HH_
#define MEM_POLICY_ASR_POLICY_HH_

#include "base_policy.hh"

class ASREpochEvent;

class ASRPolicy : public BasePolicy{

private:
	int epoch;
	int maxWays;
	int curHighPriCPUID;
	ASREpochEvent* epochEvent;

	vector<ASREpochMeasurements> epochMeasurements;

	void changeHighPriProcess();

public:
	ASRPolicy(std::string _name,
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
			  int _hybridBufferSize,
			  int _epoch);

	virtual void initPolicy();

	virtual void runPolicy(PerformanceMeasurement measurements);

	virtual bool doEvaluation(int cpuID);

	void handleEpochEvent();
};

class ASREpochEvent : public Event{
private:
	ASRPolicy* policy;
	Tick epoch;

public:
	ASREpochEvent(ASRPolicy* _policy, Tick _epoch):
	Event(&mainEventQueue), policy(_policy), epoch(_epoch){

	}

	void process() {
		policy->handleEpochEvent();
		schedule(curTick + epoch);
	}

	virtual const char *description() {
		return "ASR Policy Epoch  Event";
	}

};

#endif /* MEM_POLICY_ASR_POLICY_HH_ */
