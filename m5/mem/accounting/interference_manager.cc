
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
	(char*) "cache_capacity_request",
	(char*) "cache_capacity_response",
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

	includeStores = false;

	fullCPUs.resize(_cpu_count, NULL);
	lastPrivateCaches.resize(_cpu_count, NULL);
	l1DataCaches.resize(_cpu_count, NULL);
	requestsSinceLastSample.resize(_cpu_count, 0);
	maxMSHRs = 0;

	sharedLatencyAccumulator.resize(_cpu_count, 0);
	interferenceAccumulator.resize(_cpu_count, 0);
	sharedLatencyBreakdownAccumulator.resize(_cpu_count, vector<double>(NUM_LAT_TYPES, 0));
	interferenceBreakdownAccumulator.resize(_cpu_count, vector<double>(NUM_LAT_TYPES, 0));
	commitTraceSharedLatencyBreakdownAccumulator.resize(_cpu_count, vector<double>(NUM_LAT_TYPES, 0));
	currentRequests.resize(_cpu_count, 0);
	commitTraceBreakdownRequests.resize(_cpu_count, 0);

	privateLatencyAccumulator.resize(_cpu_count, 0);
	privateLatencyBreakdownAccumulator.resize(_cpu_count, vector<double>(NUM_LAT_TYPES, 0));
	privateRequests.resize(_cpu_count, 0);
	l1HitAccumulator.resize(_cpu_count, 0);
	l1HitRequests.resize(_cpu_count, 0);
	l1BlockedAccumulator.resize(_cpu_count, 0);

	latencySum.resize(_cpu_count, vector<Tick>(NUM_LAT_TYPES, 0));
	numLatencyReqs.resize(_cpu_count, vector<int>(NUM_LAT_TYPES, 0));

	interferenceSum.resize(_cpu_count, vector<Tick>(NUM_LAT_TYPES, 0));
	numInterferenceReqs.resize(_cpu_count, vector<int>(NUM_LAT_TYPES, 0));

	regularMisses.resize(_cpu_count, 0);
	interferenceMisses.resize(_cpu_count, 0);

	totalRequestCount.resize(_cpu_count, 0);
	runningLatencySum.resize(_cpu_count, 0);

	overlapEstimators.resize(_cpu_count, NULL);

	instTraceInterferenceSum.resize(_cpu_count, 0);
	instTraceLatencySum.resize(_cpu_count, 0);
	instTraceRequests.resize(_cpu_count, 0);
	instTraceHiddenLoads.resize(_cpu_count, 0);

	instTraceStoreInterferenceSum.resize(_cpu_count, 0);
	instTraceStoreLatencySum.resize(_cpu_count, 0);
	instTraceStoreRequests.resize(_cpu_count, 0);

	missBandwidthPolicy = NULL;
	cacheInterference = NULL;

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
	privateLatencyTraces.resize(_cpu_count, RequestTrace());

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

		stringstream mtitle;
		mtitle << "CPU" << i << "MissTrace";
		aloneMissTrace[i] = RequestTrace(mtitle.str(), "");

		vector<string> mTraceHeaders;
		mTraceHeaders.push_back("Requests");
		mTraceHeaders.push_back("Alone (estimate or actual)");
		mTraceHeaders.push_back("Interference Misses");

		aloneMissTrace[i].initalizeTrace(mTraceHeaders);

        stringstream ptitle;
        ptitle << "CPU" << i << "PrivateLatencyTrace";
		privateLatencyTraces[i] = RequestTrace(ptitle.str(),"");

		vector<string> privateLatencyHeaders;
		privateLatencyHeaders.push_back("Committed instructions");
		privateLatencyHeaders.push_back("L1 hits");
		privateLatencyHeaders.push_back("L1 misses");
		privateLatencyHeaders.push_back("Entry cycles");
		privateLatencyHeaders.push_back("L1 access cycles");
		privateLatencyHeaders.push_back("Interconnect entry");
		privateLatencyHeaders.push_back("Interconnect queue");
		privateLatencyHeaders.push_back("Interconnect transfer");
		privateLatencyHeaders.push_back("L2 access cycles");
		privateLatencyTraces[i].initalizeTrace(privateLatencyHeaders);
	}

	cpuSharedStallAccumulator.resize(_cpu_count, 0);
	cpuComTraceStallCycles.resize(_cpu_count, 0);
