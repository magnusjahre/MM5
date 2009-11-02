
#include "optimal_policy.hh"

OptimalPolicy::OptimalPolicy(string _name, InterferenceManager* _intManager, Tick _period)
: MissBandwidthPolicy(_name, _intManager, _period) {

	cout << "Optimal policy object created\n";
}

void
OptimalPolicy::runPolicy(InterferenceMeasurement measurements){
	fatal("optimal policy run not implemented");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(OptimalPolicy)
	Param<Tick> period;
	SimObjectParam<InterferenceManager* > interferenceManager;
END_DECLARE_SIM_OBJECT_PARAMS(OptimalPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(OptimalPolicy)
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1000000),
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL)
END_INIT_SIM_OBJECT_PARAMS(OptimalPolicy)

CREATE_SIM_OBJECT(OptimalPolicy)
{
	return new OptimalPolicy(getInstanceName(),
							 interferenceManager,
							 period);
}

REGISTER_SIM_OBJECT("OptimalPolicy", OptimalPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS
