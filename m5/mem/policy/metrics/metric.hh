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

	virtual std::vector<double> gradient(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np, std::vector<double> point){
		fatal("Metric has not implemented gradient");
	    return std::vector<double>();
	}

	virtual double computeFunction(PerformanceMeasurement* measurements, std::vector<double> xvals, std::vector<double> aloneCycles){
		fatal("Metric has not implemented computeFunction");
		return 0.0;
	}
};

#endif /* METRIC_HH_ */
