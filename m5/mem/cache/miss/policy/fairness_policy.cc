
#include "fairness_policy.hh"

FairnessPolicy::FairnessPolicy(string _name, InterferenceManager* _intManager, Tick _period)
: MissBandwidthPolicy(_name, _intManager, _period) {

}

void
FairnessPolicy::runPolicy(PerformanceMeasurement measurements){
	cout << curTick << ": run policy called but not implemented\n";
	addTraceEntry(&measurements);
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(FairnessPolicy)
	Param<Tick> period;
	SimObjectParam<InterferenceManager* > interferenceManager;
END_DECLARE_SIM_OBJECT_PARAMS(FairnessPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(FairnessPolicy)
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1000000),
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL)
END_INIT_SIM_OBJECT_PARAMS(FairnessPolicy)

CREATE_SIM_OBJECT(FairnessPolicy)
{
	return new FairnessPolicy(getInstanceName(),
							 interferenceManager,
							 period);
}

REGISTER_SIM_OBJECT("FairnessPolicy", FairnessPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS
