
#ifndef FAIRNESS_POLICY_HH_
#define FAIRNESS_POLICY_HH_

#include "miss_bandwidth_policy.hh"

class FairnessPolicy : public MissBandwidthPolicy{

private:
	double findMaxValue(std::vector<double>* data);
	double findMinValue(std::vector<double>* data);


public:
	FairnessPolicy(string _name,
				   InterferenceManager* _intManager,
				   Tick _period,
				   int _cpuCount,
				   double _busUtilThreshold,
				   double _cutoffReqInt,
				   RequestEstimationMethod _reqEstMethod,
				   PerformanceEstimationMethod _perfEstMethod,
				   bool _persistentAlloc);

	virtual double computeMetric(std::vector<double>* speedups);
};

#endif /* FAIRNESS_POLICY_HH_ */
