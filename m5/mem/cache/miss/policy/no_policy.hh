/*
 * no_policy.hh
 *
 *  Created on: Nov 18, 2009
 *      Author: jahre
 */

#ifndef NO_POLICY_HH_
#define NO_POLICY_HH_

#include "miss_bandwidth_policy.hh"

class NoBandwidthPolicy : public MissBandwidthPolicy{
public:
	NoBandwidthPolicy(string _name,
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
			          int _renewMeasurementsThreshold);

	virtual double computeMetric(std::vector<double>* speedups);

};


#endif /* NO_POLICY_HH_ */