//	cpuStalledAt.resize(_cpu_count, 0);
//	cpuIsStalled.resize(_cpu_count, false);

	commitTraceCommitCycles.resize(_cpu_count, 0);
	commitTraceMemIndStall.resize(_cpu_count, 0);
	commitTracePrivateStall.resize(_cpu_count, 0);
	commitTraceWriteStall.resize(_cpu_count, 0);
	commitTracePrivateBlockedStall.resize(_cpu_count, 0);
	commitTraceEmptyROBStall.resize(_cpu_count, 0);

	cpuComTraceTotalRoundtrip.resize(_cpu_count, 0);
    cpuComTraceTotalRoundtripRequests.resize(_cpu_count, 0);

    performanceModels.resize(_cpu_count, NULL);
    for(int i=0;i<_cpu_count;i++){
    	performanceModels[i] = new PerformanceModel(i);
    }
}

void
InterferenceManager::addCommitCycle(int cpuID){
	commitTraceCommitCycles[cpuID]++;
}

void
InterferenceManager::addMemIndependentStallCycle(int cpuID){
	commitTraceMemIndStall[cpuID]++;
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

	noBusLatency
		.name(name() + ".no_bus_latency")
		.desc("Total bus independent latency");

	noBusLatency = latencies[InterconnectEntry] +
			       latencies[InterconnectRequestQueue] +
			       latencies[InterconnectRequestTransfer] +
			       latencies[InterconnectResponseQueue] +
			       latencies[InterconnectResponseTransfer] +
			       latencies[InterconnectDelivery] +
			       latencies[CacheCapacityRequest] +
				   latencies[CacheCapacityResponse];

	busLatency
		.name(name() + ".bus_latency")
		.desc("Total bus dependent latency");

	busLatency = latencies[MemoryBusEntry] +
				 latencies[MemoryBusQueue] +
				 latencies[MemoryBusService];

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

	cpuComputeCycles
		.name(name() + ".cpu_compute_cycles")
		.desc("total number of clock cycles the CPU used for computation");

	cpuComputeCycles = simTicks - cpuStallCycles;

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


	totalPrivateMemsysLatency
		.init(intManCPUCount)
		.name(name() + ".total_private_memsys_latency")
		.desc("total latency through the private memory system");

	privateMemsysRequests
		.init(intManCPUCount)
		.name(name() + ".total_private_memsys_requests")
		.desc("number of private memory system requests");

	avgPrivateMemsysLatency
		.name(name() + ".avg_private_memsys_latency")
		.desc("average latency through the private memory system");

	avgPrivateMemsysLatency = totalPrivateMemsysLatency / privateMemsysRequests;

	totalL1HitLatency
	.init(intManCPUCount)
	.name(name() + ".total_l1_hit_latency")
	.desc("total latency for L1 hits");

	totalL1HitRequests
	.init(intManCPUCount)
	.name(name() + ".total_l1_hit_requests")
	.desc("number of L1 hit requests");

	avgL1HitLatency
	.name(name() + ".avg_l1_hit_latency")
	.desc("average l1 hit latency");

	avgL1HitLatency = totalL1HitLatency / totalL1HitRequests;

	totalMemsysEntryLatency
		.init(intManCPUCount)
		.name(name() + ".total_memsys_entry_latency")
		.desc("number of cycles where a blocked L1 cache delays a load");
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

    if(req->instructionMiss) return;

    assert(req->cmd == Read);
    assert(req->adaptiveMHASenderID != -1);

    if(req->isStore){
        instTraceStoreInterferenceSum[req->adaptiveMHASenderID] += interferenceTicks;
    }

    if(checkForStore(req)) return;

    DPRINTF(OverlapEstimatorBois, "Bois estimate: Adding %d interference ticks for address %d of type %s\n",
            interferenceTicks,
            req->paddr,
            latencyStrings[t]);

    req->boisInterferenceSum += interferenceTicks;

    interferenceSum[req->adaptiveMHASenderID][t] += interferenceTicks;
    interference[t][req->adaptiveMHASenderID] += interferenceTicks;

    totalInterference[req->adaptiveMHASenderID] += interferenceTicks;

    interferenceBreakdownAccumulator[req->adaptiveMHASenderID][t] += interferenceTicks;
    interferenceAccumulator[req->adaptiveMHASenderID] += interferenceTicks;

    instTraceInterferenceSum[req->adaptiveMHASenderID] += interferenceTicks;
}

