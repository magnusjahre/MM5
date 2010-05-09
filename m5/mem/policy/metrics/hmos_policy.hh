/*
 * hmos_policy.hh
 *
 *  Created on: Nov 19, 2009
 *      Author: jahre
 */

#ifndef HMOS_POLICY_HH_
#define HMOS_POLICY_HH_

#include "metric.hh"

class HmosPolicy : public Metric{

public:
	HmosPolicy();

	virtual double computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs);
};

#endif /* HMOS_POLICY_HH_ */
