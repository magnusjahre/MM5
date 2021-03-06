
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

class CacheAccessMeasurement{
public:
	int hits;
	int accesses;
	int interferenceMisses;
	int writebacks;

	CacheAccessMeasurement(){
		hits = 0;
		accesses = 0;
		interferenceMisses = 0;
		writebacks = 0;
	}

	void add(int _hits, int _accesses, int _interferenceMisses, int _writebacks){
		hits += _hits;
		accesses += _accesses;
		interferenceMisses += _interferenceMisses;
		writebacks += _writebacks;
	}
};

class ASMValues{
public:
	// CAR Alone values
	double sharedLLCAccesses;
	double atdAccesses;
	double totalCycles;
	double pmHitFraction;
	double pmHitEstimate;
	double contentionMisses;
	double avgMissTime;
	double avgHitTime;
	double excessCycles;
	double pmMissFraction;
	double pmMissEstimate;
	double avgQueuingDelay;
	double queuingDelay;
	double carAlone;

	// CAR Shared Values
	double cpuSharedLLCAccesses;
	double carShared;

	// Other
	double slowdown;
	int numEpochs;

	ASMValues(){
		sharedLLCAccesses = 0.0;
		atdAccesses = 0.0;
		totalCycles = 0.0;
		pmHitFraction = 0.0;
		pmHitEstimate = 0.0;
		contentionMisses = 0.0;
		avgMissTime = 0.0;
		avgHitTime = 0.0;
		excessCycles = 0.0;
		pmMissFraction = 0.0;
		pmMissEstimate = 0.0;
		avgQueuingDelay = 0.0;
		queuingDelay = 0.0;
		carAlone = 0.0;
		cpuSharedLLCAccesses = 0.0;
		carShared = 0.0;
		slowdown = 0.0;
		numEpochs = 0.0;
	}
};

class ASREpochMeasurements{
public:
	enum ASR_COUNTER_TYPE{
		EPOCH_HIT,
		EPOCH_MISS,
		EPOCH_HIT_TIME,
		EPOCH_MISS_TIME,
		EPOCH_ATD_HIT,
		EPOCH_ATD_MISS,
		EPOCH_QUEUEING_CYCLES,
		NUM_EPOCH_COUNTERS
	};

	static char* ASR_COUNTER_NAMES[NUM_EPOCH_COUNTERS];

	int highPriCPU;
	int epochCount;
	int cpuCount;
	Tick epochStartAt;

	std::vector<Tick> data;
	std::vector<int> cpuATDHits;
	std::vector<int> cpuATDMisses;
	std::vector<int> cpuSharedLLCAccesses;

	std::vector<int> outstandingMissCnt;
	std::vector<Tick> firstMissAt;
	std::vector<int> outstandingHitCnt;
	std::vector<Tick> firstHitAt;

	ASREpochMeasurements(){
		highPriCPU = -1;
		epochCount = 0;
		cpuCount = -1;
		epochStartAt = 0;
	}

	ASREpochMeasurements(int _cpuCount){
		epochCount = 0;
		cpuCount = _cpuCount;
		outstandingMissCnt = std::vector<int>(_cpuCount, 0);
		outstandingHitCnt = std::vector<int>(_cpuCount, 0);
		firstMissAt = std::vector<Tick>(_cpuCount, 0);
		firstHitAt  = std::vector<Tick>(_cpuCount, 0);

		quantumReset();
	}

	void finalizeEpoch(int epochCycles);

	void epochReset(){
		highPriCPU = -1;
		epochStartAt = curTick;
		data = std::vector<Tick>(NUM_EPOCH_COUNTERS, 0);
	}

	void quantumReset(){
		epochReset();
		cpuATDHits = std::vector<int>(cpuCount, 0);
		cpuATDMisses = std::vector<int>(cpuCount, 0);
		cpuSharedLLCAccesses = std::vector<int>(cpuCount, 0);
		epochCount = 0;
	}

	void addValue(int cpuID, ASR_COUNTER_TYPE type, int value = 1);

	void addValues(ASREpochMeasurements* measurements);

	void llcEvent(int cpuID, bool issued, ASR_COUNTER_TYPE type);

	void computeCARAlone(int cpuID, Tick epochLength, ASMValues* asmvals);

	void computeCARShared(int cpuID, Tick epochLength, ASMValues* asmvals);

	double safeDiv(double numerator, double denominator);
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

	std::vector<Tick> cpuSharedStallAccumulator;
	std::vector<Tick> cpuComTraceStallCycles;

	std::vector<Tick> cpuComTraceTotalRoundtrip;
	std::vector<int> cpuComTraceTotalRoundtripRequests;

	std::vector<int> llcMissesForLatencyTraceAccumulator;

	std::vector<PerformanceModel* > performanceModels;

public:

	ASREpochMeasurements asrEpocMeasurements;

	CacheInterference* cacheInterference;

	typedef enum{
			InterconnectEntry,
			InterconnectRequestQueue,
			InterconnectRequestTransfer,
			InterconnectResponseQueue,
			InterconnectResponseTransfer,
			InterconnectDelivery,
			CacheCapacityRequest,
			CacheCapacityResponse,
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
	Stats::Vector<> busLoads;

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

	Stats::Vector<> cpuSharedStallCycles;
	Stats::Vector<> cpuPrivateStallCycles;
	Stats::Vector<> cpuOtherStallCycles;
	Stats::Vector<> cpuMemIndStallCycles;
	Stats::Vector<> cpuCommitCycles;

	Stats::Vector<> numCpuStalls;
	Stats::Formula cpuComputePercentage;

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

	PerformanceModelMeasurements buildModelMeasurements(int cpuID, int committedInstructions, Tick ticksInSample, OverlapStatistics ols);

	void busWritebackCompleted(MemReqPtr& req, Tick finishedAt);

	void setASRHighPriCPUID(int newHighPriCPUID);
};

#endif
