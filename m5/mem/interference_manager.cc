
#include "interference_manager.hh"

#include "sim/builder.hh"

using namespace std;

char*
InterferenceManager::latencyStrings[NUM_LAT_TYPES] = {
	(char*) "ic_entry",
	(char*) "ic_request_queue",
	(char*) "ic_request_transfer",
	(char*) "ic_response_queue",
	(char*) "ic_response_transfer",
	(char*) "ic_delivery",
	(char*) "cache_capacity",
	(char*) "bus_entry",
	(char*) "bus_queue",
	(char*) "bus_service"
};

InterferenceManager::InterferenceManager(std::string _name, int _cpu_count)
: SimObject(_name){

	latencySum.resize(_cpu_count, vector<Tick>(NUM_LAT_TYPES, 0));
	numLatencyReqs.resize(_cpu_count, vector<int>(NUM_LAT_TYPES, 0));

	interferenceSum.resize(_cpu_count, vector<Tick>(NUM_LAT_TYPES, 0));
	numInterferenceReqs.resize(_cpu_count, vector<int>(NUM_LAT_TYPES, 0));

	totalRequestCount.resize(_cpu_count, 0);

	intManCPUCount = _cpu_count;
}

void
InterferenceManager::regStats(){

	for(int i=0;i<NUM_LAT_TYPES;i++){

		stringstream namestream;
		namestream << name() << ".interference_"  << latencyStrings[i];
		stringstream descrstream;
		descrstream << "number of " << latencyStrings[i] << " interference cycles";

		interference[i]
		             .init(intManCPUCount)
		             .name(namestream.str().c_str())
		             .desc(descrstream.str().c_str());

		stringstream namestream2;
		namestream2 << name() << ".latency_"  << latencyStrings[i];
		stringstream descrstream2;
		descrstream2 << "number of " << latencyStrings[i] << " latency cycles";
		latencies[i]
		          .init(intManCPUCount)
		          .name(namestream2.str().c_str())
		          .desc(descrstream2.str().c_str());
	}

	roundTripLatencies
		.init(intManCPUCount)
		.name(name() + ".round_trip_latency")
		.desc("total round trip latency");

	requests
		.init(intManCPUCount)
		.name(name() + ".requests")
		.desc("total number of requests");

	avgRoundTripLatency
		.name(name() + ".avg_round_trip_latency")
		.desc("average total round trip latency");

	avgRoundTripLatency = roundTripLatencies / requests;

	for(int i=0;i<NUM_LAT_TYPES;i++){

		stringstream avgIntName;
		stringstream avgIntDescr;

		avgIntName << name() << ".avg_interference_" << latencyStrings[i];
		avgIntDescr << "Average " << latencyStrings[i] << " interference cycles per request";

		avgInterference[i]
			.name(avgIntName.str().c_str())
			.desc(avgIntDescr.str().c_str());


		avgInterference[i] = interference[i] / requests;

		stringstream avgLatName;
		stringstream avgLatDescr;

		avgLatName << name() << ".avg_latency_" << latencyStrings[i];
		avgLatDescr << "Average " << latencyStrings[i] << " latency cycles per request";

		avgLatency[i]
			.name(avgLatName.str().c_str())
			.desc(avgLatDescr.str().c_str());

		avgLatency[i] = latencies[i] / requests;
	}
}

void
InterferenceManager::addInterference(LatencyType t, MemReqPtr& req, int interferenceTicks){
	assert(req->cmd == Read);
	assert(req->adaptiveMHASenderID != -1);
	interferenceSum[req->adaptiveMHASenderID][t] += interferenceTicks;
	interference[t][req->adaptiveMHASenderID] += interferenceTicks;
}

void
InterferenceManager::incrementInterferenceRequestCount(LatencyType t, MemReqPtr& req){
	assert(req->cmd == Read);
	assert(req->adaptiveMHASenderID != -1);
	numInterferenceReqs[req->adaptiveMHASenderID][t]++;
}


void
InterferenceManager::addLatency(LatencyType t, MemReqPtr& req, int latency){
	assert(req->cmd == Read);
	assert(req->adaptiveMHASenderID != -1);
	latencySum[req->adaptiveMHASenderID][t] += latency;
	latencies[t][req->adaptiveMHASenderID] += latency;
}

void
InterferenceManager::incrementLatencyRequestCount(LatencyType t, MemReqPtr& req){
	assert(req->cmd == Read);
	assert(req->adaptiveMHASenderID != -1);
	numLatencyReqs[req->adaptiveMHASenderID][t]++;
}

void
InterferenceManager::incrementTotalReqCount(MemReqPtr& req, int roundTripLatency){
	assert(req->cmd == Read);
	assert(req->adaptiveMHASenderID != -1);

	totalRequestCount[req->adaptiveMHASenderID]++;

	roundTripLatencies[req->adaptiveMHASenderID] += roundTripLatency;
	requests[req->adaptiveMHASenderID]++;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(InterferenceManager)
    Param<int> cpu_count;
END_DECLARE_SIM_OBJECT_PARAMS(InterferenceManager)

BEGIN_INIT_SIM_OBJECT_PARAMS(InterferenceManager)
	INIT_PARAM_DFLT(cpu_count, "Number of CPUs", -1)
END_INIT_SIM_OBJECT_PARAMS(InterferenceManager)

CREATE_SIM_OBJECT(InterferenceManager)
{
    return new InterferenceManager(getInstanceName(), cpu_count);
}

REGISTER_SIM_OBJECT("InterferenceManager", InterferenceManager)

#endif
