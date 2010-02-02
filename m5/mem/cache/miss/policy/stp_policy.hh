
#ifndef STP_POLICY_HH_
#define STP_POLICY_HH_

#include "miss_bandwidth_policy.hh"

class STPPolicy : public MissBandwidthPolicy{

public:
	STPPolicy(string _name,
		      InterferenceManager* _intManager,
		      Tick _period,
		      int _cpuCount,
		      double _busUtilThreshold,
		      double _cutoffReqInt,
		      RequestEstimationMethod _reqEstMethod,
		      PerformanceEstimationMethod _perfEstMethod,
		      bool _persistentAlloc,
		      double _acceptanceThreshold,
		      double _reqVariationThreshold,
		      int _renewMeasurementsThreshold,
		      SearchAlgorithm _searchAlgorithm,
		      int _iterationLatency,
		      double _busRequestThresholdIntensity);

	virtual double computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs);
};

#endif /* STP_POLICY_HH_ */
