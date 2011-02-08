
#include "stp_policy.hh"

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
STPPolicy::computeOptimalPeriod(PerformanceMeasurement* measurements, int np){
	cout << "computing optimal period\n";
	return vector<double>(np, 0.0);
}
