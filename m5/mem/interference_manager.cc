
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

InterferenceManager::InterferenceManager(std::string _name, int _cpu_count, int _sample_size, int _num_reqs_at_reset)
: SimObject(_name){

	latencySum.resize(_cpu_count, vector<Tick>(NUM_LAT_TYPES, 0));
	numLatencyReqs.resize(_cpu_count, vector<int>(NUM_LAT_TYPES, 0));

	interferenceSum.resize(_cpu_count, vector<Tick>(NUM_LAT_TYPES, 0));
	numInterferenceReqs.resize(_cpu_count, vector<int>(NUM_LAT_TYPES, 0));

	totalRequestCount.resize(_cpu_count, 0);
	runningLatencySum.resize(_cpu_count, 0);

	traceStarted = false;

	intManCPUCount = _cpu_count;
	sampleSize = _sample_size;
	resetInterval = _num_reqs_at_reset;

	estimateTraces.resize(_cpu_count, RequestTrace());
	latencyTraces.resize(_cpu_count, RequestTrace());

	for(int i=0;i<_cpu_count;i++){

		stringstream etitle;
		etitle << "CPU" << i << "InterferenceTrace";
		stringstream ltitle;
		ltitle << "CPU" << i << "LatencyTrace";

		estimateTraces[i] = RequestTrace(etitle.str(),"");
		latencyTraces[i] = RequestTrace(ltitle.str(),"");

		vector<string> traceHeaders;
		traceHeaders.push_back("Requests");
		traceHeaders.push_back("Total");
		for(int j=0;j<NUM_LAT_TYPES;j++){
			traceHeaders.push_back(latencyStrings[j]);
		}

		estimateTraces[i].initalizeTrace(traceHeaders);
		latencyTraces[i].initalizeTrace(traceHeaders);
	}
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
InterferenceManager::resetStats(){
	if(curTick != 0) traceStarted = true;
	for(int i=0;i<intManCPUCount;i++) resetInterferenceMeasurements(i);
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

	runningLatencySum[req->adaptiveMHASenderID] += roundTripLatency;
	totalRequestCount[req->adaptiveMHASenderID]++;

	roundTripLatencies[req->adaptiveMHASenderID] += roundTripLatency;
	requests[req->adaptiveMHASenderID]++;

	if(totalRequestCount[req->adaptiveMHASenderID] % sampleSize == 0 && traceStarted){
		vector<double> avgLats = traceLatency(req->adaptiveMHASenderID);
		traceInterference(req->adaptiveMHASenderID, avgLats);
	}

	if(resetInterval != -1 && totalRequestCount[req->adaptiveMHASenderID] % resetInterval == 0 && traceStarted){
		resetInterferenceMeasurements(req->adaptiveMHASenderID);
	}
}

void
InterferenceManager::traceInterference(int fromCPU, vector<double> avgLats){

	double tmpInterferenceSum = 0;

	for(int i=0;i<NUM_LAT_TYPES;i++) tmpInterferenceSum += (double) interferenceSum[fromCPU][i];

	double avgInterference = tmpInterferenceSum / totalRequestCount[fromCPU];

	std::vector<RequestTraceEntry> data;
	data.resize(NUM_LAT_TYPES+2, RequestTraceEntry(0.0));

	data[0] = requests[fromCPU].value();
	data[1] = avgLats[0] - avgInterference;

	for(int i=0;i<NUM_LAT_TYPES;i++){
		double tmpAvgInt = (double) ((double) interferenceSum[fromCPU][i] / (double) totalRequestCount[fromCPU] );
		data[i+2] = avgLats[i+1] - tmpAvgInt;
	}

	estimateTraces[fromCPU].addTrace(data);

}

vector<double>
InterferenceManager::traceLatency(int fromCPU){

	vector<double> avgLats;
	avgLats.resize(NUM_LAT_TYPES+1, 0.0);
	avgLats[0] = (double) ((double) runningLatencySum[fromCPU] / (double) totalRequestCount[fromCPU]);
	for(int i=0;i<NUM_LAT_TYPES;i++){
		avgLats[i+1] = (double) ((double) latencySum[fromCPU][i] / (double) totalRequestCount[fromCPU]);
	}

	std::vector<RequestTraceEntry> data;
	data.resize(NUM_LAT_TYPES+2, RequestTraceEntry(0.0));

	data[0] = requests[fromCPU].value();
	data[1] = avgLats[0];

	for(int i=0;i<NUM_LAT_TYPES;i++){
		data[i+2] = avgLats[i+1];
	}

	latencyTraces[fromCPU].addTrace(data);

	return avgLats;
}

void
InterferenceManager::resetInterferenceMeasurements(int fromCPU){
	totalRequestCount[fromCPU] = 0;
	runningLatencySum[fromCPU] = 0;

	for(int i=0;i<NUM_LAT_TYPES;i++) latencySum[fromCPU][i] = 0;

	for(int i=0;i<NUM_LAT_TYPES;i++) interferenceSum[fromCPU][i] = 0;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(InterferenceManager)
    Param<int> cpu_count;
    Param<int> sample_size;
    Param<int> reset_interval;
END_DECLARE_SIM_OBJECT_PARAMS(InterferenceManager)

BEGIN_INIT_SIM_OBJECT_PARAMS(InterferenceManager)
	INIT_PARAM_DFLT(cpu_count, "Number of CPUs", -1),
	INIT_PARAM_DFLT(sample_size, "Number of requests", 50),
	INIT_PARAM_DFLT(reset_interval, "Number of requests after which the measurements are reset", -1)
END_INIT_SIM_OBJECT_PARAMS(InterferenceManager)

CREATE_SIM_OBJECT(InterferenceManager)
{
    return new InterferenceManager(getInstanceName(),
								   cpu_count,
								   sample_size,
								   reset_interval);
}

REGISTER_SIM_OBJECT("InterferenceManager", InterferenceManager)

#endif
