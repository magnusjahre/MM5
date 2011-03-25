
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

	if(zs.empty()){
		zs.resize(np, 0.0);
		taus.resize(np, 0.0);
	}
	double totalAloneCycles = 0.0;
	for(int i=0;i<np;i++) totalAloneCycles += aloneCycles[i];

	// update constants
	measurements->updateConstants();
	computeZs(measurements, aloneCycles, np);
	computeTaus(measurements, aloneCycles, np);
	double gamma = computeGamma(measurements, np);
	double delta = computeDelta(measurements, np, totalAloneCycles);

	vector<double> optimalPeriod = vector<double>(np, 0.0);

	for(int i=0;i<np;i++){
		double resultNumerator = - gamma * taus[i] - delta * zs[i];
		double resultDenominator = gamma * measurements->betas[i];
		double result = resultNumerator / resultDenominator;
		DPRINTF(MissBWPolicy, "CPU %d, numerator is %f, denominator %f, result is %f\n", i, resultNumerator, resultDenominator, result);
		optimalPeriod[i] = result + aloneCycles[i];
	}

	// verify constraint
	double sum = 0;
	for(int i=0;i<np;i++) sum += optimalPeriod[i];
	int intsum = floor(sum + 0.5);
	if(intsum != np*measurements->getPeriod()){
		fatal("Sum of optimal periods %d does not satisfy constraint (should be %d)", intsum, np*measurements->getPeriod());
	}
	DPRINTF(MissBWPolicy, "Sum of optimal periods %d is equal to target %d\n", intsum, np*measurements->getPeriod());

	return optimalPeriod;
}

void
STPPolicy::computeZs(PerformanceMeasurement*  measurements, vector<double> aloneCycles, int np){
	for(int i=0;i<np;i++){
		zs[i] = sqrt(aloneCycles[i] * measurements->alphas[i]); // use positive solution
		DPRINTF(MissBWPolicy, "CPU %d z=%f\n", i, zs[i]);
	}
}

void
STPPolicy::computeTaus(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np){
	for(int i=0;i<np;i++){
		taus[i] = measurements->betas[i]*aloneCycles[i] + measurements->alphas[i];
		DPRINTF(MissBWPolicy, "CPU %d tau=%f\n", i, taus[i]);
	}
}

double
STPPolicy::computeGamma(PerformanceMeasurement*  measurements, int np){
	double gamma = 0.0;
	for(int i=0;i<np;i++){
		double element = - zs[i] / measurements->betas[i];
		DPRINTF(MissBWPolicy, "CPU %d gamma element is %f\n", i, element);
		gamma += element;
	}

	DPRINTF(MissBWPolicy, "Final gamma is %f\n", gamma);
	return gamma;
}

double
STPPolicy::computeDelta(PerformanceMeasurement*  measurements, int np, double totalAloneCycles){
	double delta = (double) (np*measurements->getPeriod()) - totalAloneCycles;
	DPRINTF(MissBWPolicy, "Initializing delta to %f\n", delta);

	for(int i=0;i<np;i++){
		double element = taus[i] / measurements->betas[i];
		DPRINTF(MissBWPolicy, "CPU %d delta element is %f\n", i, element);
		delta += element;
	}
	DPRINTF(MissBWPolicy, "Final delta is %f\n", delta);
	return delta;
}

//double
//STPPolicy::computeFraction(PerformanceMeasurement* measurements, std::vector<double> aloneCycles, int np){
//	double betamult = 1.0;
//	for(int i=0;i<np;i++){
//		betamult *= measurements->betas[i];
//	}
//
//	DPRINTF(MissBWPolicy, "Multiplied betas is %f\n", betamult);
//
//	double fracsum = 0.0;
//	for(int i=0;i<np;i++){
//
//		double privateCycles = aloneCycles[i];
//		double tmp = sqrt(measurements->alphas[i] * privateCycles);
//
//		DPRINTF(MissBWPolicy, "CPU %d sum root is %f\n", i, tmp);
//
//		double tmp2 = tmp / measurements->betas[i];
//
//		DPRINTF(MissBWPolicy, "CPU %d divided by beta gives %f\n", i, tmp2);
//
//		fracsum -= tmp2;
//	}
//
//	DPRINTF(MissBWPolicy, "Final fraction sum is %f\n", fracsum);
//
//
//
//	return (betamult * fracsum);
//}
