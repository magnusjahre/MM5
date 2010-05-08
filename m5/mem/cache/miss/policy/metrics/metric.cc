/*
 * metric.cc
 *
 *  Created on: May 8, 2010
 *      Author: jahre
 */

#include "metric.hh"

Metric::Metric(){

}

double
Metric::computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs){
	fatal("Base class computeMetric does not make sense");
	return 0.0;
}
