/*
 * miss_bandwidth_policy.hh
 *
 *  Created on: Oct 28, 2009
 *      Author: jahre
 */

#include "sim/sim_object.hh"
#include "sim/eventq.hh"
#include "sim/sim_exit.hh"
#include "base/callback.hh"

#include "mem/interference_manager.hh"
#include "mem/requesttrace.hh"
#include "mem/cache/base_cache.hh"

#include "mem/policy/performance_measurement.hh"
#include "mem/policy/metrics/metric.hh"
#include "mem/policy/metrics/hmos_policy.hh"
#include "mem/policy/metrics/fairness_policy.hh"
#include "mem/policy/metrics/stp_policy.hh"
#include "mem/policy/metrics/aggregate_ipc_policy.hh"

#ifndef BASE_POLICY_HH_
#define BASE_POLICY_HH_

class MissBandwidthPolicyEvent;
class MissBandwidthTraceEvent;
class InterferenceManager;
class BaseCache;

class BasePolicy : public SimObject{

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

    typedef enum{
    	EXHAUSTIVE_SEARCH,
    	BUS_SORTED,
    	BUS_SORTED_LOG
    } SearchAlgorithm;

protected:

    bool enableOccupancyTrace;

	Metric* performanceMetric;

	PerformanceEstimationMethod perfEstMethod;

	InterferenceManager* intManager;
	Tick period;
	MissBandwidthPolicyEvent* policyEvent;
	MissBandwidthTraceEvent* traceEvent;

	std::vector<Addr> cummulativeMemoryRequests;
	std::vector<Addr> cummulativeCommittedInsts;

	SearchAlgorithm searchAlgorithm;
	int iterationLatency;

	RequestTrace aloneIPCTrace;

	RequestTrace measurementTrace;
	std::vector<BaseCache* > caches;

	RequestTrace searchTrace;

	std::vector<double> aloneIPCEstimates;
	std::vector<double> aloneCycles;
	std::vector<double> avgLatencyAloneIPCModel;

	std::vector<double> computedOverlap;

//	bool dumpInitalized;
//	Tick dumpSearchSpaceAt;

	RequestTrace predictionTrace;

	std::vector<double> currentRequestProjection;
	std::vector<double> currentLatencyProjection;
	std::vector<double> currentIPCProjection;
	std::vector<double> currentInterferenceMissProjection;

	std::vector<double> bestRequestProjection;
	std::vector<double> bestLatencyProjection;
	std::vector<double> bestIPCProjection;
	std::vector<double> bestInterferenceMissProjection;

	std::vector<double> requestAccumulator;
	std::vector<double> requestSqAccumulator;
	std::vector<double> avgReqsPerSample;
	std::vector<double> reqsPerSampleStdDev;

	std::vector<RequestTrace> comInstModelTraces;
	std::vector<Tick> comInstModelTraceCummulativeInst;

	bool usePersistentAllocations;

	RequestTrace numMSHRsTrace;

	std::vector<std::vector<double> > mostRecentMWSEstimate;
	std::vector<std::vector<double> > mostRecentMLPEstimate;

	PerformanceMeasurement* currentMeasurements;

	int maxMSHRs;
	int cpuCount;

	bool measurementsValid;

	void initProjectionTrace(int cpuCount);
	void traceBestProjection();
	void traceNumMSHRs();
	void initNumMSHRsTrace(int cpuCount);
	void initSearchTrace(int cpuCount, SearchAlgorithm searchAlg);

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
	void tracePerformance(std::vector<double>& sharedCycles, std::vector<double>& speedups);

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
		                       double responsesWhileStalled,
		                       int cpuID);

//	void dumpSearchSpace(std::vector<int>* mhaConfig, double metricValue);

	double squareRoot(double num);

	void updateBestProjections();

public:

	BasePolicy(std::string _name,
			   InterferenceManager* _intManager,
			   Tick _period,
			   int _cpuCount,
			   PerformanceEstimationMethod _perfEstMethod,
			   bool _persistentAllocations,
			   int _iterationLatency,
			   Metric* _performanceMetric,
			   bool _enforcePolicy);

	~BasePolicy();

	void handlePolicyEvent();

	void handleTraceEvent();

	void registerCache(BaseCache* _cache, int _cpuID, int _maxMSHRs);

	void addTraceEntry(PerformanceMeasurement* measurement);

	virtual void runPolicy(PerformanceMeasurement measurements) = 0;

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
	static PerformanceEstimationMethod parsePerformanceMethod(std::string methodName);
	static SearchAlgorithm parseSearchAlgorithm(std::string methodName);
	static Metric* parseOptimizationMetric(std::string metricName);

	void implementMHA(std::vector<int> bestMHA);

	virtual bool doEvaluation(int cpuID) = 0;
};

class MissBandwidthPolicyEvent : public Event{
private:
	BasePolicy* policy;
	Tick period;

public:
	MissBandwidthPolicyEvent(BasePolicy* _policy, Tick _period):
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
	BasePolicy* policy;
	Tick period;

public:
	MissBandwidthTraceEvent(BasePolicy* _policy, Tick _period):
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

class MissBandwidthImplementMHAEvent : public Event{

private:
	BasePolicy* policy;
	std::vector<int> bestMHA;

public:
	MissBandwidthImplementMHAEvent(BasePolicy* _policy, std::vector<int> _bestMHA):
		Event(&mainEventQueue), policy(_policy), bestMHA(_bestMHA){

	}

	void process(){
		policy->implementMHA(bestMHA);
		assert(!scheduled());
		delete this;
	}

	virtual const char *description() {
		return "Miss Bandwidth Implement MHA Event";
	}
};

class PolicyCallback : public Callback
{
    private:
	BasePolicy *bp;
    public:
        PolicyCallback(BasePolicy *p) : bp(p) {}
        virtual void process() {
        	std::cout << "issuing callback\n";
        	bp->handlePolicyEvent();
        };
};

#endif /* BASE_POLICY_HH_ */
