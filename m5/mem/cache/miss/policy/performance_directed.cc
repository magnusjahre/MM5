/*
 * performance_directed.cc
 *
 *  Created on: May 8, 2010
 *      Author: jahre
 */

#include "performance_directed.hh"

PerformanceDirectedPolicy:: PerformanceDirectedPolicy(std::string _name,
			                                          InterferenceManager* _intManager,
			                                          Tick _period,
			                                          int _cpuCount,
			                                          PerformanceEstimationMethod _perfEstMethod,
			                                          bool _persistentAllocations,
			                                          int _iterationLatency,
			                                          Metric* _performanceMetric,
			                                          bool _enforcePolicy)
: BasePolicy(_name, _intManager, _period, _cpuCount, _perfEstMethod, _persistentAllocations, _iterationLatency, _performanceMetric, _enforcePolicy)
{
	cout << "created performance directed policy\n";
}


void
PerformanceDirectedPolicy::runPolicy(PerformanceMeasurement measurements){
	fatal("Not implemented");
}

bool
PerformanceDirectedPolicy::doEvaluation(int cpuID){
	fatal("Not implemented");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(PerformanceDirectedPolicy)
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<Tick> period;
	Param<int> cpuCount;
	Param<string> performanceEstimationMethod;
	Param<bool> persistentAllocations;
	Param<int> iterationLatency;
	Param<string> optimizationMetric;
	Param<bool> enforcePolicy;;
END_DECLARE_SIM_OBJECT_PARAMS(PerformanceDirectedPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(PerformanceDirectedPolicy)
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1048576),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true),
	INIT_PARAM_DFLT(iterationLatency, "The number of cycles it takes to evaluate one MHA", 0),
	INIT_PARAM_DFLT(optimizationMetric, "The metric to optimize for", "hmos"),
	INIT_PARAM_DFLT(enforcePolicy, "Should the policy be enforced?", true)
END_INIT_SIM_OBJECT_PARAMS(PerformanceDirectedPolicy)

CREATE_SIM_OBJECT(PerformanceDirectedPolicy)
{

	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	return new PerformanceDirectedPolicy(getInstanceName(),
							       interferenceManager,
							       period,
							       cpuCount,
							       perfEstMethod,
							       persistentAllocations,
							       iterationLatency,
							       performanceMetric,
							       enforcePolicy);
}

REGISTER_SIM_OBJECT("PerformanceDirectedPolicy", PerformanceDirectedPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS
