
#ifndef STP_POLICY_HH_
#define STP_POLICY_HH_

#include "metric.hh"

class STPPolicy : public Metric{

public:
	STPPolicy();

	virtual double computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs);

	virtual std::vector<double> computeOptimalPeriod(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np);

private:
	double computeFraction(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np);
};

#endif /* STP_POLICY_HH_ */
