
#include "fairness_policy.hh"

using namespace std;

FairnessPolicy::FairnessPolicy()
: Metric() {

}

double
FairnessPolicy::findMaxValue(std::vector<double>* data){
	double max = 0;
	for(int i=0;i<data->size();i++){
		if(max < data->at(i)){
			max = data->at(i);
		}
	}
	return max;
}

double
FairnessPolicy::findMinValue(std::vector<double>* data){
	double min = 100; // the fairness metric has a maximum value of 1
	for(int i=0;i<data->size();i++){
		if(min > data->at(i)){
			min = data->at(i);
		}
	}
	return min;
}

double
FairnessPolicy::computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs){

	double max = findMaxValue(speedups);
	double min = findMinValue(speedups);

	return min / max;
}
