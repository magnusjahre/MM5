/*
 * equalize_slowdown.cc
 *
 *  Created on: Jan 15, 2016
 *      Author: jahre
 */

#include "equalize_slowdown_policy.hh"

using namespace std;

EqualizeSlowdownPolicy::EqualizeSlowdownPolicy(std::string _name,
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
											   int _hybridBufferSize)
: BasePolicy(_name,
			_intManager,
			_period,
			_cpuCount,
			_perfEstMethod,
			_persistentAllocations,
			_iterationLatency,
			_performanceMetric,
			_enforcePolicy,
			_wst,
			_pbst,
			_rst,
			_maximumDamping,
			_hybridDecisionError,
			_hybridBufferSize){


	cout << "hello worle\n";

}

void
EqualizeSlowdownPolicy::runPolicy(PerformanceMeasurement measurements){
	fatal("runPolicy not implemented");
}

bool
EqualizeSlowdownPolicy::doEvaluation(int cpuID){
	return true;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(EqualizeSlowdownPolicy)
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<Tick> period;
	Param<int> cpuCount;
	Param<string> performanceEstimationMethod;
	Param<bool> persistentAllocations;
	Param<int> iterationLatency;
	Param<string> optimizationMetric;
	Param<bool> enforcePolicy;
	Param<string> writeStallTechnique;
	Param<string> privateBlockedStallTechnique;
	Param<string> emptyROBStallTechnique;
	Param<double> maximumDamping;
	Param<double> hybridDecisionError;
	Param<int> hybridBufferSize;
END_DECLARE_SIM_OBJECT_PARAMS(EqualizeSlowdownPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(EqualizeSlowdownPolicy)
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1048576),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true),
	INIT_PARAM_DFLT(iterationLatency, "The number of cycles it takes to evaluate one MHA", 0),
	INIT_PARAM_DFLT(optimizationMetric, "The metric to optimize for", "hmos"),
	INIT_PARAM_DFLT(enforcePolicy, "Should the policy be enforced?", true),
	INIT_PARAM(writeStallTechnique, "The technique to use to estimate private write stalls"),
	INIT_PARAM(privateBlockedStallTechnique, "The technique to use to estimate private blocked stalls"),
	INIT_PARAM(emptyROBStallTechnique, "The technique to use to estimate private mode empty ROB stalls"),
	INIT_PARAM_DFLT(maximumDamping, "The maximum absolute damping the damping policies can apply", 0.25),
	INIT_PARAM_DFLT(hybridDecisionError, "The error at which to switch from CPL to CPL-CWP with the hybrid scheme", 0.0),
	INIT_PARAM_DFLT(hybridBufferSize, "The number of errors to use in the decision buffer", 3)
END_INIT_SIM_OBJECT_PARAMS(EqualizeSlowdownPolicy)

CREATE_SIM_OBJECT(EqualizeSlowdownPolicy)
{
	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	BasePolicy::WriteStallTechnique wst = BasePolicy::parseWriteStallTech(writeStallTechnique);
	BasePolicy::PrivBlockedStallTechnique pbst = BasePolicy::parsePrivBlockedStallTech(privateBlockedStallTechnique);

	BasePolicy::EmptyROBStallTechnique rst = BasePolicy::parseEmptyROBStallTech(emptyROBStallTechnique);

	return new EqualizeSlowdownPolicy(getInstanceName(),
							          interferenceManager,
									  period,
									  cpuCount,
									  perfEstMethod,
									  persistentAllocations,
									  iterationLatency,
									  performanceMetric,
									  enforcePolicy,
									  wst,
									  pbst,
									  rst,
									  maximumDamping,
									  hybridDecisionError,
									  hybridBufferSize);
}

REGISTER_SIM_OBJECT("EqualizeSlowdownPolicy", EqualizeSlowdownPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS

