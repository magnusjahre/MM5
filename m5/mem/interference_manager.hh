
#ifndef INTERFERNCE_MANAGER_HH
#define INTERFERNCE_MANAGER_HH

#include "sim/sim_object.hh"
#include "base/misc.hh"
#include "mem_req.hh"
#include "base/statistics.hh"
#include "requesttrace.hh"
#include "mem/cache/cache_interference.hh"

class CacheInterference;

#include <vector>

class InterferenceManager : public SimObject{

private:

	std::vector<std::vector<Tick> > interferenceSum;
	std::vector<std::vector<int> > numInterferenceReqs;

	std::vector<std::vector<Tick> > latencySum;
	std::vector<std::vector<int> > numLatencyReqs;

	std::vector<int> totalRequestCount;
	std::vector<Tick> runningLatencySum;

	int intManCPUCount;

	bool traceStarted;
	int sampleSize;
	int resetInterval;

	std::vector<RequestTrace> estimateTraces;
	std::vector<RequestTrace> latencyTraces;

	std::vector<CacheInterference* > cacheInterferenceObjs;

	void traceInterference(int fromCPU, std::vector<double> avgLats);

	std::vector<double> traceLatency(int fromCPU);

	void resetInterferenceMeasurements(int fromCPU);

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
};

#endif
