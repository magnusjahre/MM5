/*
 * performance_directed.hh
 *
 *  Created on: May 8, 2010
 *      Author: jahre
 */

#ifndef PERFORMANCE_DIRECTED_HH_
#define PERFORMANCE_DIRECTED_HH_

#include "base_policy.hh"

class PerformanceDirectedPolicy : public BasePolicy{

private:

	double bandwidthResolution;
	int cacheResolution;
	int numCacheSets;

	double bestMetricValue;
	std::vector<int> bestCacheSets;
	std::vector<double> bestBWAllocs;

	PerformanceMeasurement* currentMeasurements;

	template<class T>
	T computeSum(std::vector<T>* values);

	void exhaustiveSearch(int level, std::vector<double> bandwidthAllocations, std::vector<int> cacheSets);

	double evaluateAllocation(std::vector<double> bandwidthAllocations, std::vector<int> cacheSets);

	std::vector<double> getBusLatencies(std::vector<double> bandwidthAllocations);
	std::vector<double> getMissRates(std::vector<int> cacheSets);

public:
	PerformanceDirectedPolicy(std::string _name,
			                  InterferenceManager* _intManager,
			                  Tick _period,
			                  int _cpuCount,
			                  PerformanceEstimationMethod _perfEstMethod,
			                  bool _persistentAllocations,
			                  int _iterationLatency,
			                  Metric* _performanceMetric,
			                  bool _enforcePolicy);

	virtual void runPolicy(PerformanceMeasurement measurements);
	virtual bool doEvaluation(int cpuID);

};

#endif /* PERFORMANCE_DIRECTED_HH_ */
