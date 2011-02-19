
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
STPPolicy::computeOptimalPeriod(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np){

	measurements->updateConstants();

	double fraction = computeFraction(measurements, aloneCycles, np);
	DPRINTF(MissBWPolicy, "Final fraction is %f\n", fraction);

	vector<double> optimalPeriod = vector<double>(np, 0.0);

	for(int i=0;i<np;i++){
		double tmp = ((double) np*measurements->getPeriod()) - (measurements->alphas[i]*measurements->betas[i]);
		DPRINTF(MissBWPolicy, "CPU %d specific result is %f\n", i, tmp);
		optimalPeriod[i] = tmp / fraction;
	}

	// verify constraint
	int sum = 0;
	for(int i=0;i<np;i++) sum += optimalPeriod[i];
	if(sum != np*measurements->getPeriod()){
		fatal("Sum of optimal periods %d does not satisfy constraint (should be %d)", sum, np*measurements->getPeriod());
	}

	return optimalPeriod;
}

double
STPPolicy::computeFraction(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np){
	double betamult = 1.0;
	for(int i=0;i<np;i++){
		betamult *= measurements->betas[i];
	}

	DPRINTF(MissBWPolicy, "Multiplied betas is %f\n", betamult);

	double fracsum = 0.0;
	for(int i=0;i<np;i++){

		double privateCycles = aloneCycles[i];
		double tmp = sqrt(measurements->alphas[i] * privateCycles);

		DPRINTF(MissBWPolicy, "CPU %d sum root is %f\n", i, tmp);

		double tmp2 = tmp / measurements->betas[i];

		DPRINTF(MissBWPolicy, "CPU %d divided by beta gives %f\n", i, tmp2);

		fracsum -= tmp2;
	}

	DPRINTF(MissBWPolicy, "Final fraction sum is %f\n", fracsum);



	return (betamult * fracsum);
}
