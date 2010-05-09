
#ifndef AGGREGATE_IPC_POLICY_
#define AGGREGATE_IPC_POLICY_

#include "metric.hh"

class AggregateIPCPolicy : public Metric{

public:
	AggregateIPCPolicy();

	virtual double computeMetric(std::vector<double>* speedups, std::vector<double>* sharedIPCs);
};

#endif /* AGGREGATE_IPC_POLICY_ */
