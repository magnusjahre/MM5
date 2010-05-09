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
	cacheResolution = 4; // FIXME: Parameterize
	bandwidthResolution = 4.0; // FIXME: Parameterize

	numCacheSets = 16; //FIXME: get this value from the caches

}

void
PerformanceDirectedPolicy::exhaustiveSearch(int level,
											std::vector<double> bandwidthAllocations,
											std::vector<int> cacheSets){

	if(level < cpuCount){
		int cacheAllocUnit = numCacheSets / cacheResolution;
		double bwAllocUnit = 1.0 / bandwidthResolution;
		for(int i=1;i<cacheResolution;i++){
			for(int j=1;j<bandwidthResolution;j++){
				vector<int> newCacheSets = cacheSets;
				newCacheSets.push_back(i*cacheAllocUnit);

				vector<double> newBWAlloc = bandwidthAllocations;
				newBWAlloc.push_back(j*bwAllocUnit);

				exhaustiveSearch(level+1, newBWAlloc, newCacheSets);
			}
		}
	}
	else{
		assert(bandwidthAllocations.size() == cpuCount);
		assert(cacheSets.size() == cpuCount);
		if(computeSum(&bandwidthAllocations) != 1.0) return;
		if(computeSum(&cacheSets) != numCacheSets) return;

		DPRINTF(MissBWPolicyVerbose, "Possible resource allocation found:\n");
		traceVerboseVector("Evaluating cache allocation: ", cacheSets);
		traceVerboseVector("Evaluating bandwidth allocation: ", bandwidthAllocations);

		double metval = evaluateAllocation(bandwidthAllocations, cacheSets);
		if(metval > bestMetricValue){
			bestMetricValue = metval;
			bestCacheSets = cacheSets;
			bestBWAllocs = bandwidthAllocations;
		}
	}
}

double
PerformanceDirectedPolicy::evaluateAllocation(std::vector<double> bandwidthAllocations,
											  std::vector<int> cacheSets){

	warn("Evaluation not implemented");
	return 1.0;
}

void
PerformanceDirectedPolicy::runPolicy(PerformanceMeasurement measurements){

	DPRINTF(MissBWPolicy, "--- Running performance directed policy\n");

	bestBWAllocs.clear();
	bestCacheSets.clear();
	bestMetricValue = 0.0;

	exhaustiveSearch(0, vector<double>(), vector<int>());

	assert(bestBWAllocs.size() == cpuCount && bestCacheSets.size() == cpuCount);

	DPRINTF(MissBWPolicy, "Got metric value %d, best allocation is:\n", bestMetricValue);
	traceVector("Cache sets:", bestCacheSets);
	traceVector("Best BW alloc:", bestBWAllocs);

	fatal("Stop her for now");
}

bool
PerformanceDirectedPolicy::doEvaluation(int cpuID){
	return true;
}

template <class T>
T
PerformanceDirectedPolicy::computeSum(vector<T>* values){
	T sum = 0;
	for(int i=0;i<cpuCount;i++){
		sum += values->at(i);
	}
	return sum;
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
