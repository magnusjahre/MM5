/*
 * memory_overlap_estimator.cc
 *
 *  Created on: Nov 18, 2011
 *      Author: jahre
 */

#include "memory_overlap_estimator.hh"
#include "sim/builder.hh"
#include "base/trace.hh"

using namespace std;

#define CACHE_BLK_SIZE 64

MemoryOverlapEstimator::MemoryOverlapEstimator(string name, int id, InterferenceManager* _interferenceManager)
: SimObject(name){
	isStalled = false;
	stalledAt = 0;
	resumedAt = 0;

	cpuID = id;
	interferenceManager = _interferenceManager;

	for(int i=0;i<NUM_STALL_CAUSES;i++) stallCycles[i] = 0;
	lastTraceAt = 1;
	commitCycles = 0;

	initOverlapTrace();
	initStallTrace();
	initRequestGroupTrace();

	lastActivityCycle = 0;
}

void
MemoryOverlapEstimator::cpuStarted(Tick firstTick){
	lastActivityCycle = firstTick-1;
	stallCycles[STALL_OTHER] += firstTick-1;
}

void
MemoryOverlapEstimator::addStall(StallCause cause, Tick cycles, bool memStall){

	assert(!isStalled);

	if(memStall){
		//memory stalls are detected one cycle late
		assert(lastActivityCycle == curTick-cycles-1);
		lastActivityCycle = curTick-1;
	}
	else{
		assert(lastActivityCycle == curTick-1);
		lastActivityCycle = curTick;

		assert(interferenceManager != NULL);
		interferenceManager->addCommitCycle(cpuID);
	}

	stallCycles[cause] += cycles;
}

void
MemoryOverlapEstimator::addCommitCycle(){

	assert(!isStalled);
	assert(lastActivityCycle == curTick-1);
	lastActivityCycle = curTick;
	commitCycles++;

	assert(interferenceManager != NULL);
	interferenceManager->addCommitCycle(cpuID);
}

void
MemoryOverlapEstimator::initOverlapTrace(){
	overlapTrace = RequestTrace(name(), "OverlapTrace", 1);

	vector<string> headers;
	headers.push_back("Committed instructions");
	headers.push_back("Total stall cycles");
	headers.push_back("Shared stall cycles");
	headers.push_back("Total Requests");
	headers.push_back("Shared Loads");
	headers.push_back("Average Shared Load Latency");

	overlapTrace.initalizeTrace(headers);

	stallCycleAccumulator = 0;
	sharedStallCycleAccumulator = 0;
	totalRequestAccumulator = 0;
	sharedRequestAccumulator = 0;
	sharedLatencyAccumulator = 0;
}

void
MemoryOverlapEstimator::traceOverlap(int committedInstructions){
	vector<RequestTraceEntry> data;

	data.push_back(committedInstructions);
	data.push_back(stallCycleAccumulator);
	data.push_back(sharedStallCycleAccumulator);
	data.push_back(totalRequestAccumulator);
	data.push_back(sharedRequestAccumulator);

	if(sharedRequestAccumulator > 0){
		data.push_back((double) sharedLatencyAccumulator / (double) sharedRequestAccumulator);
	}
	else{
		data.push_back(0);
	}

	stallCycleAccumulator = 0;
	sharedStallCycleAccumulator = 0;
	totalRequestAccumulator = 0;
	sharedRequestAccumulator = 0;
	sharedLatencyAccumulator = 0;

	overlapTrace.addTrace(data);
}

void
MemoryOverlapEstimator::initStallTrace(){
	stallTrace = RequestTrace(name(), "StallTrace", 1);

	vector<string> headers;
	headers.push_back("Committed instructions");
	headers.push_back("Ticks in Sample");
	headers.push_back("Commit cycles");
	headers.push_back("Storebuffer Stalls");
	headers.push_back("Functional Unit Stalls");
	headers.push_back("Memory Related Stalls");
	headers.push_back("Other Stalls");

	stallTrace.initalizeTrace(headers);
}

