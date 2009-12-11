/*
 * no_policy.cc
 *
 *  Created on: Nov 18, 2009
 *      Author: jahre
 */

#include "no_policy.hh"

NoBandwidthPolicy::NoBandwidthPolicy(string _name,
		                             InterferenceManager* _intManager,
		                             Tick _period,
		                             int _cpuCount,
		                             double _busUtilThreshold,
		                             double _cutoffReqInt,
		                             RequestEstimationMethod _reqEstMethod,
		                             PerformanceEstimationMethod _perfEstMethod)
: MissBandwidthPolicy(_name, _intManager, _period, _cpuCount, _busUtilThreshold, _cutoffReqInt, _reqEstMethod, _perfEstMethod, false){

}

double
NoBandwidthPolicy::computeMetric(std::vector<double>* speedups){
	fatal("computeMetric should not be called on NoBandwidthPolicy objects");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(NoBandwidthPolicy)
	Param<Tick> period;
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<int> cpuCount;
	Param<double> busUtilizationThreshold;
	Param<double> requestCountThreshold;
	Param<string> requestEstimationMethod;
	Param<string> performanceEstimationMethod;
END_DECLARE_SIM_OBJECT_PARAMS(NoBandwidthPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(NoBandwidthPolicy)
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1000000),
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM_DFLT(busUtilizationThreshold, "The actual bus utilzation to consider the bus as full", 0.95),
	INIT_PARAM_DFLT(requestCountThreshold, "The request intensity (requests / tick) to assume no request increase", 0.001),
	INIT_PARAM(requestEstimationMethod, "The request estimation method to use"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations")
END_INIT_SIM_OBJECT_PARAMS(NoBandwidthPolicy)

CREATE_SIM_OBJECT(NoBandwidthPolicy)
{
	MissBandwidthPolicy::RequestEstimationMethod reqEstMethod =
		MissBandwidthPolicy::parseRequestMethod(requestEstimationMethod);
	MissBandwidthPolicy::PerformanceEstimationMethod perfEstMethod =
		MissBandwidthPolicy::parsePerfrormanceMethod(performanceEstimationMethod);


	return new NoBandwidthPolicy(getInstanceName(),
								 interferenceManager,
								 period,
								 cpuCount,
								 busUtilizationThreshold,
								 requestCountThreshold,
								 reqEstMethod,
								 perfEstMethod);
}

REGISTER_SIM_OBJECT("NoBandwidthPolicy", NoBandwidthPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS
