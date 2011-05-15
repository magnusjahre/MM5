/*
 * throttle_control.cc
 *
 *  Created on: Apr 22, 2011
 *      Author: jahre
 */

#include "throttle_control.hh"
#include "sim/builder.hh"

using namespace std;

ThrottleControl::ThrottleControl(string _name,
								 double _targetRequestRate,
								 bool _traceArrivalRates,
								 std::string _throttlingPolicy,
								 std::string _cacheType,
								 int _cpuCount,
								 int _cacheCPUID)
: SimObject(_name){

	allocationSetAt = 0;
	sampleSize = 10000;
	traceArrivalRates = _traceArrivalRates;

	cpuCount = _cpuCount;

	if(_throttlingPolicy == "strict"){
		throttlingPolicy = STRICT;
	}
	else if(_throttlingPolicy == "average"){
		throttlingPolicy = AVERAGE;
	}
	else if(_throttlingPolicy == "token"){
		throttlingPolicy = TOKEN;
	}
	else{
		fatal("Unknown throttling policy");
	}

	if(_cacheType == "shared"){
		cacheType = SHARED_CACHE;
	}
	else if(_cacheType == "private"){
		cacheType = PRIVATE_CACHE;
	}
	else{
		fatal("Unknown cache type");
	}

	cacheCPUID = _cacheCPUID;
	if(cacheType == SHARED_CACHE && cacheCPUID != -1) fatal("A cache CPU ID value for a shared cache does not make sense");

	RequestRateSamplingEvent* event = new RequestRateSamplingEvent(this, sampleSize);
	event->schedule(curTick+sampleSize);

	targetRequestRate = vector<double>(cpuCount, 0.0);

	if(_targetRequestRate != -1.0){
		assert(cacheType == PRIVATE_CACHE);
		assert(cacheCPUID != -1);
		targetRequestRate[cacheCPUID] = _targetRequestRate;
	}

	measuredArrivalRate = vector<double>(cpuCount, 0.0);
	sampleAverage = vector<double>(cpuCount, 0.0);

	nextAllowedRequestTime = vector<Tick>(cpuCount, 0);
	tokenRunLast = vector<Tick>(cpuCount, 0);

	arrivalRateRequests = vector<int>(cpuCount, 0);
	sampleRequests = vector<int>(cpuCount, 0);
	tokens = vector<int>(cpuCount, 0);

	if(traceArrivalRates){
		arrivalRateTrace = RequestTrace(_name, "ArrivalRateTrace");

		vector<string> arrParams;
		for(int i=0;i<cpuCount;i++){
			arrParams.push_back(RequestTrace::buildTraceName("Requests  CPU", i));
			arrParams.push_back(RequestTrace::buildTraceName("Measured Average Arrival Rate CPU", i));
			arrParams.push_back(RequestTrace::buildTraceName("10K CC Average Arrival Rate CPU", i));
			arrParams.push_back(RequestTrace::buildTraceName("Target Arrival Rate CPU", i));
		}

		arrivalRateTrace.initalizeTrace(arrParams);
	}
}

Tick
ThrottleControl::determineIssueTime(Tick time, int cpuid){

	arrivalRateRequests[cpuid]++;
	sampleRequests[cpuid]++;
	measuredArrivalRate[cpuid] = (double) ((double) arrivalRateRequests[cpuid] / (double) (curTick - allocationSetAt));

	if(traceArrivalRates){
		vector<RequestTraceEntry> entries;
		for(int i=0;i<cpuCount;i++){
			entries.push_back(arrivalRateRequests[i]);
			entries.push_back(measuredArrivalRate[i]);
			entries.push_back(sampleAverage[i]);
			entries.push_back(targetRequestRate[i]);
		}
		arrivalRateTrace.addTrace(entries);
	}

	if(targetRequestRate[cpuid] > 0.0){
		if(throttlingPolicy == STRICT) return useStrictPolicy(time, cpuid);
		if(throttlingPolicy == AVERAGE) return useAveragePolicy(time, cpuid);
		if(throttlingPolicy == TOKEN) return useTokenPolicy(time, cpuid);
	}
	return time;
}

