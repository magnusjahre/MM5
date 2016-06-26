
#ifndef AGGREGATE_IPC_POLICY_
#define AGGREGATE_IPC_POLICY_

#include "metric.hh"

class AggregateIPCPolicy : public Metric{

public:
	AggregateIPCPolicy();

	virtual double computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs);

	virtual std::string metricName(){
		return std::string("Aggregate IPC");
	}
};

#endif /* AGGREGATE_IPC_POLICY_ */
