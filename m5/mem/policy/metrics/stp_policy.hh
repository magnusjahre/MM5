
#ifndef STP_POLICY_HH_
#define STP_POLICY_HH_

#include "metric.hh"

class STPPolicy : public Metric{

private:
	std::vector<double> zs;
	std::vector<double> taus;

public:
	STPPolicy();

	virtual double computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs);

	virtual std::vector<double> computeOptimalPeriod(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np);
	virtual std::vector<double> gradient(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np, std::vector<double> point);
	virtual double getInitLambda(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, double x0);
	virtual double computeFunction(PerformanceMeasurement* measurements, std::vector<double> xvals, std::vector<double> aloneCycles);

private:

	void computeZs(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np);

	void computeTaus(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np);

	double computeGamma(PerformanceMeasurement* measurements, int np);

	double computeDelta(PerformanceMeasurement* measurements, int np, double totalAloneCycles);
};

#endif /* STP_POLICY_HH_ */
