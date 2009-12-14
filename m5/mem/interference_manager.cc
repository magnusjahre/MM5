
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

InterferenceManager::InterferenceManager(std::string _name,
										 int _cpu_count,
										 int _sample_size,
										 int _num_reqs_at_reset)
: SimObject(_name){

	cpuCount = _cpu_count;

	fullCPUs.resize(_cpu_count, NULL);
	lastPrivateCaches.resize(_cpu_count, NULL);
	requestsSinceLastSample.resize(_cpu_count, 0);
	maxMSHRs = 0;

	sharedLatencyAccumulator.resize(_cpu_count, 0);
	interferenceAccumulator.resize(_cpu_count, 0);
	sharedLatencyBreakdownAccumulator.resize(_cpu_count, vector<double>(NUM_LAT_TYPES, 0));
	interferenceBreakdownAccumulator.resize(_cpu_count, vector<double>(NUM_LAT_TYPES, 0));
	currentRequests.resize(_cpu_count, 0);

	latencySum.resize(_cpu_count, vector<Tick>(NUM_LAT_TYPES, 0));
	numLatencyReqs.resize(_cpu_count, vector<int>(NUM_LAT_TYPES, 0));

	interferenceSum.resize(_cpu_count, vector<Tick>(NUM_LAT_TYPES, 0));
	numInterferenceReqs.resize(_cpu_count, vector<int>(NUM_LAT_TYPES, 0));

	regularMisses.resize(_cpu_count, 0);
	interferenceMisses.resize(_cpu_count, 0);

	totalRequestCount.resize(_cpu_count, 0);
	runningLatencySum.resize(_cpu_count, 0);


	instTraceInterferenceSum.resize(_cpu_count, 0);
	instTraceLatencySum.resize(_cpu_count, 0);
	instTraceRequests.resize(_cpu_count, 0);

	missBandwidthPolicy = NULL;

	traceStarted = false;

	intManCPUCount = _cpu_count;
	sampleSize = _sample_size;
	resetInterval = _num_reqs_at_reset;

	if(resetInterval != -1){
		if(resetInterval < sampleSize){
			fatal("InterferenceManager: resetting measurements more often than they are taken does not make sense.");
		}

		if(resetInterval % sampleSize != 0){
			fatal("InterferenceManager: The reset interval must be a multiple of the sample size");
		}
	}

	estimateTraces.resize(_cpu_count, RequestTrace());
	latencyTraces.resize(_cpu_count, RequestTrace());
	aloneMissTrace.resize(_cpu_count, RequestTrace());

	for(int i=0;i<_cpu_count;i++){

		stringstream etitle;
		etitle << "CPU" << i << "InterferenceTrace";
		stringstream ltitle;
		ltitle << "CPU" << i << "LatencyTrace";

		estimateTraces[i] = RequestTrace(etitle.str(),"", 1);
		latencyTraces[i] = RequestTrace(ltitle.str(),"", 1);

		vector<string> traceHeaders;
		traceHeaders.push_back("Requests");
		traceHeaders.push_back("Total");
		for(int j=0;j<NUM_LAT_TYPES;j++){
			traceHeaders.push_back(latencyStrings[j]);
		}

		estimateTraces[i].initalizeTrace(traceHeaders);
		latencyTraces[i].initalizeTrace(traceHeaders);

		stringstream mtitle;
		mtitle << "CPU" << i << "MissTrace";
		aloneMissTrace[i] = RequestTrace(mtitle.str(), "");

		vector<string> mTraceHeaders;
		mTraceHeaders.push_back("Requests");
		mTraceHeaders.push_back("Alone (estimate or actual)");
		mTraceHeaders.push_back("Interference Misses");

		aloneMissTrace[i].initalizeTrace(mTraceHeaders);
	}

	cpuStallAccumulator.resize(_cpu_count, 0);
	cpuStalledAt.resize(_cpu_count, 0);
	cpuIsStalled.resize(_cpu_count, false);
}