void
MemoryOverlapEstimator::traceStalls(int committedInstructions){
	vector<RequestTraceEntry> data;

	Tick cyclesSinceLast = curTick - lastTraceAt;
	lastTraceAt = curTick;

	data.push_back(committedInstructions);
	data.push_back(cyclesSinceLast);
	data.push_back(commitCycles);
	data.push_back(stallCycles[STALL_STORE_BUFFER]);
	data.push_back(stallCycles[STALL_FUNC_UNIT]);
	data.push_back(stallCycles[STALL_DMEM]);
	data.push_back(stallCycles[STALL_OTHER]);

	stallTrace.addTrace(data);

	Tick sum = commitCycles;
	for(int i=0;i<NUM_STALL_CAUSES;i++){
		sum += stallCycles[i];
	}
	assert(sum == cyclesSinceLast);

	for(int i=0;i<NUM_STALL_CAUSES;i++) stallCycles[i] = 0;
	commitCycles = 0;
}

void
MemoryOverlapEstimator::initRequestGroupTrace(){
	requestGroupTrace = RequestTrace(name(), "RequestGroupTrace", 1);

	vector<string> headers;
	headers.push_back("Committed instructions");
	headers.push_back("Shared Cache Hits");
	headers.push_back("Shared Cache Misses");
	headers.push_back("Avg Private Requests");
	headers.push_back("Avg Shared Latency");
	headers.push_back("Frequency");

	requestGroupTrace.initalizeTrace(headers);
}

void
MemoryOverlapEstimator::sampleCPU(int committedInstructions){
	traceOverlap(committedInstructions);
	traceStalls(committedInstructions);
	traceRequestGroups(committedInstructions);
}

void
MemoryOverlapEstimator::regStats(){

	privateStallCycles
		.name(name() + ".private_stall_cycles")
		.desc("number of memory stall cycles due to the private memory system");

	sharedStallCycles
		.name(name() + ".shared_stall_cycles")
		.desc("number of memory stall cycles due to the shared memory system");

	sharedRequestCount
		.name(name() + ".shared_requests")
		.desc("number of requests to the shared memory system");

	sharedLoadCount
		.name(name() + ".shared_loads")
		.desc("number of shared requests that are loads");

	totalLoadLatency
		.name(name() + ".total_load_latency")
		.desc("sum of the latency of all shared mem sys loads");

	burstAccumulator
		.name(name() + ".shared_burst_accumulator")
		.desc("Accumulated burst sizes (shared reqs only)");

	numSharedStalls
		.name(name() + ".num_shared_stalls")
		.desc("Number of stalls due to shared events");

	avgBurstSize
		.name(name() + ".avg_shared_burst_size")
		.desc("Average size of a shared memory system burst");

	avgBurstSize = burstAccumulator / numSharedStalls;

	totalStalls
		.name(name() + ".num_stalls")
		.desc("Number of memory related stalls");
}

void
MemoryOverlapEstimator::issuedMemoryRequest(MemReqPtr& req){
	DPRINTF(OverlapEstimator, "Issuing memory request for addr %d, command %s\n",req->paddr, req->cmd);
	pendingRequests.push_back(EstimationEntry(req->paddr & ~(CACHE_BLK_SIZE-1),curTick, req->cmd));
}

