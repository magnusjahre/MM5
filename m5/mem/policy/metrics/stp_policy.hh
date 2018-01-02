
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

	virtual std::vector<double> gradient(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np, std::vector<double> point);

	virtual double computeFunction(PerformanceMeasurement* measurements, std::vector<double> xvals, std::vector<double> aloneCycles);

	virtual std::string metricName(){
		return std::string("STP");
	}

	virtual LookaheadMetricComponent getLookaheadMetricComponent(){
		return LMC_SPEEDUP;
	}

};

#endif /* STP_POLICY_HH_ */
