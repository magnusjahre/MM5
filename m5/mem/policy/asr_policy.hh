/*
 * asr.hh
 *
 *  Created on: Feb 24, 2017
 *      Author: jahre
 */

#ifndef MEM_POLICY_ASR_POLICY_HH_
#define MEM_POLICY_ASR_POLICY_HH_

#include "base_policy.hh"

class ASRPolicy : public BasePolicy{

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
			  int _hybridBufferSize);

	virtual void init();

	virtual void runPolicy(PerformanceMeasurement measurements);

	virtual bool doEvaluation(int cpuID);
};


#endif /* MEM_POLICY_ASR_POLICY_HH_ */