void
InterferenceManager::regStats(){

	using namespace Stats;

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
		.desc("total round trip latency")
		.flags(total);

	requests
		.init(intManCPUCount)
		.name(name() + ".requests")
		.desc("total number of requests")
		.flags(total);

	avgRoundTripLatency
		.name(name() + ".avg_round_trip_latency")
		.desc("average total round trip latency")
		.flags(total);

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

	totalLatency
		.init(intManCPUCount)
		.name(name() + ".total_latency")
		.desc("total latency");

	totalInterference
		.init(intManCPUCount)
		.name(name() + ".total_interference")
		.desc("total estimated latency clock cycles");

	avgTotalLatency
		.name(name() + ".avg_total_latency")
		.desc("Average shared memory system latency");

	avgTotalLatency = totalLatency / requests;

	avgTotalInterference
		.name(name() + ".avg_total_interference")
		.desc("Average shared memory system interference latency");

	avgTotalInterference = totalInterference / requests;

	avgInterferencePercentage
		.name(name() + ".avg_total_interference_percentage")
		.desc("The percentage interference / round trip latency");

	avgInterferencePercentage = avgTotalInterference / avgTotalLatency;

	cpuStallCycles
		.init(intManCPUCount)
		.name(name() + ".cpu_stall_cycles")
		.desc("total number of clock cycles the CPU was stalled");

	numCpuStalls
		.init(intManCPUCount)
		.name(name() + ".num_of_cpu_stalls")
		.desc("total number of times the CPU was stalled");

	cpuStallPercentage
		.name(name() + ".cpu_stall_percentage")
		.desc("The percentage of time the CPU was stalled");

	cpuStallPercentage = cpuStallCycles / simTicks;

	avgCpuStallLength
		.name(name() + ".avg_cpu_stall_length")
		.desc("The average length of a CPU stall");

	avgCpuStallLength = cpuStallCycles / numCpuStalls;
}

void
InterferenceManager::registerBus(Bus* bus){
	memoryBuses.push_back(bus);
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

	totalInterference[req->adaptiveMHASenderID] += interferenceTicks;

	interferenceBreakdownAccumulator[req->adaptiveMHASenderID][t] += interferenceTicks;
	interferenceAccumulator[req->adaptiveMHASenderID] += interferenceTicks;

	instTraceInterferenceSum[req->adaptiveMHASenderID] += interferenceTicks;
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

	totalLatency[req->adaptiveMHASenderID] += latency;

	sharedLatencyBreakdownAccumulator[req->adaptiveMHASenderID][t] += latency;
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

	requestsSinceLastSample[req->adaptiveMHASenderID]++;

	currentRequests[req->adaptiveMHASenderID]++;
	sharedLatencyAccumulator[req->adaptiveMHASenderID] += roundTripLatency;

	instTraceLatencySum[req->adaptiveMHASenderID] += roundTripLatency;
	instTraceRequests[req->adaptiveMHASenderID]++;

	if(totalRequestCount[req->adaptiveMHASenderID] % sampleSize == 0 && traceStarted){
		vector<double> tmpLatencies = traceLatency(req->adaptiveMHASenderID);
		traceInterference(req->adaptiveMHASenderID, tmpLatencies);
		traceMisses(req->adaptiveMHASenderID);
	}

	if(resetInterval != -1 && totalRequestCount[req->adaptiveMHASenderID] % resetInterval == 0 && traceStarted){
		resetInterferenceMeasurements(req->adaptiveMHASenderID);

		for(int i=0;i<cacheInterferenceObjs.size(); i++){
			cacheInterferenceObjs[i]->computeInterferenceProbabilities(req->adaptiveMHASenderID);

			if(!cacheInterferenceObjs[i]->interferenceInsertionsInitiated(req->adaptiveMHASenderID)){
				cacheInterferenceObjs[i]->initiateInterferenceInsertions(req->adaptiveMHASenderID);
			}
		}
	}
}

void
InterferenceManager::traceMisses(int fromCPU){
	std::vector<RequestTraceEntry> data;
	data.resize(3, RequestTraceEntry(0));
	data[0] = RequestTraceEntry(requests[fromCPU].value());
	data[1] = RequestTraceEntry(regularMisses[fromCPU]);
	data[2] = RequestTraceEntry(interferenceMisses[fromCPU]);
	aloneMissTrace[fromCPU].addTrace(data);
}