void
MemoryOverlapEstimator::completedMemoryRequest(MemReqPtr& req, Tick finishedAt, bool hiddenLoad){

	int useIndex = -1;
	for(int i=0;i<pendingRequests.size();i++){
		if((req->paddr & ~(CACHE_BLK_SIZE-1)) == pendingRequests[i].address){
			assert(useIndex == -1);
			useIndex = i;
		}
	}
	assert(useIndex != -1);

	pendingRequests[useIndex].completedAt = finishedAt;
	pendingRequests[useIndex].isSharedReq = req->beenInSharedMemSys;
	pendingRequests[useIndex].isSharedCacheMiss = req->isSharedCacheMiss;

	totalRequestAccumulator++;

	if(pendingRequests[useIndex].isSharedReq){
		sharedRequestCount++;
	}

	if(pendingRequests[useIndex].isStore() && !hiddenLoad){
		DPRINTF(OverlapEstimator, "Memory request for addr %d complete, command %s (original %s), latency %d, does not hide a load\n",
						req->paddr,
						req->cmd,
						pendingRequests[useIndex].origCmd,
						finishedAt - pendingRequests[useIndex].issuedAt);
	}
	else{
		DPRINTF(OverlapEstimator, "Memory request for addr %d complete, command %s (original %s), latency %d, adding to completed reqs\n",
					req->paddr,
					req->cmd,
					pendingRequests[useIndex].origCmd,
					finishedAt - pendingRequests[useIndex].issuedAt);

		if(pendingRequests[useIndex].isSharedReq){
			DPRINTF(OverlapEstimator, "Memory request for addr %d has been in the shared memory system\n", req->paddr);
			sharedLoadCount++;
			sharedRequestAccumulator++;
			totalLoadLatency += pendingRequests[useIndex].latency();
			sharedLatencyAccumulator += pendingRequests[useIndex].latency();
		}

		completedRequests.push_back(pendingRequests[useIndex]);
	}

	pendingRequests.erase(pendingRequests.begin()+	useIndex);
}

void
MemoryOverlapEstimator::stalledForMemory(){
	assert(!isStalled);
	isStalled = true;
	stalledAt = curTick;
	totalStalls++;

	DPRINTF(OverlapEstimator, "Stalling...\n");
}

void
MemoryOverlapEstimator::executionResumed(){
	assert(isStalled);
	isStalled = false;
	resumedAt = curTick;

	Tick stallLength = curTick - stalledAt;
	stallCycleAccumulator += stallLength;

	DPRINTF(OverlapEstimator, "Resuming execution, CPU was stalled for %d cycles\n", stallLength);

	while(!completedRequests.empty() && completedRequests.front().completedAt < stalledAt){
		DPRINTF(OverlapEstimator, "Skipping request for address %d\n", completedRequests.front().address);
		completedRequests.erase(completedRequests.begin());
	}

	int sharedCacheHits = 0;
	int sharedCacheMisses = 0;
	Tick sharedLatency = 0;
	int privateRequests = 0;
	while(!completedRequests.empty() && completedRequests.front().completedAt < curTick){
		DPRINTF(OverlapEstimator, "Request %d is part of burst, latency %d, %s\n",
				completedRequests.front().address,
				completedRequests.front().latency(),
				(completedRequests.front().isSharedReq ? "shared": "private") );

		if(completedRequests.front().isSharedReq){
			if(completedRequests.front().isSharedCacheMiss) sharedCacheMisses++;
			else sharedCacheHits++;
			sharedLatency += completedRequests.front().latency();
		}
		else{
			privateRequests++;
		}

		completedRequests.erase(completedRequests.begin());

	}

	if((sharedCacheHits+sharedCacheMisses) > 0){
		burstAccumulator += sharedCacheHits+sharedCacheMisses;
		numSharedStalls++;

		sharedStallCycles += stallLength;
		sharedStallCycleAccumulator += stallLength;

		updateRequestGroups(sharedCacheHits, sharedCacheMisses, privateRequests, sharedLatency);
	}
	else{
		privateStallCycles += stallLength;
	}

	addStall(STALL_DMEM, stallLength, true);

	assert(interferenceManager != NULL);
	interferenceManager->addStallCycles(cpuID, stallLength, true);

	DPRINTF(OverlapEstimator, "Current stall breakdown: store buffer %d, func unit %d, mem %d, other %d\n",
			stallCycles[STALL_STORE_BUFFER],
			stallCycles[STALL_FUNC_UNIT],
			stallCycles[STALL_DMEM],
			stallCycles[STALL_OTHER]);
}

void
MemoryOverlapEstimator::incrementPrivateRequestCount(MemReqPtr& req){
	interferenceManager->incrementPrivateRequestCount(req);
}

