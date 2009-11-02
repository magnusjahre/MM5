/*
 * optimal_policy.hh
 *
 *  Created on: Nov 2, 2009
 *      Author: jahre
 */

#ifndef OPTIMAL_POLICY_HH_
#define OPTIMAL_POLICY_HH_

#include "miss_bandwidth_policy.hh"

class OptimalPolicy : public MissBandwidthPolicy{

public:
	OptimalPolicy(string _name, InterferenceManager* _intManager, Tick _period);

	virtual void runPolicy(InterferenceMeasurement measurements);
};

#endif /* OPTIMAL_POLICY_HH_ */
