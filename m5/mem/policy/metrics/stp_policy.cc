
#include "stp_policy.hh"
#include "base/trace.hh"

using namespace std;

STPPolicy::STPPolicy()
: Metric() {

}

double
STPPolicy::computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs){
	double sum = 0.0;
	for(int i=0;i<speedups->size();i++){
		sum += speedups->at(i);
	}
	return sum;
}

std::vector<double>
STPPolicy::gradient(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np, std::vector<double> point){
	assert(point.size() == np);
	vector<double> gradient = vector<double>(np, 0.0);

	for(int i=0;i<np;i++){
		assert(measurements->betas[i] >= 0);
		if(measurements->betas[i] == 0){
			gradient[i] = 0;
		}
		else{
			assert(measurements->alphas[i] >= 0);
			double numerator = aloneCycles[i] * measurements->alphas[i];
			double factor = measurements->betas[i] + (measurements->alphas[i] / point[i]);
			double denominator = point[i] * point[i] * factor * factor;
			gradient[i] = numerator/denominator;
		}
	}

	return gradient;
}

double
STPPolicy::computeFunction(PerformanceMeasurement* measurements, std::vector<double> xvals, std::vector<double> aloneCycles){
	double funcval = 0.0;

	assert(xvals.size() == aloneCycles.size());

	for(int i=0;i<xvals.size();i++){
		double denominator = (measurements->alphas[i] / xvals[i]) + measurements->betas[i];
		funcval += aloneCycles[i] / denominator;
	}

	return funcval;
}
