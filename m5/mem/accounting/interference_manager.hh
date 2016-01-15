
#ifndef INTERFERNCE_MANAGER_HH
#define INTERFERNCE_MANAGER_HH

#include "sim/sim_object.hh"
#include "base/misc.hh"
#include "mem/mem_req.hh"
#include "base/statistics.hh"
#include "mem/requesttrace.hh"
#include "mem/cache/cache_interference.hh"
#include "mem/policy/base_policy.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "mem/cache/base_cache.hh"
#include "mem/bus/bus.hh"
#include "mem/policy/performance_measurement.hh"
#include "mem/accounting/memory_overlap_estimator.hh"
#include "mem/policy/performance_model.hh"
#include "mem/accounting/performance_model_measurements.hh"

class CacheInterference;
class BasePolicy;
class BaseCache;
class Bus;
class MSHROccupancy;
class OverlapStatistics;
class PerformanceModel;

#include <vector>

class RateMeasurement{
public:
	double nominator;
	double denominator;

	RateMeasurement(double _nominator, double _denominator){
		nominator = _nominator;
		denominator = _denominator;
	}

	double getRate(){
		return nominator / denominator;
	}
};

class InterferenceManager : public SimObject{

private:

	int cpuCount;
	int maxMSHRs;

	bool includeStores;

	BasePolicy* missBandwidthPolicy;
	std::vector<FullCPU*> fullCPUs;
	std::vector<BaseCache*> lastPrivateCaches;
	std::vector<Bus*> memoryBuses;
	std::vector<BaseCache* > sharedCaches;
	std::vector<BaseCache* > l1DataCaches;
	std::vector<int> requestsSinceLastSample;

	std::vector<double> sharedLatencyAccumulator;
	std::vector<double> interferenceAccumulator;
	std::vector<std::vector<double> > sharedLatencyBreakdownAccumulator;
	std::vector<std::vector<double> > commitTraceSharedLatencyBreakdownAccumulator;
	std::vector<std::vector<double> > interferenceBreakdownAccumulator;
	std::vector<int> currentRequests;
	std::vector<int> commitTraceBreakdownRequests;

	std::vector<double> privateLatencyAccumulator;
	std::vector<std::vector<double> > privateLatencyBreakdownAccumulator;
	std::vector<int> privateRequests;

	std::vector<double> l1HitAccumulator;
	std::vector<int> l1HitRequests;
	std::vector<double> l1BlockedAccumulator;

	std::vector<std::vector<Tick> > interferenceSum;
	std::vector<std::vector<int> > numInterferenceReqs;

	std::vector<std::vector<Tick> > latencySum;
	std::vector<std::vector<int> > numLatencyReqs;

	std::vector<Tick> instTraceInterferenceSum;
	std::vector<Tick> instTraceLatencySum;
	std::vector<int> instTraceRequests;
	std::vector<int> instTraceHiddenLoads;

	std::vector<Tick> instTraceStoreInterferenceSum;
	std::vector<Tick> instTraceStoreLatencySum;
	std::vector<int> instTraceStoreRequests;

	std::vector<int> totalRequestCount;
	std::vector<Tick> runningLatencySum;

	std::vector<int> interferenceMisses;
	std::vector<int> regularMisses;

	std::vector<Tick> commitTraceCommitCycles;
	std::vector<Tick> commitTraceMemIndStall;
	std::vector<Tick> commitTracePrivateStall;
	std::vector<Tick> commitTraceWriteStall;
	std::vector<Tick> commitTracePrivateBlockedStall;
	std::vector<Tick> commitTraceEmptyROBStall;

	std::vector<MemoryOverlapEstimator*> overlapEstimators;

	int intManCPUCount;

	bool traceStarted;
	int sampleSize;
	int resetInterval;

	std::vector<RequestTrace> estimateTraces;
	std::vector<RequestTrace> latencyTraces;
	std::vector<RequestTrace> privateLatencyTraces;

	std::vector<double> traceInterference(int fromCPU, std::vector<double> avgLats);

	std::vector<double> traceLatency(int fromCPU);

	void tracePrivateLatency(int fromCPU, int committedInstructions);

	void traceMisses(int fromCPU);

	void resetInterferenceMeasurements(int fromCPU);

	std::vector<RequestTrace> aloneMissTrace;

	std::vector<Tick> cpuStallAccumulator;
	std::vector<Tick> cpuComTraceStallCycles;

	std::vector<Tick> cpuComTraceTotalRoundtrip;
	std::vector<int> cpuComTraceTotalRoundtripRequests;

	std::vector<PerformanceModel* > performanceModels;

public:

	CacheInterference* cacheInterference;

	typedef enum{
			InterconnectEntry,
			InterconnectRequestQueue,
			InterconnectRequestTransfer,
			InterconnectResponseQueue,
			InterconnectResponseTransfer,
			InterconnectDelivery,
			CacheCapacity,
			MemoryBusEntry,
			MemoryBusQueue,
			MemoryBusService,
			NUM_LAT_TYPES
		} LatencyType;

	static char* latencyStrings[NUM_LAT_TYPES];

protected:

	Stats::Vector<> interference[NUM_LAT_TYPES];

	Stats::Vector<> latencies[NUM_LAT_TYPES];

	Stats::Vector<> roundTripLatencies;

	Stats::Vector<> requests;

	Stats::Formula avgInterference[NUM_LAT_TYPES];

	Stats::Formula avgLatency[NUM_LAT_TYPES];
	Stats::Formula noBusLatency;
	Stats::Formula busLatency;

	Stats::Formula avgRoundTripLatency;


	Stats::Vector<> totalLatency;
	Stats::Vector<> totalInterference;
	Stats::Formula avgTotalLatency;
	Stats::Formula avgTotalInterference;
	Stats::Formula avgInterferencePercentage;

	Stats::Vector<> totalPrivateMemsysLatency;
	Stats::Vector<> privateMemsysRequests;
	Stats::Formula avgPrivateMemsysLatency;

	Stats::Vector<> totalL1HitLatency;
	Stats::Vector<> totalL1HitRequests;
	Stats::Formula avgL1HitLatency;

	Stats::Vector<> totalMemsysEntryLatency;

	Stats::Vector<> cpuStallCycles;
	Stats::Vector<> numCpuStalls;
	Stats::Formula cpuStallPercentage;
	Stats::Formula avgCpuStallLength;
	Stats::Formula cpuComputeCycles;

public:

	InterferenceManager(std::string _name, int _cpu_count, int _sample_size, int _num_reqs_at_reset);

	void regStats();

	void resetStats();

	void addInterference(LatencyType t, MemReqPtr& req, int interference);

	void incrementInterferenceRequestCount(LatencyType t, MemReqPtr& req);

	void addLatency(LatencyType t, MemReqPtr& req, int latency);

	void addPrivateLatency(LatencyType t, MemReqPtr& req, int latency);

	void incrementPrivateRequestCount(MemReqPtr& req);

	void addSharedReqTotalRoundtrip(MemReqPtr& req, Tick latency);

	void addL1Hit(int cpuID, Tick latency);

	void incrementLatencyRequestCount(LatencyType t, MemReqPtr& req);

	void incrementTotalReqCount(MemReqPtr& req, int roundTripLatency);

	void registerCacheInterferenceObj(CacheInterference* ci);

	void addCacheResult(MemReqPtr& req);

	PerformanceMeasurement buildInterferenceMeasurement(int period);

	void registerMissBandwidthPolicy(BasePolicy* policy);

	void registerLastLevelPrivateCache(BaseCache* cache, int cpuID, int maxMSHRs);
	void registerSharedCache(BaseCache* cache);
	void registerL1DataCache(int cpuID, BaseCache* cache);

	void registerCPU(FullCPU* cpu, int cpuID);

	int getCPUCount(){
		return cpuCount;
	}

	void registerBus(Bus* bus);

	bool checkForStore(MemReqPtr& req);

//	void setStalledForMemory(int cpuID, int detectionDelay);
//	void clearStalledForMemory(int cpuID, bool incrementNumStalls = true);
//	bool isStalledForMemory(int cpuID);

	void addStallCycles(int cpuID, Tick cpuStalledFor, bool isShared, bool incrementNumStalls, Tick writeStall, Tick blockedStall, Tick emptyROBStall);

	double getAvgNoBusLat(double avgRoundTripLatency, int cpuID);

	void updatePrivPerfEst(int cpuID, int committedInstructions, Tick ticksInSample, OverlapStatistics ols, double cwp, int numWriteStalls, Tick boisAloneStallEst);

	void enableMSHROccupancyTrace();

	std::vector<MSHROccupancy>* getMSHROccupancyList(int cpuID);

	void clearMSHROccupancyLists();

	void addCommitCycle(int cpuID);

	void addMemIndependentStallCycle(int cpuID);

	void addL1BlockedCycle(int cpuID);

	void hiddenLoadDetected(int cpuID);

	void itcaIntertaskMiss(int cpuID, Addr addr, bool isInstructionMiss, Addr cpuAddr);

	void registerMemoryOverlapEstimator(MemoryOverlapEstimator* moe, int cpuID);

	PerformanceModelMeasurements buildModelMeasurements(int committedInstructions, Tick ticksInSample, OverlapStatistics ols);

	void busWritebackCompleted(MemReqPtr& req, Tick finishedAt);
};

#endif
