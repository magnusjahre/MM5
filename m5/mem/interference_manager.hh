
#ifndef INTERFERNCE_MANAGER_HH
#define INTERFERNCE_MANAGER_HH

#include "sim/sim_object.hh"
#include "base/misc.hh"
#include "mem_req.hh"
#include "base/statistics.hh"
#include "requesttrace.hh"
#include "mem/cache/cache_interference.hh"
#include "mem/cache/miss/policy/miss_bandwidth_policy.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "cache/base_cache.hh"

class CacheInterference;
class MissBandwidthPolicy;
class BaseCache;

#include <vector>

class PerformanceMeasurement{
private:
	int cpuCount;
	int numIntTypes;
	int maxMSHRs;

public:
	std::vector<int> committedInstructions;
	std::vector<int> requestsInSample;

	std::vector<std::vector<double> > mlpEstimate;

	std::vector<double> sharedLatencies;
	std::vector<double> estimatedPrivateLatencies;
	std::vector<std::vector<double> > latencyBreakdown;
	std::vector<std::vector<double> > privateLatencyBreakdown;

	PerformanceMeasurement(int _cpuCount, int _numIntTypes, int _maxMSHRs);

	void addInterferenceData(std::vector<std::vector<double> > sharedAvgLatencies,
							 std::vector<std::vector<double> > privateAvgEstimation);

	std::vector<std::string> getTraceHeader();
	std::vector<RequestTraceEntry> createTraceLine();
};

class InterferenceManager : public SimObject{

private:

	int cpuCount;
	int maxMSHRs;

	MissBandwidthPolicy* missBandwidthPolicy;
	std::vector<FullCPU*> fullCPUs;
	std::vector<BaseCache*> lastPrivateCaches;
	std::vector<int> requestsSinceLastSample;

	std::vector<std::vector<double> > currentAvgLatencyMeasurement;
	std::vector<std::vector<double> > currentAvgPrivateLatencyEstimation;

	std::vector<std::vector<Tick> > interferenceSum;
	std::vector<std::vector<int> > numInterferenceReqs;

	std::vector<std::vector<Tick> > latencySum;
	std::vector<std::vector<int> > numLatencyReqs;

	std::vector<int> totalRequestCount;
	std::vector<Tick> runningLatencySum;

	std::vector<int> interferenceMisses;
	std::vector<int> regularMisses;

	int intManCPUCount;

	bool traceStarted;
	int sampleSize;
	int resetInterval;

	std::vector<RequestTrace> estimateTraces;
	std::vector<RequestTrace> latencyTraces;

	std::vector<CacheInterference* > cacheInterferenceObjs;

	std::vector<double> traceInterference(int fromCPU, std::vector<double> avgLats);

	std::vector<double> traceLatency(int fromCPU);

	void traceMisses(int fromCPU);

	void resetInterferenceMeasurements(int fromCPU);

	std::vector<RequestTrace> aloneMissTrace;
public:
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

	Stats::Formula avgRoundTripLatency;


	Stats::Vector<> totalLatency;
	Stats::Vector<> totalInterference;
	Stats::Formula avgTotalLatency;
	Stats::Formula avgTotalInterference;
	Stats::Formula avgInterferencePercentage;

public:

	InterferenceManager(std::string _name, int _cpu_count, int _sample_size, int _num_reqs_at_reset);

	void regStats();

	void resetStats();

	void addInterference(LatencyType t, MemReqPtr& req, int interference);

	void incrementInterferenceRequestCount(LatencyType t, MemReqPtr& req);

	void addLatency(LatencyType t, MemReqPtr& req, int latency);

	void incrementLatencyRequestCount(LatencyType t, MemReqPtr& req);

	void incrementTotalReqCount(MemReqPtr& req, int roundTripLatency);

	void registerCacheInterferenceObj(CacheInterference* ci);

	void addCacheResult(MemReqPtr& req);

	void buildInterferenceMeasurement();

	void registerMissBandwidthPolicy(MissBandwidthPolicy* policy);

	void registerLastLevelPrivateCache(BaseCache* cache, int cpuID, int maxMSHRs);

	void registerCPU(FullCPU* cpu, int cpuID);

	int getCPUCount(){
		return cpuCount;
	}
};

#endif
