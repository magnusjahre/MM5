/*
 * hmos_policy.hh
 *
 *  Created on: Nov 19, 2009
 *      Author: jahre
 */

#ifndef HMOS_POLICY_HH_
#define HMOS_POLICY_HH_

#include "miss_bandwidth_policy.hh"

class HmosPolicy : public MissBandwidthPolicy{

public:
	HmosPolicy(string _name,
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
			   int _iterationLatency);

	virtual double computeMetric(std::vector<double>* speedups);
};

#endif /* HMOS_POLICY_HH_ */