void
InterferenceManager::addCacheResult(MemReqPtr& req){
	if(req->interferenceMissAt > 0){
		interferenceMisses[req->adaptiveMHASenderID]++;
	}
	else{
		regularMisses[req->adaptiveMHASenderID]++;
	}
}

vector<double>
InterferenceManager::traceInterference(int fromCPU, vector<double> avgLats){

	vector<double> privateLatencyEstimate;
	privateLatencyEstimate.resize(NUM_LAT_TYPES+1, 0.0);

	double tmpInterferenceSum = 0;

	for(int i=0;i<NUM_LAT_TYPES;i++) tmpInterferenceSum += (double) interferenceSum[fromCPU][i];

	double avgInterference = tmpInterferenceSum / totalRequestCount[fromCPU];

	std::vector<RequestTraceEntry> data;
	data.resize(NUM_LAT_TYPES+2, RequestTraceEntry(0.0));

	data[0] = requests[fromCPU].value();
	data[1] = avgLats[0] - avgInterference;

	privateLatencyEstimate[0] = avgLats[0] - avgInterference;

	for(int i=0;i<NUM_LAT_TYPES;i++){
		double tmpAvgInt = (double) ((double) interferenceSum[fromCPU][i] / (double) totalRequestCount[fromCPU] );
		data[i+2] = avgLats[i+1] - tmpAvgInt;
		privateLatencyEstimate[i+1] =  avgLats[i+1] - tmpAvgInt;
	}

	estimateTraces[fromCPU].addTrace(data);

	return privateLatencyEstimate;

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

	interferenceMisses[fromCPU] = 0;
	regularMisses[fromCPU] = 0;
}

PerformanceMeasurement
InterferenceManager::buildInterferenceMeasurement(int period){

	int np = fullCPUs.size();

	PerformanceMeasurement currentMeasurement(np, NUM_LAT_TYPES, maxMSHRs, period);

	for(int i=0;i<fullCPUs.size();i++){
		currentMeasurement.committedInstructions[i] = fullCPUs[i]->getCommittedInstructions();

		if(cpuIsStalled[i]){
			// make sure no stalls crosses sample boundaries
			clearStalledForMemory(i, false);
			setStalledForMemory(i, 0);
		}

		currentMeasurement.cpuStallCycles[i] = cpuStallAccumulator[i];
		cpuStallAccumulator[i] = 0;
	}

	for(int i=0;i<lastPrivateCaches.size();i++){
		currentMeasurement.mlpEstimate[i] = lastPrivateCaches[i]->getMLPEstimate();
		currentMeasurement.avgMissesWhileStalled[i] = lastPrivateCaches[i]->getServicedMissesWhileStalledEstimate();
		currentMeasurement.requestsInSample[i] = currentRequests[i];
		currentMeasurement.responsesWhileStalled[i] = lastPrivateCaches[i]->getResponsesWhileStalled();
	}

	currentMeasurement.addInterferenceData(sharedLatencyAccumulator,
										   interferenceAccumulator,
										   sharedLatencyBreakdownAccumulator,
										   interferenceBreakdownAccumulator,
										   currentRequests);

	double utilSum = 0.0;
	for(int i=0;i<memoryBuses.size();i++){
		utilSum += memoryBuses[i]->getActualUtilization();
	}
	currentMeasurement.actualBusUtilization = utilSum / (double) memoryBuses.size();

	double totalMisses = 0.0;
	double totalAccesses = 0.0;
	for(int i=0;i<sharedCaches.size();i++){
		RateMeasurement rm = sharedCaches[i]->getMissRate();
		totalMisses += rm.nominator;
		totalAccesses += rm.denominator;
	}
	currentMeasurement.sharedCacheMissRate = totalMisses / totalAccesses;

	for(int i=0;i<cpuCount;i++){

		currentRequests[i] = 0;
		sharedLatencyAccumulator[i] = 0;
		interferenceAccumulator[i] = 0;

		for(int j=0;j<NUM_LAT_TYPES;j++){
			sharedLatencyBreakdownAccumulator[i][j] = 0;
			interferenceBreakdownAccumulator[i][j] = 0;
		}
	}

	return currentMeasurement;
}