void
InterferenceManager::incrementInterferenceRequestCount(LatencyType t, MemReqPtr& req){

	if(checkForStore(req)) return;
	if(req->instructionMiss) return;

	assert(req->cmd == Read);
	assert(req->adaptiveMHASenderID != -1);

	numInterferenceReqs[req->adaptiveMHASenderID][t]++;
}


void
InterferenceManager::addLatency(LatencyType t, MemReqPtr& req, int latency){
	assert(req->cmd == Read);
	assert(req->adaptiveMHASenderID != -1);

	if(checkForStore(req)) return;
	if(req->instructionMiss) return;

	latencySum[req->adaptiveMHASenderID][t] += latency;
	latencies[t][req->adaptiveMHASenderID] += latency;

	totalLatency[req->adaptiveMHASenderID] += latency;

	sharedLatencyBreakdownAccumulator[req->adaptiveMHASenderID][t] += latency;
	commitTraceSharedLatencyBreakdownAccumulator[req->adaptiveMHASenderID][t] += latency;
}

void
InterferenceManager::incrementLatencyRequestCount(LatencyType t, MemReqPtr& req){
	assert(req->cmd == Read);
	assert(req->adaptiveMHASenderID != -1);

	if(checkForStore(req)) return;
	if(req->instructionMiss) return;

	numLatencyReqs[req->adaptiveMHASenderID][t]++;
}

void
InterferenceManager::incrementTotalReqCount(MemReqPtr& req, int roundTripLatency){
	assert(req->cmd == Read);
	assert(req->adaptiveMHASenderID != -1);

	if(req->instructionMiss) return;

	if(req->isStore){
		instTraceStoreLatencySum[req->adaptiveMHASenderID] += roundTripLatency;
		instTraceStoreRequests[req->adaptiveMHASenderID]++;
	}

	if(checkForStore(req)) return;

	runningLatencySum[req->adaptiveMHASenderID] += roundTripLatency;
	totalRequestCount[req->adaptiveMHASenderID]++;

	roundTripLatencies[req->adaptiveMHASenderID] += roundTripLatency;
	requests[req->adaptiveMHASenderID]++;

	requestsSinceLastSample[req->adaptiveMHASenderID]++;

	currentRequests[req->adaptiveMHASenderID]++;
	commitTraceBreakdownRequests[req->adaptiveMHASenderID]++;
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
	}
}

