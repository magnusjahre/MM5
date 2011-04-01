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

	RequestTrace throttleTrace;

	bool doVerification;

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
			        bool _verify,
			        std::vector<double> _staticArrivalRates);

	virtual void runPolicy(PerformanceMeasurement measurements);

	virtual bool doEvaluation(int cpuID);

private:

	void setArrivalRates(std::vector<double> rates);
	std::vector<double> findOptimalArrivalRates(PerformanceMeasurement* measurements);

	void initThrottleTrace(int np);
	void traceThrottling(std::vector<double> throttles);

	void quitForVerification(PerformanceMeasurement* measurements, std::vector<double> optimalArrivalRates);

	double magnitudeOfDifference(std::vector<double> oldvec, std::vector<double> newvec);

	class StaticAllocationEvent : public Event{
	private:
		ModelThrottlingPolicy* mtp;
		std::vector<double> rates;
	public:
		StaticAllocationEvent(ModelThrottlingPolicy* _mtp, std::vector<double> _rates):
		Event(&mainEventQueue), mtp(_mtp), rates(_rates){

		}

		void process() {
			mtp->setArrivalRates(rates);
			delete this;
		}
	};
};

#endif /* MODEL_THROTTLING_HH_ */
