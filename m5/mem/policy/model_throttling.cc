/*
 * model_throttling.cc
 *
 *  Created on: Jan 9, 2011
 *      Author: jahre
 */

#include "model_throttling.hh"

ModelThrottlingPolicy::ModelThrottlingPolicy(std::string _name,
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
	cout << "creating model throttling policy\n";
}

void
ModelThrottlingPolicy::runPolicy(PerformanceMeasurement measurements){
	fatal("model throttling runPolicy not implemented");
}

bool
ModelThrottlingPolicy::doEvaluation(int cpuID){
	fatal("model throttling doEvaluation not implemented");
	return false;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<Tick> period;
	Param<int> cpuCount;
	Param<string> performanceEstimationMethod;
	Param<bool> persistentAllocations;
	Param<int> iterationLatency;
	Param<string> optimizationMetric;
	Param<bool> enforcePolicy;
END_DECLARE_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1048576),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true),
	INIT_PARAM_DFLT(iterationLatency, "The number of cycles it takes to evaluate one MHA", 0),
	INIT_PARAM_DFLT(optimizationMetric, "The metric to optimize for", "hmos"),
	INIT_PARAM_DFLT(enforcePolicy, "Should the policy be enforced?", true)
END_INIT_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)

CREATE_SIM_OBJECT(ModelThrottlingPolicy)
{

	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	return new ModelThrottlingPolicy(getInstanceName(),
							         interferenceManager,
							         period,
							         cpuCount,
							         perfEstMethod,
							         persistentAllocations,
							         iterationLatency,
							         performanceMetric,
							         enforcePolicy);
}

REGISTER_SIM_OBJECT("ModelThrottlingPolicy", ModelThrottlingPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS


