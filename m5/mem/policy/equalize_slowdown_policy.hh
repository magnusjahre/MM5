/*
 * equalize_slowdown.hh
 *
 *  Created on: Jan 15, 2016
 *      Author: jahre
 */

#ifndef MEM_POLICY_EQUALIZE_SLOWDOWN_POLICY_HH_
#define MEM_POLICY_EQUALIZE_SLOWDOWN_POLICY_HH_


#include "base_policy.hh"

class EqualizeSlowdownPolicy : public BasePolicy{

private:

	double bestMetricValue;
	vector<int> bestAllocation;
	int maxWays;

	RequestTrace allocationTrace;

	void dumpMissCurves(PerformanceMeasurement measurements);

	double getConstBForCPU(PerformanceMeasurement measurements, int cpuID);

	//double getGradientForCPU(PerformanceMeasurement measurements, int cpuID);

	double computeGradientForCPU(PerformanceMeasurement measurement, int cpuID, double b);

	double computeSpeedup(int cpuID, int misses, double gradient, double b);

	std::vector<double> computeSpeedups(PerformanceMeasurement* measurements,
			                            std::vector<int> allocation,
										std::vector<double> gradients,
										std::vector<double> bs);

	void exhaustiveSearch(PerformanceMeasurement* measurements,
                          std::vector<int> allocation,
						  std::vector<double> gradients,
						  std::vector<double> bs);

	void evaluateAllocation(PerformanceMeasurement* measurements,
                            std::vector<int> allocation,
							std::vector<double> gradients,
							std::vector<double> bs);

	std::string getAllocString(std::vector<int> allocation);

	int sum(std::vector<int> allocation);

public:
	EqualizeSlowdownPolicy(std::string _name,
			               InterferenceManager* _intManager,
						   Tick _period,
						   int _cpuCount,
						   PerformanceEstimationMethod _perfEstMethod,
						   bool _persistentAllocations,
						   int _iterationLatency,
						   Metric* _performanceMetric,
						   bool _enforcePolicy,
						   WriteStallTechnique _wst,
						   PrivBlockedStallTechnique _pbst,
						   EmptyROBStallTechnique _rst,
						   double _maximumDamping,
						   double _hybridDecisionError,
						   int _hybridBufferSize);

	virtual void init();

	virtual void runPolicy(PerformanceMeasurement measurements);

	virtual bool doEvaluation(int cpuID);
};

#endif /* MEM_POLICY_EQUALIZE_SLOWDOWN_POLICY_HH_ */
