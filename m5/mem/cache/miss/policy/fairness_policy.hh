
#ifndef FAIRNESS_POLICY_HH_
#define FAIRNESS_POLICY_HH_

#include "miss_bandwidth_policy.hh"

class FairnessPolicy : public MissBandwidthPolicy{

public:
	FairnessPolicy(string _name, InterferenceManager* _intManager, Tick _period);

	virtual double computeMetric();
};

#endif /* FAIRNESS_POLICY_HH_ */
