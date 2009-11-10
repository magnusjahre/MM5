
#include "fairness_policy.hh"

using namespace std;

FairnessPolicy::FairnessPolicy(string _name, InterferenceManager* _intManager, Tick _period, int _cpuCount)
: MissBandwidthPolicy(_name, _intManager, _period, _cpuCount) {

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
FairnessPolicy::computeMetric(std::vector<int>* mhaConfig, std::vector<std::vector<double> >* speedups){

	vector<double> useSpeedups = retrieveSpeedups(mhaConfig, speedups);

	double max = findMaxValue(&useSpeedups);
	double min = findMinValue(&useSpeedups);

	return min / max;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(FairnessPolicy)
	Param<Tick> period;
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<int> cpuCount;
END_DECLARE_SIM_OBJECT_PARAMS(FairnessPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(FairnessPolicy)
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1000000),
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM(cpuCount, "The number of cpus in the system")
END_INIT_SIM_OBJECT_PARAMS(FairnessPolicy)

CREATE_SIM_OBJECT(FairnessPolicy)
{
	return new FairnessPolicy(getInstanceName(),
							 interferenceManager,
							 period,
							 cpuCount);
}

REGISTER_SIM_OBJECT("FairnessPolicy", FairnessPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS
