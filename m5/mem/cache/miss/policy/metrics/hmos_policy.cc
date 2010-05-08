/*
 * hmos_policy.cc
 *
 *  Created on: Nov 19, 2009
 *      Author: jahre
 */


#include "hmos_policy.hh"

using namespace std;

HmosPolicy::HmosPolicy()
: Metric() {

}

double
HmosPolicy::computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs){

	int n = speedups->size();

	double denominator = 0.0;

	for(int i=0;i<speedups->size();i++){
		denominator += 1 / speedups->at(i);
	}

	return n / denominator;
}


