
#ifndef INTERFERNCE_MANAGER_HH
#define INTERFERNCE_MANAGER_HH

#include "sim/sim_object.hh"
#include "base/misc.hh"
#include "mem_req.hh"
#include "base/statistics.hh"

#include <vector>

class InterferenceManager : public SimObject{

private:

	std::vector<std::vector<Tick> > interferenceSum;
	std::vector<std::vector<int> > numInterferenceReqs;

	std::vector<std::vector<Tick> > latencySum;
	std::vector<std::vector<int> > numLatencyReqs;

	std::vector<int> totalRequestCount;

	int intManCPUCount;

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

	InterferenceManager(std::string _name, int _cpu_count);

	void regStats();

	void addInterference(LatencyType t, MemReqPtr& req, int interference);

	void incrementInterferenceRequestCount(LatencyType t, MemReqPtr& req);

	void addLatency(LatencyType t, MemReqPtr& req, int latency);

	void incrementLatencyRequestCount(LatencyType t, MemReqPtr& req);

	void incrementTotalReqCount(MemReqPtr& req, int roundTripLatency);
};

#endif
