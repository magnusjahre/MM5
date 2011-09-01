
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
	assert(point.size() == np*2);
	vector<double> gradient = vector<double>(np*2, 0.0);

	// bandwidth components
	for(int i=0;i<np;i++){
		assert(measurements->betas[i] >= 0);
		if(measurements->betas[i] == 0){
			gradient[i] = 0;
		}
		else{
			double sharedCacheMisses = measurements->getMisses(i, point[i+np]);

			assert(measurements->alphas[i] >= 0);
			double numerator = aloneCycles[i] * measurements->alphas[i] * sharedCacheMisses;
			double factor = measurements->betas[i] + ((measurements->alphas[i]*sharedCacheMisses) / point[i]);
			double denominator = point[i] * point[i] * factor * factor;
			gradient[i] = numerator/denominator;
		}
	}

	// cache miss components
	for(int i=0;i<np;i++){
		assert(measurements->betas[i] >= 0);
		if(measurements->betas[i] == 0){
			gradient[i+np] = 0;
		}
		else{
			if(measurements->inFlatSection(i, point[i+np])){
				gradient[i+np] = 0;
			}
			else{
				double sharedCacheMisses = measurements->getMisses(i, point[i+np]);

				assert(measurements->alphas[i] >= 0);
				double numerator = aloneCycles[i] * measurements->alphas[i] * measurements->getMissGradient(i);
				double factor = measurements->betas[i] + ((measurements->alphas[i]*sharedCacheMisses) / point[i]);
				double denominator = point[i] * factor * factor;
				gradient[i+np] = -numerator/denominator;
			}
		}
	}


	return gradient;
}

double
STPPolicy::computeFunction(PerformanceMeasurement* measurements, std::vector<double> xvals, std::vector<double> aloneCycles){
	double funcval = 0.0;

	assert(xvals.size() == aloneCycles.size()*2);

	for(int i=0;i<aloneCycles.size();i++){
		double denominator = ((measurements->alphas[i] * measurements->getMisses(i, xvals[i+aloneCycles.size()])) / xvals[i]) + measurements->betas[i];
		funcval += aloneCycles[i] / denominator;
	}

	return funcval;
}
