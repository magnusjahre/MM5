
#include "aggregate_ipc_policy.hh"

using namespace std;

AggregateIPCPolicy::AggregateIPCPolicy()
: Metric() {

}

double
AggregateIPCPolicy::computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs){
	double sum = 0.0;
	for(int i=0;i<sharedIPCs->size();i++){
		sum += sharedIPCs->at(i);
	}
	return sum;
}