void
InterferenceManager::addSharedReqTotalRoundtrip(MemReqPtr& req, Tick latency){
	if(checkForStore(req)) return;

	assert(req->cmd == Read);
	assert(req->adaptiveMHASenderID != -1);
	assert(!req->instructionMiss);

	cpuComTraceTotalRoundtrip[req->adaptiveMHASenderID] += latency;
	cpuComTraceTotalRoundtripRequests[req->adaptiveMHASenderID]++;
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
		if(!fullCPUs[i]->getCommitTraceEnabled()) fullCPUs[i]->updatePrivPerfEst(false);
		currentMeasurement.committedInstructions[i] = fullCPUs[i]->getCommittedInstructions();
		currentMeasurement.cpuSharedStallCycles[i] = cpuSharedStallAccumulator[i];
		cpuSharedStallAccumulator[i] = 0;
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
	double totalBusReqs = 0.0;
	double totalAvgBusServiceLat = 0.0;
	double otherRequests = 0;
	double totalQueueCycles = 0.0;

	for(int i=0;i<memoryBuses.size();i++){
		vector<double> utilvals = memoryBuses[i]->getActualUtilization();
		totalAvgBusServiceLat += utilvals[0];
		totalBusReqs += utilvals[1];
		utilSum += utilvals[2];
		totalQueueCycles += utilvals[3];

		vector<int> tmpBusAccesses = memoryBuses[i]->getPerCoreBusAccesses();
		for(int j=0;j<tmpBusAccesses.size();j++){
			currentMeasurement.busAccessesPerCore[j] += tmpBusAccesses[j];
		}
		vector<int> tmpBusReads = memoryBuses[i]->getPerCoreBusReads();
		for(int j=0;j<tmpBusReads.size();j++){
			currentMeasurement.busReadsPerCore[j] += tmpBusReads[j];
		}
		otherRequests += memoryBuses[i]->getOtherRequests();
	}

	currentMeasurement.avgBusServiceCycles = totalAvgBusServiceLat / (double) memoryBuses.size();
	currentMeasurement.actualBusUtilization = utilSum / (double) memoryBuses.size();
	currentMeasurement.otherBusRequests = (int) (otherRequests / (double) memoryBuses.size());
	currentMeasurement.sumBusQueueCycles = totalQueueCycles; //FIXME: this does not handle multiple channels

	double totalMisses = 0.0;
	double totalAccesses = 0.0;
	for(int i=0;i<sharedCaches.size();i++){
		RateMeasurement rm = sharedCaches[i]->getMissRate();
		totalMisses += rm.nominator;
		totalAccesses += rm.denominator;
	}
	currentMeasurement.sharedCacheMissRate = totalMisses / totalAccesses;

	assert(cacheInterference != NULL);
	vector<CacheMissMeasurements> currentCacheMeasurements = cacheInterference->getMissMeasurementSample();
	for(int j=0;j<cpuCount;j++) currentMeasurement.perCoreCacheMeasurements[j] = currentCacheMeasurements[j];

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
	assert(cacheInterference == NULL);
	cacheInterference = ci;
}

void
InterferenceManager::registerMissBandwidthPolicy(BasePolicy* policy){
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
InterferenceManager::registerL1DataCache(int cpuID, BaseCache* cache){
	assert(l1DataCaches[cpuID] == NULL);
	l1DataCaches[cpuID] = cache;
}

void
InterferenceManager::registerCPU(FullCPU* cpu, int cpuID){
	assert(fullCPUs[cpuID] == NULL);
	fullCPUs[cpuID] = cpu;
}

bool
InterferenceManager::checkForStore(MemReqPtr& req){
	if(includeStores) return false;
	return req->isStore;
}

void
InterferenceManager::addStallCycles(int cpuID, Tick cpuStalledFor, bool isShared, bool incrementNumStalls, Tick writeStall, Tick blockedStall, Tick emptyROBStall){
	if(isShared){
		cpuSharedStallAccumulator[cpuID] += cpuStalledFor;
		cpuComTraceStallCycles[cpuID] += cpuStalledFor;
		cpuStallCycles[cpuID] += cpuStalledFor;
		if(incrementNumStalls) numCpuStalls[cpuID]++;
	}
	else{
		commitTracePrivateStall[cpuID] += cpuStalledFor;
	}

	commitTraceWriteStall[cpuID] += writeStall;
	commitTracePrivateBlockedStall[cpuID] += blockedStall;
	commitTraceEmptyROBStall[cpuID] += emptyROBStall;
}

void
InterferenceManager::addPrivateLatency(LatencyType t, MemReqPtr& req, int latency){
	if(checkForStore(req)) return;
	if(req->instructionMiss) return;
	assert(req->adaptiveMHASenderID != -1);

	privateLatencyAccumulator[req->adaptiveMHASenderID] += latency;
	privateLatencyBreakdownAccumulator[req->adaptiveMHASenderID][t] += latency;

	totalPrivateMemsysLatency[req->adaptiveMHASenderID] += latency;
}

void
InterferenceManager::incrementPrivateRequestCount(MemReqPtr& req){
	if(checkForStore(req)) return;

	assert(!req->instructionMiss);
	assert(req->adaptiveMHASenderID != -1);

	privateRequests[req->adaptiveMHASenderID]++;
	privateMemsysRequests[req->adaptiveMHASenderID]++;
}

void
InterferenceManager::addL1Hit(int cpuID, Tick latency){
	l1HitAccumulator[cpuID] += latency;
	l1HitRequests[cpuID]++;

	totalL1HitLatency[cpuID] += latency;
	totalL1HitRequests[cpuID]++;
}

void
InterferenceManager::addL1BlockedCycle(int cpuID){
	l1BlockedAccumulator[cpuID]++;
	totalMemsysEntryLatency[cpuID]++;
}

void
InterferenceManager::tracePrivateLatency(int fromCPU, int committedInstructions){

	vector<RequestTraceEntry> data;

	data.push_back(committedInstructions);
	data.push_back(l1HitRequests[fromCPU]);
	data.push_back(privateRequests[fromCPU]);
	data.push_back(l1BlockedAccumulator[fromCPU]);
	data.push_back((double) l1HitAccumulator[fromCPU] / (double) l1HitRequests[fromCPU]);
	data.push_back((double) privateLatencyBreakdownAccumulator[fromCPU][InterconnectEntry] / (double)privateRequests[fromCPU]);
	data.push_back((double) (privateLatencyBreakdownAccumulator[fromCPU][InterconnectRequestQueue] + privateLatencyBreakdownAccumulator[fromCPU][InterconnectResponseQueue]) / (double) privateRequests[fromCPU]);
	data.push_back((double) (privateLatencyBreakdownAccumulator[fromCPU][InterconnectRequestTransfer] + privateLatencyBreakdownAccumulator[fromCPU][InterconnectResponseTransfer]) / privateRequests[fromCPU]);
	data.push_back((double) privateLatencyBreakdownAccumulator[fromCPU][CacheCapacityRequest] / (double) privateRequests[fromCPU]);

	privateLatencyTraces[fromCPU].addTrace(data);

	for(int i=0;i<NUM_LAT_TYPES;i++) privateLatencyBreakdownAccumulator[fromCPU][i] = 0;
}

double
InterferenceManager::getAvgNoBusLat(double avgRoundTripLatency, int cpuID){

	double busLatSum = commitTraceSharedLatencyBreakdownAccumulator[cpuID][MemoryBusEntry];
	busLatSum += commitTraceSharedLatencyBreakdownAccumulator[cpuID][MemoryBusQueue];
	busLatSum += commitTraceSharedLatencyBreakdownAccumulator[cpuID][MemoryBusService];

	double avgBusLat = 0.0;
	if(commitTraceBreakdownRequests[cpuID]){
		avgBusLat = busLatSum / (double) commitTraceBreakdownRequests[cpuID];
	}

	commitTraceBreakdownRequests[cpuID] = 0;
	for(int j=0;j<NUM_LAT_TYPES;j++){
		commitTraceSharedLatencyBreakdownAccumulator[cpuID][j] = 0;
	}

	// A pending requests add latency to the accumulator as it experiences the latency.
	// Thus, the total queue cycles may in some rare cases exceed the roundtrip latency.
	if(avgRoundTripLatency < avgBusLat) return 0;
	return avgRoundTripLatency - avgBusLat;
}

PerformanceModelMeasurements
InterferenceManager::buildModelMeasurements(int cpuID, int committedInstructions, Tick ticksInSample, OverlapStatistics ols){
	PerformanceModelMeasurements modelMeasurements = PerformanceModelMeasurements();
	modelMeasurements.committedInstructions = committedInstructions;
	modelMeasurements.ticksInSample = ticksInSample;
	modelMeasurements.cpl = ols.graphCPL;

	DPRINTF(PerformanceModelMeasurements, "Initializing performance model measurements with instructions %d total ticks %d\n",
			modelMeasurements.committedInstructions,
			modelMeasurements.ticksInSample);

	if(memoryBuses.size() != 1) warn("CPU %d: Multichannel memory bus not supported by performance model measurements @ %d", cpuID, curTick);
	modelMeasurements = memoryBuses[0]->updateModelMeasurements(modelMeasurements);

	modelMeasurements.avgMemBusParallelism = ols.globalAvgMemBusPara;

	modelMeasurements.memBusParaHistorgram = ols.memBusParaHistogram;

	return modelMeasurements;
}

void
InterferenceManager::updatePrivPerfEst(int cpuID, int committedInstructions, Tick ticksInSample, OverlapStatistics ols, double cwp, int numWriteStalls, Tick boisAloneStallEst){

	tracePrivateLatency(cpuID, committedInstructions);

	// get alone latency prediction
	double avgSharedLatency = 0;
	double avgInterferenceLatency = 0;
	if(instTraceRequests[cpuID] > 0 ){
		avgSharedLatency = (double) instTraceLatencySum[cpuID] / (double) instTraceRequests[cpuID];
		avgInterferenceLatency = (double) instTraceInterferenceSum[cpuID] / (double) instTraceRequests[cpuID];
	}
	double predictedAloneLat = avgSharedLatency - avgInterferenceLatency;

	double avgSharedStoreLat = 0;
	double avgSharedStoreIntLat = 0;
	if(instTraceStoreRequests[cpuID] > 0){
		avgSharedStoreLat = (double) instTraceStoreLatencySum[cpuID] / (double) instTraceStoreRequests[cpuID];
		avgSharedStoreIntLat = (double) instTraceStoreInterferenceSum[cpuID] / (double) instTraceStoreRequests[cpuID];
	}
	double predictedStoreAloneLat = avgSharedStoreLat - avgSharedStoreIntLat;

	double avgTotalLat = 0.0;
	if(cpuComTraceTotalRoundtripRequests[cpuID] > 0){
		avgTotalLat = (double) cpuComTraceTotalRoundtrip[cpuID] / (double) cpuComTraceTotalRoundtripRequests[cpuID];
	}

	CacheAccessMeasurement privateLLCEstimates = cacheInterference->getPrivateHitEstimate(cpuID);
	CacheAccessMeasurement llcMeasurements;
	for(int i=0;i<sharedCaches.size();i++){
		llcMeasurements = sharedCaches[i]->updateCacheMissMeasurements(llcMeasurements, cpuID);
	}

	// Base policy trace
	if(missBandwidthPolicy != NULL){

		missBandwidthPolicy->updatePrivPerfEst(cpuID,
											   avgSharedLatency,
											   predictedAloneLat,
											   instTraceRequests[cpuID],
											   cpuComTraceStallCycles[cpuID],
											   ticksInSample,
											   committedInstructions,
											   commitTraceCommitCycles[cpuID],
											   commitTracePrivateStall[cpuID],
											   avgTotalLat - avgSharedLatency,
											   commitTraceWriteStall[cpuID],
											   instTraceHiddenLoads[cpuID],
											   commitTraceMemIndStall[cpuID],
											   ols,
											   cacheInterference->getPrivateCommitTraceMissRate(cpuID),
											   cwp,
											   commitTracePrivateBlockedStall[cpuID],
											   avgSharedStoreLat,
											   predictedStoreAloneLat,
											   instTraceStoreRequests[cpuID],
											   numWriteStalls,
											   commitTraceEmptyROBStall[cpuID],
											   boisAloneStallEst,
											   privateLLCEstimates,
											   llcMeasurements);

		PerformanceModelMeasurements modelMeasurements = buildModelMeasurements(cpuID, committedInstructions, ticksInSample, ols);
		missBandwidthPolicy->doPerformanceModelTrace(cpuID, modelMeasurements);

	}

	// Performance model
	performanceModels[cpuID]->reset();
	performanceModels[cpuID]->setHelperVariables(committedInstructions, ols.tableCPL, ticksInSample);
	performanceModels[cpuID]->updateCPIMemInd(commitTraceCommitCycles[cpuID], commitTraceMemIndStall[cpuID]);
	performanceModels[cpuID]->updateCPIOther(commitTraceWriteStall[cpuID],
			                                 commitTracePrivateBlockedStall[cpuID],
			                                 commitTraceEmptyROBStall[cpuID]);
	performanceModels[cpuID]->updateCPIPrivLoads(commitTracePrivateStall[cpuID]);
	performanceModels[cpuID]->updateCPIOverlap(cwp);
	double avgNoBusLat = getAvgNoBusLat(avgTotalLat, cpuID);
	performanceModels[cpuID]->updateCPINoBus(avgNoBusLat);
	performanceModels[cpuID]->updateMeasuredCPIBus(avgTotalLat - avgNoBusLat);

	performanceModels[cpuID]->traceModelParameters();

	// compute interference probabilities for next sample (needed by the random IPP)
	cacheInterference->computeInterferenceProbabilities(cpuID);

	instTraceInterferenceSum[cpuID] = 0;
	instTraceRequests[cpuID] = 0;
	instTraceHiddenLoads[cpuID] = 0;
	instTraceLatencySum[cpuID] = 0;
	cpuComTraceStallCycles[cpuID] = 0;

	instTraceStoreInterferenceSum[cpuID] = 0;
	instTraceStoreLatencySum[cpuID] = 0;
	instTraceStoreRequests[cpuID] = 0;

	commitTraceCommitCycles[cpuID] = 0;
	commitTraceMemIndStall[cpuID] = 0;
	commitTracePrivateStall[cpuID] = 0;
	commitTraceWriteStall[cpuID] = 0;
	commitTracePrivateBlockedStall[cpuID] = 0;
	commitTraceEmptyROBStall[cpuID] = 0;

	cpuComTraceTotalRoundtrip[cpuID] = 0;
	cpuComTraceTotalRoundtripRequests[cpuID] = 0;

	privateLatencyAccumulator[cpuID] = 0;
	privateRequests[cpuID] = 0;

	l1HitAccumulator[cpuID] = 0;
	l1HitRequests[cpuID] = 0;
	l1BlockedAccumulator[cpuID] = 0;
}

void
InterferenceManager::hiddenLoadDetected(int cpuID){
	assert(cpuID != -1);
	instTraceHiddenLoads[cpuID]++;
}

void
InterferenceManager::enableMSHROccupancyTrace(){
	for(int i=0;i<lastPrivateCaches.size();i++){
		lastPrivateCaches[i]->enableOccupancyList();
	}
}

std::vector<MSHROccupancy>*
InterferenceManager::getMSHROccupancyList(int cpuID){
	return lastPrivateCaches[cpuID]->getOccupancyList();
}

void
InterferenceManager::clearMSHROccupancyLists(){
	for(int i=0;i<lastPrivateCaches.size();i++){
		lastPrivateCaches[i]->clearOccupancyList();
	}
}

void
InterferenceManager::itcaIntertaskMiss(int cpuID, Addr addr, bool isInstructionMiss, Addr cpuAddr){
	assert(cpuID != -1);
	assert(overlapEstimators[cpuID] != NULL);
	overlapEstimators[cpuID]->itcaIntertaskMiss(addr, isInstructionMiss, cpuAddr);
}

void
InterferenceManager::registerMemoryOverlapEstimator(MemoryOverlapEstimator* moe, int cpuID){
	assert(overlapEstimators[cpuID] == NULL);
	overlapEstimators[cpuID] = moe;
}

void
InterferenceManager::busWritebackCompleted(MemReqPtr& req, Tick finishedAt){
	assert(cpuCount == 1);
	overlapEstimators[0]->busWritebackCompleted(req, finishedAt);
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