void
MemoryOverlapEstimator::addPrivateLatency(MemReqPtr& req, int latency){
	if(req->isStore) return;
	if(req->instructionMiss) return;
	interferenceManager->addPrivateLatency(InterferenceManager::CacheCapacity, req, latency);
}

void
MemoryOverlapEstimator::addL1Access(MemReqPtr& req, int latency, bool hit){
	if(req->isStore) return;
	if(hit){
		assert(req->adaptiveMHASenderID != -1);
		interferenceManager->addL1Hit(req->adaptiveMHASenderID, latency);
	}
	else{
		interferenceManager->addPrivateLatency(InterferenceManager::CacheCapacity, req, latency);
	}
}

void
MemoryOverlapEstimator::registerL1DataCache(int cpuID, BaseCache* cache){
	interferenceManager->registerL1DataCache(cpuID, cache);
}

void
MemoryOverlapEstimator::updateRequestGroups(int sharedHits, int sharedMisses, int pa, Tick sl){

	double avgSLat = (double) sl / (double) (sharedHits + sharedMisses);

	for(int i=0;i<groupSignatures.size();i++){
		if(groupSignatures[i].match(sharedHits, sharedMisses)){
			groupSignatures[i].add(pa, avgSLat);
			return;
		}
	}

	RequestGroupSignature rgs = RequestGroupSignature(sharedHits, sharedMisses);
	rgs.add(pa, avgSLat);
	groupSignatures.push_back(rgs);
}

void
MemoryOverlapEstimator::traceRequestGroups(int committedInstructions){
	for(int i=0;i<groupSignatures.size();i++){
		vector<RequestTraceEntry> data;
		data.push_back(committedInstructions);
		groupSignatures[i].populate(&data);
		requestGroupTrace.addTrace(data);
	}
	groupSignatures.clear();
}

MemoryOverlapEstimator::RequestGroupSignature::RequestGroupSignature(int _sharedCacheHits, int _sharedCacheMisses){
	sharedCacheHits = _sharedCacheHits;
	sharedCacheMisses = _sharedCacheMisses;

	avgPrivateAccesses = 0.0;
	avgSharedLatency = 0.0;
	entries = 0;
}

bool
MemoryOverlapEstimator::RequestGroupSignature::match(int _sharedCacheHits, int _sharedCacheMisses){
	if(sharedCacheHits == _sharedCacheHits && sharedCacheMisses == _sharedCacheMisses){
		return true;
	}
	return false;
}

void
MemoryOverlapEstimator::RequestGroupSignature::add(double pa, double avgSharedLat){

	double curPrivReqTotal = avgPrivateAccesses * entries;
	double curSharedTotal = avgSharedLatency * entries;

	entries++;
	if(curPrivReqTotal > 0 || pa > 0) avgPrivateAccesses = (curPrivReqTotal + pa) / (double) entries;
	else avgPrivateAccesses = 0;
	if(curSharedTotal > 0 || avgSharedLat > 0) avgSharedLatency = (curSharedTotal + avgSharedLat) / (double) entries;
	else avgSharedLatency = 0;
}

void
MemoryOverlapEstimator::RequestGroupSignature::populate(std::vector<RequestTraceEntry>* data){
	data->push_back(sharedCacheHits);
	data->push_back(sharedCacheMisses);
	data->push_back(avgPrivateAccesses);
	data->push_back(avgSharedLatency);
	data->push_back(entries);
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
    Param<int> id;
	SimObjectParam<InterferenceManager *> interference_manager;
END_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

BEGIN_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
	INIT_PARAM(id, "ID of this estimator"),
	INIT_PARAM_DFLT(interference_manager, "Interference manager", NULL)
END_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

CREATE_SIM_OBJECT(MemoryOverlapEstimator)
{
    return new MemoryOverlapEstimator(getInstanceName(), id, interference_manager);
}

REGISTER_SIM_OBJECT("MemoryOverlapEstimator", MemoryOverlapEstimator)
#endif

