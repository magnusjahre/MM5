/*
 * miss_bandwidth_policy.hh
 *
 *  Created on: Oct 28, 2009
 *      Author: jahre
 */

#include "sim/sim_object.hh"
#include "sim/eventq.hh"
#include "mem/interference_manager.hh"
#include "mem/requesttrace.hh"
#include "mem/cache/base_cache.hh"
#include "mem/cache/miss/policy/performance_measurement.hh"

#ifndef MISS_BANDWIDTH_POLICY_HH_
#define MISS_BANDWIDTH_POLICY_HH_

class MissBandwidthPolicyEvent;
class MissBandwidthTraceEvent;
class InterferenceManager;
class BaseCache;

class MissBandwidthPolicy : public SimObject{

protected:
	InterferenceManager* intManager;
	Tick period;
	MissBandwidthPolicyEvent* policyEvent;
	MissBandwidthTraceEvent* traceEvent;
	std::vector<Addr> cummulativeMemoryRequests;

	RequestTrace aloneIPCTrace;

	RequestTrace measurementTrace;
	std::vector<BaseCache* > caches;

	std::vector<double> aloneIPCEstimates;
	std::vector<double> avgLatencyAloneIPCModel;


	RequestTrace predictionTrace;
	RequestTrace partialMeasurementTrace;
	std::vector<double> currentLatencyProjection;
	std::vector<double> currentRequestProjection;
	std::vector<double> currentSpeedupProjection;
	std::vector<double> bestLatencyProjection;
	std::vector<double> bestRequestProjection;
	std::vector<double> bestSpeedupProjection;

	RequestTrace numMSHRsTrace;

	std::vector<std::vector<double> > mostRecentMWSEstimate;
	std::vector<std::vector<double> > mostRecentMLPEstimate;

	PerformanceMeasurement* currentMeasurements;

	int maxMSHRs;
	int cpuCount;

	double requestCountThreshold;
	double busUtilizationThreshold;
	double acceptanceThreshold;
	int renewMeasurementsThreshold;

	int renewMeasurementsCounter;

	void getAverageMemoryLatency(std::vector<int>* currentMHA,
							     std::vector<double>* estimatedSharedLatencies);

	int level;
	double maxMetricValue;
	std::vector<int> maxMHAConfiguration;
	std::vector<int> exhaustiveSearch();
	void recursiveExhaustiveSearch(std::vector<int>* value, int k);

	std::vector<int> relocateMHA(std::vector<int>* mhaConfig);

	double evaluateMHA(std::vector<int>* mhaConfig);

	template<class T>
	T computeSum(std::vector<T>* values);

	template<class T>
	std::vector<double> computePercetages(std::vector<T>* values);

	void initProjectionTrace(int cpuCount);
	void traceBestProjection();
	void traceNumMSHRs();
	void initNumMSHRsTrace(int cpuCount);
	void initPartialMeasurementTrace(int cpuCount);
	void tracePartialMeasurements();
	void initAloneIPCTrace(int cpuCount, bool policyEnforced);

	void traceAloneIPC(std::vector<int> memoryRequests, std::vector<double> ipcs);

	Stats::Vector<> aloneEstimationFailed;

	Stats::VectorStandardDeviation<> requestAbsError;
	Stats::VectorStandardDeviation<> sharedLatencyAbsError;
	Stats::VectorStandardDeviation<> requestRelError;
	Stats::VectorStandardDeviation<> sharedLatencyRelError;

	void regStats();

	double computeError(double estimate, double actual);

	// Debug trace methods
	void traceVerboseVector(const char* message, std::vector<int>& data);
	void traceVerboseVector(const char* message, std::vector<double>& data);
	void traceVector(const char* message, std::vector<int>& data);
	void traceVector(const char* message, std::vector<double>& data);
	void tracePerformance(std::vector<double>& sharedCycles);

	bool doMHAEvaluation(std::vector<int>& currentMHA);

	double computeCurrentMetricValue();

	void updateAloneIPCEstimate();

	void updateMWSEstimates();

	double computeSpeedup(double sharedIPCEstimate, int cpuID);

	double estimateStallCycles(double currentStallTime, double currentMWS, double currentAvgSharedLat, double newMWS, double newAvgSharedLat);

public:
	MissBandwidthPolicy(std::string _name,
						InterferenceManager* _intManager,
						Tick _period,
						int _cpuCount,
						double _busUtilThreshold,
						double _cutoffReqInt,
						bool _enforcePolicy = true);

	~MissBandwidthPolicy();

	void handlePolicyEvent();

	void handleTraceEvent();

	void registerCache(BaseCache* _cache, int _cpuID, int _maxMSHRs);

	void addTraceEntry(PerformanceMeasurement* measurement);

	void runPolicy(PerformanceMeasurement measurements);

	virtual double computeMetric(std::vector<double>* speedups) = 0;


};

class MissBandwidthPolicyEvent : public Event{
private:
	MissBandwidthPolicy* policy;
	Tick period;

public:
	MissBandwidthPolicyEvent(MissBandwidthPolicy* _policy, Tick _period):
	Event(&mainEventQueue), policy(_policy), period(_period){

	}
	void process() {
		policy->handlePolicyEvent();
		schedule(curTick + period);
	}

	virtual const char *description() {
		return "Miss Bandwidth Policy Event";
	}

};

class MissBandwidthTraceEvent : public Event{
private:
	MissBandwidthPolicy* policy;
	Tick period;

public:
	MissBandwidthTraceEvent(MissBandwidthPolicy* _policy, Tick _period):
	Event(&mainEventQueue), policy(_policy), period(_period){

	}
	void process() {
		policy->handleTraceEvent();
		schedule(curTick + period);
	}

	virtual const char *description() {
		return "Miss Bandwidth Trace Event";
	}

};


#endif /* MISS_BANDWIDTH_POLICY_HH_ */
