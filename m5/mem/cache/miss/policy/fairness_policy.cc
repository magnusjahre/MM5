
#include "fairness_policy.hh"

using namespace std;

FairnessPolicy::FairnessPolicy(string _name,
		                       InterferenceManager* _intManager,
		                       Tick _period,
		                       int _cpuCount,
		                       double _busUtilThreshold,
		                       double _cutoffReqInt,
		                       RequestEstimationMethod _reqEstMethod,
		                       PerformanceEstimationMethod _perfEstMethod,
		                       bool _persistentAlloc)
: MissBandwidthPolicy(_name, _intManager, _period, _cpuCount, _busUtilThreshold, _cutoffReqInt, _reqEstMethod, _perfEstMethod, _persistentAlloc) {

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
FairnessPolicy::computeMetric(std::vector<double>* speedups){

	double max = findMaxValue(speedups);
	double min = findMinValue(speedups);

	return min / max;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(FairnessPolicy)
	Param<Tick> period;
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<int> cpuCount;
	Param<double> busUtilizationThreshold;
	Param<double> requestCountThreshold;
	Param<string> requestEstimationMethod;
	Param<string> performanceEstimationMethod;
	Param<bool> persistentAllocations;
END_DECLARE_SIM_OBJECT_PARAMS(FairnessPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(FairnessPolicy)
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1000000),
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM_DFLT(busUtilizationThreshold, "The actual bus utilzation to consider the bus as full", 0.95),
	INIT_PARAM_DFLT(requestCountThreshold, "The request intensity (requests / tick) to assume no request increase", 0.001),
	INIT_PARAM(requestEstimationMethod, "The request estimation method to use"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true)
END_INIT_SIM_OBJECT_PARAMS(FairnessPolicy)

CREATE_SIM_OBJECT(FairnessPolicy)
{

	MissBandwidthPolicy::RequestEstimationMethod reqEstMethod =
		MissBandwidthPolicy::parseRequestMethod(requestEstimationMethod);
	MissBandwidthPolicy::PerformanceEstimationMethod perfEstMethod =
		MissBandwidthPolicy::parsePerfrormanceMethod(performanceEstimationMethod);

	return new FairnessPolicy(getInstanceName(),
							 interferenceManager,
							 period,
							 cpuCount,
							 busUtilizationThreshold,
							 requestCountThreshold,
							 reqEstMethod,
							 perfEstMethod,
							 persistentAllocations);
}

REGISTER_SIM_OBJECT("FairnessPolicy", FairnessPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS
