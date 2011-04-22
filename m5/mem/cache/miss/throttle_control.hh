/*
 * throttle_control.hh
 *
 *  Created on: Apr 22, 2011
 *      Author: jahre
 */

#ifndef THROTTLE_CONTROL_HH_
#define THROTTLE_CONTROL_HH_

#include "sim/sim_object.hh"
#include "sim/eventq.hh"
#include "mem/requesttrace.hh"

class ThrottleControl : public SimObject{

private:

	int cacheCPUID;
	int cpuCount;

    bool traceArrivalRates;
    RequestTrace arrivalRateTrace;

    std::vector<Tick> nextAllowedRequestTime;

    std::vector<int> arrivalRateRequests;
    std::vector<double> targetRequestRate;
    std::vector<double> measuredArrivalRate;
    Tick allocationSetAt;

    std::vector<int> sampleRequests;
    std::vector<double> sampleAverage;
    Tick sampleSize;

    std::vector<Tick> tokenRunLast;
    std::vector<int> tokens;

    enum ThrottlingPolicy{
    	STRICT,
    	AVERAGE,
    	TOKEN
    };

    ThrottlingPolicy throttlingPolicy;

    enum CacheType{
    	SHARED_CACHE,
    	PRIVATE_CACHE
    };

    CacheType cacheType;

    Tick useStrictPolicy(Tick time, int cpuid);
    Tick useAveragePolicy(Tick time, int cpuid);
    Tick useTokenPolicy(Tick time, int cpuid);

public:
	ThrottleControl(std::string _name,
			        double _targetRequestRate,
			        bool _traceArrivalRates,
			        std::string _throttlingPolicy,
			        std::string _cacheType,
			        int _cpuCount,
			        int _cacheCPUID);

    Tick determineIssueTime(Tick time, int cpuid);

    void setTargetArrivalRate(std::vector<double> newRates);

    void sampleArrivalRate();

    class RequestRateSamplingEvent : public Event{
    private:
    	ThrottleControl* tc;
    	Tick frequency;
    public:
    	RequestRateSamplingEvent(ThrottleControl* _tc, Tick _frequency) :
    		Event(&mainEventQueue){
    		tc = _tc;
    		frequency = _frequency;

    	}

		void process() {
			tc->sampleArrivalRate();
			schedule(curTick+frequency);
		}
    };
};

#endif /* THROTTLE_CONTROL_HH_ */