Tick
ThrottleControl::useStrictPolicy(Tick time, int cpuid){
	Tick issueAt = 0;
	double timeBetweenRequests = (1.0 / targetRequestRate[cpuid]);
	if(nextAllowedRequestTime[cpuid] > time){
		issueAt = nextAllowedRequestTime[cpuid];
		nextAllowedRequestTime[cpuid] += (int) timeBetweenRequests;
	}
	else{
		issueAt = time;
		nextAllowedRequestTime[cpuid] = time + (int) timeBetweenRequests;
	}
	assert(issueAt > 0);
	return issueAt;
}

Tick
ThrottleControl::useAveragePolicy(Tick time, int cpuid){
	if(measuredArrivalRate[cpuid] > targetRequestRate[cpuid]){
		Tick issueAt = 0;
		double timeBetweenRequests = (1.0 / targetRequestRate[cpuid]);
		if(nextAllowedRequestTime[cpuid] > time){
			issueAt = nextAllowedRequestTime[cpuid];
			nextAllowedRequestTime[cpuid] += (int) timeBetweenRequests;
		}
		else{
			issueAt = time;
			nextAllowedRequestTime[cpuid] = time + (int) timeBetweenRequests;
		}
		assert(issueAt > 0);
		return issueAt;
	}
	return time;
}

Tick
ThrottleControl::useTokenPolicy(Tick time, int cpuid){

	int cyclesBetweenRequests = (int) (1.0/targetRequestRate[cpuid]);
	if(tokenRunLast[cpuid] < time){

		Tick cyclesSinceLast = time - tokenRunLast[cpuid];
		Tick overflow = cyclesSinceLast % cyclesBetweenRequests;
		int newTokens = cyclesSinceLast / cyclesBetweenRequests;

		tokens[cpuid] += newTokens;

		if(tokens[cpuid] > 0){
			tokens[cpuid]--;
			tokenRunLast[cpuid] = time - overflow;
			return time;
		}
	}

	assert(tokens[cpuid] >= 0);
	tokenRunLast[cpuid] += cyclesBetweenRequests;
	return tokenRunLast[cpuid];
}

void
ThrottleControl::setTargetArrivalRate(vector<double> newRates){
	assert(newRates.size() == targetRequestRate.size());
	for(int i=0;i<newRates.size();i++){
		targetRequestRate[i] = newRates[i];
		measuredArrivalRate[i] = 0.0;
		arrivalRateRequests[i] = 0;

		tokens[i] = 0;
		tokenRunLast[i] = curTick;
	}
	allocationSetAt = curTick;
}

void
ThrottleControl::sampleArrivalRate(){
	for(int i=0;i<cpuCount;i++){
		sampleAverage[i] = (double) ((double) sampleRequests[i] / (double) sampleSize);
		sampleRequests[i] = 0;
	}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ThrottleControl)
	Param<double> target_request_rate;
    Param<bool> do_arrival_rate_trace;
    Param<string> throttling_policy;
    Param<string> cache_type;
    Param<int> cpu_count;
    Param<int> cache_cpu_id;
END_DECLARE_SIM_OBJECT_PARAMS(ThrottleControl)

BEGIN_INIT_SIM_OBJECT_PARAMS(ThrottleControl)
	INIT_PARAM_DFLT(target_request_rate, "The downstream request rate target for this cache", -1.0),
	INIT_PARAM_DFLT(do_arrival_rate_trace, "Trace the arrival rate on every request (caution!)", false),
	INIT_PARAM_DFLT(throttling_policy, "The policy to use to enforce throttles", "token"),
	INIT_PARAM_DFLT(cache_type, "Type of cache (shared or private)", "private"),
	INIT_PARAM(cpu_count, "Number of cores"),
	INIT_PARAM_DFLT(cache_cpu_id, "The CPU ID of the attached cache (only for private caches)", -1)
END_INIT_SIM_OBJECT_PARAMS(ThrottleControl)

CREATE_SIM_OBJECT(ThrottleControl)
{
	return new ThrottleControl(getInstanceName(),
			                   target_request_rate,
			                   do_arrival_rate_trace,
			                   throttling_policy,
			                   cache_type,
			                   cpu_count,
			                   cache_cpu_id);
}

REGISTER_SIM_OBJECT("ThrottleControl", ThrottleControl)

#endif //DOXYGEN_SHOULD_SKIP_THIS


