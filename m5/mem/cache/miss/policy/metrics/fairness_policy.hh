
#ifndef FAIRNESS_POLICY_HH_
#define FAIRNESS_POLICY_HH_

#include "metric.hh"

class FairnessPolicy : public Metric{

private:
	double findMaxValue(std::vector<double>* data);
	double findMinValue(std::vector<double>* data);


public:
	FairnessPolicy();

	virtual double computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs);
};

#endif /* FAIRNESS_POLICY_HH_ */
