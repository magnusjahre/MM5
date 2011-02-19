/*
 * metric.hh
 *
 *  Created on: May 8, 2010
 *      Author: jahre
 */

#ifndef METRIC_HH_
#define METRIC_HH_

#include <vector>
#include <cmath>
#include "base/misc.hh"
#include "mem/policy/performance_measurement.hh"

class Metric {

public:
	Metric();

    virtual ~Metric(){ }

	virtual double computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs);

	virtual std::vector<double> computeOptimalPeriod(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np){
		fatal("Metric has not implemented computeOptimalPeriod");
		return std::vector<double>();
	}
};

#endif /* METRIC_HH_ */