void
InterferenceManager::registerSharedCache(BaseCache* cache){
	sharedCaches.push_back(cache);
}

void
InterferenceManager::registerCacheInterferenceObj(CacheInterference* ci){
	cacheInterferenceObjs.push_back(ci);
}

void
InterferenceManager::registerMissBandwidthPolicy(MissBandwidthPolicy* policy){
	assert(missBandwidthPolicy == NULL);
	missBandwidthPolicy = policy;
}

void
InterferenceManager::registerLastLevelPrivateCache(BaseCache* cache, int cpuID, int cacheMaxMSHRs){
	assert(lastPrivateCaches[cpuID] == NULL);
	lastPrivateCaches[cpuID] = cache;
	maxMSHRs = cacheMaxMSHRs;
}

void
InterferenceManager::registerCPU(FullCPU* cpu, int cpuID){
	assert(fullCPUs[cpuID] == NULL);
	fullCPUs[cpuID] = cpu;
}

bool
InterferenceManager::isStalledForMemory(int cpuID){
	return cpuIsStalled[cpuID];
}

void
InterferenceManager::setStalledForMemory(int cpuID, int detectionDelay){
	assert(!cpuIsStalled[cpuID]);

	cpuStalledAt[cpuID] = curTick - detectionDelay;
	cpuIsStalled[cpuID] = true;
}

void
InterferenceManager::clearStalledForMemory(int cpuID, bool incrementNumStalls){
	assert(cpuIsStalled[cpuID]);

	Tick cpuStalledFor = curTick - cpuStalledAt[cpuID];

	cpuStallAccumulator[cpuID] += cpuStalledFor;
	cpuStallCycles[cpuID] += cpuStalledFor;
	if(incrementNumStalls) numCpuStalls[cpuID]++;

	cpuStalledAt[cpuID] = 0;
	cpuIsStalled[cpuID] = false;
}

void
InterferenceManager::doCommitTrace(int cpuID, int committedInstructions, int stallCycles, Tick ticksInSample){

	double mws = lastPrivateCaches[cpuID]->getInstTraceMWS();

	double mlp = lastPrivateCaches[cpuID]->getInstTraceMLP();

	int responsesWhileStalled  = lastPrivateCaches[cpuID]->getInstTraceRespWhileStalled();

	// get alone latency prediction
	double avgSharedLatency = 0;
	double avgInterferenceLatency = 0;
	if(instTraceRequests[cpuID] > 0 ){
		avgSharedLatency = (double) instTraceLatencySum[cpuID] / (double) instTraceRequests[cpuID];
		avgInterferenceLatency = (double) instTraceInterferenceSum[cpuID] / (double) instTraceRequests[cpuID];
	}
	double predictedAloneLat = avgSharedLatency - avgInterferenceLatency;



	missBandwidthPolicy->doCommittedInstructionTrace(cpuID,
			                                         avgSharedLatency,
			                                         predictedAloneLat,
			                                         mws,
			                                         mlp,
			                                         instTraceRequests[cpuID],
			                                         stallCycles,
			                                         ticksInSample,
			                                         committedInstructions,
			                                         responsesWhileStalled);

	instTraceInterferenceSum[cpuID] = 0;
	instTraceRequests[cpuID] = 0;
	instTraceLatencySum[cpuID] = 0;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(InterferenceManager)
    Param<int> cpu_count;
    Param<int> sample_size;
    Param<int> reset_interval;
END_DECLARE_SIM_OBJECT_PARAMS(InterferenceManager)

BEGIN_INIT_SIM_OBJECT_PARAMS(InterferenceManager)
	INIT_PARAM_DFLT(cpu_count, "Number of CPUs", -1),
	INIT_PARAM_DFLT(sample_size, "Number of requests", 1024),
	INIT_PARAM_DFLT(reset_interval, "Number of requests after which the measurements are reset", 1024)
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
