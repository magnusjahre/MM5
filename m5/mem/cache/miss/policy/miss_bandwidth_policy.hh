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

public:
    typedef enum{
        MWS,
        MLP
    } RequestEstimationMethod;

    typedef enum{
        LATENCY_MLP,
        LATENCY_MLP_SREQ,
        RATIO_MWS,
        NO_MLP
    } PerformanceEstimationMethod;

protected:

	RequestEstimationMethod reqEstMethod;
	PerformanceEstimationMethod perfEstMethod;

	InterferenceManager* intManager;
	Tick period;
	MissBandwidthPolicyEvent* policyEvent;
	MissBandwidthTraceEvent* traceEvent;

	std::vector<Addr> cummulativeMemoryRequests;
	std::vector<Addr> cummulativeCommittedInsts;

	RequestTrace aloneIPCTrace;

	RequestTrace measurementTrace;
	std::vector<BaseCache* > caches;

	std::vector<double> aloneIPCEstimates;
	std::vector<double> avgLatencyAloneIPCModel;

	bool dumpInitalized;
	Tick dumpSearchSpaceAt;

	RequestTrace predictionTrace;

	std::vector<double> currentRequestProjection;
	std::vector<double> currentLatencyProjection;
	std::vector<double> currentMWSProjection;
	std::vector<double> currentMLPProjection;
	std::vector<double> currentIPCProjection;
	std::vector<double> currentSpeedupProjection;

	std::vector<double> bestRequestProjection;
	std::vector<double> bestLatencyProjection;
	std::vector<double> bestMWSProjection;
	std::vector<double> bestMLPProjection;
	std::vector<double> bestIPCProjection;
	std::vector<double> bestSpeedupProjection;

	std::vector<RequestTrace> comInstModelTraces;
	std::vector<Tick> comInstModelTraceCummulativeInst;

	bool usePersistentAllocations;

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

	void initAloneIPCTrace(int cpuCount, bool policyEnforced);

	void traceAloneIPC(std::vector<int> memoryRequests,
			           std::vector<double> ipcs,
			           std::vector<int> committedInstructions,
			           std::vector<int> stallCycles,
			           std::vector<double> avgLatencies,
		               std::vector<std::vector<double> > missesWhileStalled);

	void initComInstModelTrace(int cpuCount);

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

	double estimateStallCycles(double currentStallTime,
		                       double currentMWS,
		                       double currentMLP,
		                       double currentAvgSharedLat,
		                       double currentRequests,
		                       double newMWS,
		                       double newMLP,
		                       double newAvgSharedLat,
		                       double newRequests,
		                       double responsesWhileStalled);

	double computeRequestScalingRatio(int cpuID, int newMSHRCount);

	void dumpSearchSpace(std::vector<int>* mhaConfig, double metricValue);

public:

	MissBandwidthPolicy(std::string _name,
						InterferenceManager* _intManager,
						Tick _period,
						int _cpuCount,
						double _busUtilThreshold,
						double _cutoffReqInt,
						RequestEstimationMethod _reqEstMethod,
						PerformanceEstimationMethod _perfEstMethod,
						bool _persistentAllocations,
						bool _enforcePolicy = true);

	~MissBandwidthPolicy();

	void handlePolicyEvent();

	void handleTraceEvent();

	void registerCache(BaseCache* _cache, int _cpuID, int _maxMSHRs);

	void addTraceEntry(PerformanceMeasurement* measurement);

	void runPolicy(PerformanceMeasurement measurements);

	virtual double computeMetric(std::vector<double>* speedups) = 0;

	void doCommittedInstructionTrace(int cpuID,
				                     double avgSharedLat,
				                     double avgPrivateLatEstimate,
				                     double mws,
				                     double mlp,
				                     int reqs,
				                     int stallCycles,
				                     int totalCycles,
				                     int committedInsts,
				                     int responsesWhileStalled);

	static RequestEstimationMethod parseRequestMethod(std::string methodName);
	static PerformanceEstimationMethod parsePerfrormanceMethod(std::string methodName);
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
