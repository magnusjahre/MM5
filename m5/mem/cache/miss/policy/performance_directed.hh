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
