/*
 * metric.hh
 *
 *  Created on: May 8, 2010
 *      Author: jahre
 */

#ifndef METRIC_HH_
#define METRIC_HH_

#include <vector>
#include "base/misc.hh"

class Metric {
public:
	Metric();

	virtual double computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs);
};

#endif /* METRIC_HH_ */
