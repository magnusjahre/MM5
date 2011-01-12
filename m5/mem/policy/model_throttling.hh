/*
 * model_throttling.hh
 *
 *  Created on: Jan 9, 2011
 *      Author: jahre
 */

#ifndef MODEL_THROTTLING_HH_
#define MODEL_THROTTLING_HH_

#include "base_policy.hh"

class ModelThrottlingPolicy : public BasePolicy{

private:
	std::vector<std::vector<MSHROccupancy>* > mshrOccupancyPtrs;
	double bestMetricValue;
	std::vector<int> bestMHA;
	std::vector<int> bestThrot;

public:
	ModelThrottlingPolicy(std::string _name,
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

private:
	void simpleSearch(PerformanceMeasurement* measurements);

	double processConfiguration(std::vector<int> mha, std::vector<int> throttle, PerformanceMeasurement* measurements);

	int estimateInsertedRequests(int cpuID, int mshrs, int throttling, PerformanceMeasurement* measurements);
};

#endif /* MODEL_THROTTLING_HH_ */
