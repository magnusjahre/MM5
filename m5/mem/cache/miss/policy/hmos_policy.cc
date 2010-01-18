/*
 * hmos_policy.cc
 *
 *  Created on: Nov 19, 2009
 *      Author: jahre
 */


#include "hmos_policy.hh"

using namespace std;

HmosPolicy::HmosPolicy(string _name,
		               InterferenceManager* _intManager,
		               Tick _period,
		               int _cpuCount,
		               double _busUtilThreshold,
		               double _cutoffReqInt,
		               RequestEstimationMethod _reqEstMethod,
		               PerformanceEstimationMethod _perfEstMethod,
		               bool _persistentAlloc,
		               double _acceptanceThreshold,
		               double _reqVariationThreshold,
		               int _renewMeasurementsThreshold,
                       SearchAlgorithm _searchAlgorithm,
                       int _iterationLatency,
                       bool _useBusAccessesInLatencyPrediction,
                       double _busRequestThresholdIntensity)
: MissBandwidthPolicy(_name, _intManager, _period, _cpuCount, _busUtilThreshold, _cutoffReqInt, _reqEstMethod, _perfEstMethod, _persistentAlloc, _acceptanceThreshold, _reqVariationThreshold, _renewMeasurementsThreshold, _searchAlgorithm, _iterationLatency, _useBusAccessesInLatencyPrediction, _busRequestThresholdIntensity) {

}

double
HmosPolicy::computeMetric(std::vector<double>* speedups){

	int n = speedups->size();

	double denominator = 0.0;

	for(int i=0;i<speedups->size();i++){
		denominator += 1 / speedups->at(i);
	}

	return n / denominator;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(HmosPolicy)
	Param<Tick> period;
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<int> cpuCount;
	Param<double> busUtilizationThreshold;
	Param<double> requestCountThreshold;
	Param<string> requestEstimationMethod;
	Param<string> performanceEstimationMethod;
	Param<bool> persistentAllocations;
	Param<double> acceptanceThreshold;
	Param<double> requestVariationThreshold;
	Param<double> renewMeasurementsThreshold;
	Param<string> searchAlgorithm;
	Param<int> iterationLatency;
	Param<bool> useBusAccessInLatPred;
	Param<double> busRequestThreshold;
END_DECLARE_SIM_OBJECT_PARAMS(HmosPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(HmosPolicy)
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1000000),
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM_DFLT(busUtilizationThreshold, "The actual bus utilzation to consider the bus as full", 0.95),
	INIT_PARAM_DFLT(requestCountThreshold, "The request intensity (requests / tick) to assume no request increase", 0.001),
	INIT_PARAM(requestEstimationMethod, "The request estimation method to use"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true),
	INIT_PARAM_DFLT(acceptanceThreshold, "The performance improvement needed to accept new MHA", 1.05),
	INIT_PARAM_DFLT(requestVariationThreshold, "Maximum acceptable request variation", 0.5),
	INIT_PARAM_DFLT(renewMeasurementsThreshold, "Samples to keep MHA", 10),
	INIT_PARAM_DFLT(searchAlgorithm, "The search algorithm to use", "exhaustive"),
	INIT_PARAM_DFLT(iterationLatency, "The number of cycles it takes to evaluate one MHA", 0),
	INIT_PARAM_DFLT(useBusAccessInLatPred, "Use bus accesses in latency prediciton", true),
	INIT_PARAM_DFLT(busRequestThreshold, "The bus request intensity necessary to consider request increases", 0.001)
END_INIT_SIM_OBJECT_PARAMS(HmosPolicy)

CREATE_SIM_OBJECT(HmosPolicy)
{
	MissBandwidthPolicy::RequestEstimationMethod reqEstMethod =
		MissBandwidthPolicy::parseRequestMethod(requestEstimationMethod);
	MissBandwidthPolicy::PerformanceEstimationMethod perfEstMethod =
		MissBandwidthPolicy::parsePerformanceMethod(performanceEstimationMethod);
	MissBandwidthPolicy::SearchAlgorithm searchAlg =
			MissBandwidthPolicy::parseSearchAlgorithm(searchAlgorithm);


	return new HmosPolicy(getInstanceName(),
							 interferenceManager,
							 period,
							 cpuCount,
							 busUtilizationThreshold,
							 requestCountThreshold,
							 reqEstMethod,
							 perfEstMethod,
							 persistentAllocations,
							 acceptanceThreshold,
							 requestVariationThreshold,
							 renewMeasurementsThreshold,
							 searchAlg,
							 iterationLatency,
							 useBusAccessInLatPred,
							 busRequestThreshold);
}

REGISTER_SIM_OBJECT("HmosPolicy", HmosPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS


