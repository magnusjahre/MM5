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

private:
	int epoch;
	int maxWays;
	int curHighPriCPUID;
	bool doLLCAlloc;
	ASREpochEvent* epochEvent;
	double maximumSpeedup;
	bool manageMemoryBus;

	vector<ASREpochMeasurements> epochMeasurements;
	std::vector<RequestTrace> asmTraces;
	RequestTrace allocationTrace;
	RequestTrace bandwidthTrace;
	std::vector<RequestTrace> speedupCurveTraces;

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
	void setEpochProbabilities(std::vector<vector<double> > speedups, std::vector<int> llcQuotas);

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
			  bool _doLLCAlloc,
			  double _maximumSpeedup,
			  bool _manageMemoryBus);

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
