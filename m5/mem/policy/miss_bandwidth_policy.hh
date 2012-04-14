/*
 * miss_bandwidth_policy.hh
 *
 *  Created on: May 8, 2010
 *      Author: jahre
 */

#ifndef MISS_BANDWIDTH_POLICY_HH_
#define MISS_BANDWIDTH_POLICY_HH_

#include "base_policy.hh"

class MissBandwidthPolicy : public BasePolicy{

private:
	int level;
	double maxMetricValue;
	std::vector<int> maxMHAConfiguration;

	RequestEstimationMethod reqEstMethod;
	double requestCountThreshold;
	double busUtilizationThreshold;
	double acceptanceThreshold;
	int renewMeasurementsThreshold;
	double requestVariationThreshold;
	double busRequestThreshold;

	int renewMeasurementsCounter;

	std::vector<int> exhaustiveSearch();
	void recursiveExhaustiveSearch(std::vector<int>* value, int k);
	std::vector<int> busSearch(bool onlyPowerOfTwoMSHRs);

	std::vector<int> relocateMHA(std::vector<int>* mhaConfig);

	double evaluateMHA(std::vector<int> mhaConfig);

	double computeRequestScalingRatio(int cpuID, int newMSHRCount);

	void getAverageMemoryLatency(std::vector<int>* currentMHA,
							     std::vector<double>* estimatedSharedLatencies);

	void computeRequestStatistics();

	template<class T>
	T computeSum(std::vector<T>* values);

	template<class T>
	std::vector<double> computePercetages(std::vector<T>* values);

public:

	MissBandwidthPolicy(std::string _name,
			            InterferenceManager* _intManager,
			            Tick _period,
			            int _cpuCount,
			            PerformanceEstimationMethod _perfEstMethod,
			            bool _persistentAllocations,
			            int _iterationLatency,
			            Metric* _performanceMetric,
			            bool _enforcePolicy,
			            ThrottleControl* _sharedCacheThrottle,
			            std::vector<ThrottleControl* > _privateCacheThrottles,
			            double _busUtilThreshold,
			            double _cutoffReqInt,
			            RequestEstimationMethod _reqEstMethod,
			            double _acceptanceThreshold,
			            double _reqVariationThreshold,
			            int _renewMeasurementsThreshold,
			            SearchAlgorithm _searchAlgorithm,
			            double _busRequestThresholdIntensity,
			            WriteStallTechnique _wst,
			            PrivBlockedStallTechnique _pbst,
			            EmptyROBStallTechnique _rst);

	virtual void runPolicy(PerformanceMeasurement measurements);

	virtual bool doEvaluation(int cpuID);
};


#endif /* MISS_BANDWIDTH_POLICY_HH_ */
