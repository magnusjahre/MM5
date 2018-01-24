/*
 * asr.hh
 *
 *  Created on: Feb 24, 2017
 *      Author: jahre
 */

#ifndef MEM_POLICY_ASM_POLICY_HH_
#define MEM_POLICY_ASM_POLICY_HH_

#include "base_policy.hh"

class ASREpochEvent;

class ASMPolicy : public BasePolicy{

public:
	enum ASMSubpolicy{
		ASM_MEM,
		ASM_MEM_EQUAL,
		ASM_CACHE,
		ASM_CACHE_MEM
	};

private:
	int epoch;
	int maxWays;
	int curHighPriCPUID;
	ASREpochEvent* epochEvent;
	double maximumSpeedup;
	ASMSubpolicy asmPolicy;

	vector<ASREpochMeasurements> epochMeasurements;
	std::vector<RequestTrace> asmTraces;
	RequestTrace allocationTrace;
	RequestTrace bandwidthTrace;
	std::vector<RequestTrace> slowdownCurveTraces;

	vector<int> curAllocation;
	vector<double> avgLLCMissAdditionalCycles;
	vector<double> CARshared;
	vector<double> CARalone;
	vector<double> epochCumProbDistrib;

	void changeHighPriProcess();

	void prepareASMTraces(int numCPUs);
	void traceASMValues(std::vector<ASMValues> values);
	void initCurveTracefiles();

	void updateProbabilityDistribution(std::vector<double> probabilities);
	void setEpochProbabilities(std::vector<vector<double> > slowdowns, std::vector<int> llcQuotas);

public:

	ASMPolicy(std::string _name,
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
			  int _epoch,
			  ASMSubpolicy _subpol,
			  double _maximumSpeedup);

	virtual void initPolicy();

	virtual void runPolicy(PerformanceMeasurement measurements);

	virtual bool doEvaluation(int cpuID);

	void handleEpochEvent();

	virtual void prepareEstimates();
};

class ASREpochEvent : public Event{
private:
	ASMPolicy* policy;
	Tick epoch;

public:
	ASREpochEvent(ASMPolicy* _policy, Tick _epoch):
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

#endif /* MEM_POLICY_ASM_POLICY_HH_ */
