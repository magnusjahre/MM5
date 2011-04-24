/*
 * model_throttling.hh
 *
 *  Created on: Jan 9, 2011
 *      Author: jahre
 */

#ifndef MODEL_THROTTLING_HH_
#define MODEL_THROTTLING_HH_

#include "base_policy.hh"
#include "sim/sim_events.hh"
#include "mem/datadump.hh"

class ModelThrottlingPolicy : public BasePolicy{

private:

	std::vector<double> optimalPeriods;
	vector<double> optimalBWShares;

	RequestTrace throttleTrace;
	RequestTrace modelSearchTrace;
	int searchItemNum;

	bool doVerification;
	double throttleLimit;

	enum BWImplStrat{
		BW_IMPL_NFQ,
		BW_IMPL_THROTTLE
	};
	BWImplStrat implStrat;

public:
	ModelThrottlingPolicy(std::string _name,
			   	    InterferenceManager* _intManager,
			        Tick _period,
			        int _cpuCount,
			        PerformanceEstimationMethod _perfEstMethod,
			        bool _persistentAllocations,
			        int _iterationLatency,
			        Metric* _performanceMetric,
			        bool _enforcePolicy,
			        ThrottleControl* _sharedCacheThrottle,
			        std::vector<ThrottleControl* > _privateCacheThrottles,
			        bool _verify,
			        std::vector<double> _staticArrivalRates,
			        std::string _implStrategy);

	virtual void runPolicy(PerformanceMeasurement measurements);

	virtual bool doEvaluation(int cpuID);

private:

	std::vector<double> findNewTrialPoint(std::vector<double> gradient, PerformanceMeasurement* measurements);

	void implementAllocation(std::vector<double> allocation);
	std::vector<double> findOptimalArrivalRates(PerformanceMeasurement* measurements);

	double findOptimalStepSize(std::vector<double> xvec, std::vector<double> xstar, PerformanceMeasurement* measurements);
	double fastFindOptimalStepSize(std::vector<double> xvec, std::vector<double> xstar, PerformanceMeasurement* measurements);
	double setPrecision(double number, int decimalPlaces);

	std::vector<double> addMultCons(std::vector<double> xvec, std::vector<double> xstar, double step);

	void initThrottleTrace(int np);
	void traceThrottling(std::vector<double> throttles);

	void initSearchTrace(int np);
	void traceSearch(std::vector<double> xvec);

	void dumpVerificationData(PerformanceMeasurement* measurements, std::vector<double> optimalArrivalRates);

	double getTotalMisses(int cpuid, PerformanceMeasurement* measurements);

	double checkConvergence(std::vector<double> xstar, std::vector<double> xvec, std::vector<double> gradient);

	class StaticAllocationEvent : public Event{
	private:
		ModelThrottlingPolicy* mtp;
		std::vector<double> allocations;
	public:
		StaticAllocationEvent(ModelThrottlingPolicy* _mtp, std::vector<double> _alloc):
		Event(&mainEventQueue), mtp(_mtp), allocations(_alloc){

		}

		void process() {
			mtp->implementAllocation(allocations);
			delete this;
		}
	};
};

#endif /* MODEL_THROTTLING_HH_ */
