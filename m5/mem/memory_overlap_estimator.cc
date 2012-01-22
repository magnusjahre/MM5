/*
 * memory_overlap_estimator.cc
 *
 *  Created on: Nov 18, 2011
 *      Author: jahre
 */

#include "memory_overlap_estimator.hh"
#include "sim/builder.hh"
#include "base/trace.hh"
#include <algorithm>

using namespace std;

#define CACHE_BLK_SIZE 64

MemoryOverlapEstimator::MemoryOverlapEstimator(string name, int id, InterferenceManager* _interferenceManager, int cpu_count, HierParams* params)
: BaseHier(name, params){
	isStalled = false;
	stalledAt = 0;
	resumedAt = 0;
	stalledOnAddr = 0;

	stallIdentifyAlg = SHARED_STALL_ROB; //FIXME: parameterize
	//stallIdentifyAlg = SHARED_STALL_EXISTS; //FIXME: parameterize

	cpuID = id;
	cpuCount = cpu_count;
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
	headers.push_back("Avg Hidden Shared Latency");
	headers.push_back("Computed shared stall");
	headers.push_back("Computed overlap");

	overlapTrace.initalizeTrace(headers);

	stallCycleAccumulator = 0;
	sharedStallCycleAccumulator = 0;
	totalRequestAccumulator = 0;
	sharedRequestAccumulator = 0;
	sharedLatencyAccumulator = 0;
	hiddenSharedLatencyAccumulator = 0;
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
		data.push_back((double) hiddenSharedLatencyAccumulator / (double) sharedRequestAccumulator);
		data.push_back(sharedLatencyAccumulator - hiddenSharedLatencyAccumulator);
		data.push_back((double) sharedStallCycleAccumulator / (double) sharedLatencyAccumulator);
	}
	else{
		data.push_back(0);
		data.push_back(0);
		data.push_back(0);
		data.push_back(0);
	}

	stallCycleAccumulator = 0;
	sharedStallCycleAccumulator = 0;
	totalRequestAccumulator = 0;
	sharedRequestAccumulator = 0;
	sharedLatencyAccumulator = 0;
	hiddenSharedLatencyAccumulator = 0;

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
	headers.push_back("Private Memory Stalls");
	headers.push_back("Shared Memory Stalls");
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
	data.push_back(stallCycles[STALL_DMEM_PRIVATE]);
	data.push_back(stallCycles[STALL_DMEM_SHARED]);
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
	headers.push_back("Avg Stall Length");
	headers.push_back("Avg Issue To Stall");
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

	hiddenSharedLoads
		.name(name() + ".num_hidden_shared_loads")
		.desc("Number of shared loads that were hidden behind stores");

	hiddenPrivateLoads
		.name(name() + ".num_hidden_private_loads")
		.desc("Number of private loads that were hidden behind stores");

	hiddenSharedLoadRate
		.name(name() + ".hidden_shared_load_rate")
		.desc("Ratio of hidden loads to actual shared accesses");

	hiddenSharedLoadRate = hiddenSharedLoads / sharedRequestCount;
}

void
MemoryOverlapEstimator::issuedMemoryRequest(MemReqPtr& req){
	DPRINTF(OverlapEstimator, "Issuing memory request for addr %d, command %s\n",
			(req->paddr & ~(CACHE_BLK_SIZE-1)),
			req->cmd);
	pendingRequests.push_back(EstimationEntry(req->paddr & ~(CACHE_BLK_SIZE-1),curTick, req->cmd));
}

bool
compareEEs(EstimationEntry e1, EstimationEntry e2){
	if(e1.completedAt < e2.completedAt) return true;
	return false;
}

void
MemoryOverlapEstimator::l1HitDetected(MemReqPtr& req, Tick finishedAt){

	Addr blkAddr = req->paddr & ~(CACHE_BLK_SIZE-1);
	DPRINTF(OverlapEstimator, "L1 hit detected for addr %d, command %s\n",
				blkAddr,
				req->cmd);

	EstimationEntry ee = EstimationEntry(blkAddr, curTick, req->cmd);
	ee.completedAt = finishedAt;
	ee.isL1Hit = false;

	DPRINTF(OverlapEstimator, "Request is not a store, adding to completed requests with complete at %d\n",
			finishedAt);
	if(!completedRequests.empty()) assert(completedRequests.back().completedAt <= ee.completedAt);
	completedRequests.push_back(ee);

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
	pendingRequests[useIndex].hidesLoad = hiddenLoad;

	totalRequestAccumulator++;

	if(pendingRequests[useIndex].isSharedReq){
		sharedRequestCount++;
		interferenceManager->addSharedReqTotalRoundtrip(req, pendingRequests[useIndex].latency());
	}

	if(pendingRequests[useIndex].isStore() && hiddenLoad){
		if(pendingRequests[useIndex].isSharedReq) hiddenSharedLoads++;
		else hiddenPrivateLoads++;
	}

	DPRINTF(OverlapEstimator, "Memory request for addr %d complete, command %s (original %s), latency %d, %s, adding to completed reqs\n",
			req->paddr,
			req->cmd,
			pendingRequests[useIndex].origCmd,
			pendingRequests[useIndex].latency(),
			(hiddenLoad ? "hidden load" : "no hidden load"));

	if(!completedRequests.empty()) assert(completedRequests.back().completedAt <= pendingRequests[useIndex].completedAt);
	completedRequests.push_back(pendingRequests[useIndex]);

	pendingRequests.erase(pendingRequests.begin()+	useIndex);
}

void
MemoryOverlapEstimator::stalledForMemory(Addr stalledOnCoreAddr){
	assert(!isStalled);
	isStalled = true;
	stalledAt = curTick;
	totalStalls++;

	stalledOnAddr = relocateAddrForCPU(cpuID, stalledOnCoreAddr, cpuCount);

	DPRINTF(OverlapEstimator, "Stalling, oldest core address is %d, relocated to %d\n", stalledOnCoreAddr, stalledOnAddr);
}

void
MemoryOverlapEstimator::executionResumed(bool endedBySquash){
	assert(isStalled);
	isStalled = false;
	resumedAt = curTick;

	Tick stallLength = curTick - stalledAt;
	stallCycleAccumulator += stallLength;

	DPRINTF(OverlapEstimator, "Resuming execution, CPU was stalled for %d cycles, due to %s, stalled on %d\n",
			stallLength,
			(endedBySquash ? "squash" : "memory response"),
			stalledOnAddr);

	int sharedCacheHits = 0;
	int sharedCacheMisses = 0;
	Tick sharedLatency = 0;
	Tick issueToStallLat = 0;
	int privateRequests = 0;
	bool stalledOnShared = false;
	bool stalledOnPrivate = false;

	while(!completedRequests.empty() && completedRequests.front().completedAt < curTick){
		DPRINTF(OverlapEstimator, "Request %d, cmd %s, is part of burst, latency %d, %s, %s\n",
				completedRequests.front().address,
				completedRequests.front().origCmd,
				completedRequests.front().latency(),
				(completedRequests.front().isSharedReq ? "shared": "private"),
				(completedRequests.front().isL1Hit ? "L1 hit": "L1 miss"));

		if(completedRequests.front().isSharedReq){
			if(completedRequests.front().address == stalledOnAddr){
				assert(!completedRequests.front().isStore() || completedRequests.front().hidesLoad);
				stalledOnShared = true;
				issueToStallLat = stalledAt -completedRequests.front().issuedAt;
				DPRINTF(OverlapEstimator, "This request caused the stall, issue to stall %d, stall is shared\n", issueToStallLat);

				hiddenSharedLatencyAccumulator += issueToStallLat;
			}
			else{
				hiddenSharedLatencyAccumulator += completedRequests.front().latency();
			}

			sharedLoadCount++;
			sharedRequestAccumulator++;
			totalLoadLatency += completedRequests.front().latency();
			sharedLatencyAccumulator += completedRequests.front().latency();

			if(completedRequests.front().isSharedCacheMiss) sharedCacheMisses++;
			else sharedCacheHits++;
			sharedLatency += completedRequests.front().latency();
		}
		else{
			if(completedRequests.front().address == stalledOnAddr){
				stalledOnPrivate = true;
			    DPRINTF(OverlapEstimator, "This request caused the stall, stall is private\n");
			}
			privateRequests++;
		}

		completedRequests.erase(completedRequests.begin());

	}

	if(endedBySquash && !(stalledOnShared || stalledOnPrivate)){
		DPRINTF(OverlapEstimator, "Stall ended with squash and there is no completed memory request to blame, defaulting to private stall\n");
		stalledOnPrivate = true;
	}

	assert(stalledOnShared || stalledOnPrivate);

	if(isSharedStall(stalledOnShared, sharedCacheHits+sharedCacheMisses)){

		burstAccumulator += sharedCacheHits+sharedCacheMisses;
		numSharedStalls++;

		sharedStallCycles += stallLength;
		sharedStallCycleAccumulator += stallLength;

		double avgIssueToStall = (double) issueToStallLat / (double) (sharedCacheHits+sharedCacheMisses);

		DPRINTF(OverlapEstimator, "Stall on shared request, updating request group hits %d, misses %d, latency %d, stall %d, avg issue to stall %d\n",
				sharedCacheHits,
				sharedCacheMisses,
				sharedLatency / (sharedCacheHits+sharedCacheMisses),
				stallLength,
				avgIssueToStall);

		updateRequestGroups(sharedCacheHits, sharedCacheMisses, privateRequests, sharedLatency, stallLength, avgIssueToStall);
		addStall(STALL_DMEM_SHARED, stallLength, true);
	}
	else{
		DPRINTF(OverlapEstimator, "Stall on private request\n");
		privateStallCycles += stallLength;

		addStall(STALL_DMEM_PRIVATE, stallLength, true);
	}

	assert(interferenceManager != NULL);
	interferenceManager->addStallCycles(cpuID,
			                            stallLength,
			                            isSharedStall(stalledOnShared, sharedCacheHits+sharedCacheMisses),
			                            true);
}

bool
MemoryOverlapEstimator::isSharedStall(bool oldestInstIsShared, int sharedReqs){
	switch(stallIdentifyAlg){
	case SHARED_STALL_EXISTS:
		if(sharedReqs > 0) return true;
		return false;
		break;
	case SHARED_STALL_ROB:
		return oldestInstIsShared;
		break;
	default:
		fatal("Unknown stall identification algorithm");
	}

	return false;
}

void
MemoryOverlapEstimator::incrementPrivateRequestCount(MemReqPtr& req){
	interferenceManager->incrementPrivateRequestCount(req);
}

void
MemoryOverlapEstimator::addPrivateLatency(MemReqPtr& req, int latency){
	if(interferenceManager->checkForStore(req)) return;
	if(req->instructionMiss) return;
	interferenceManager->addPrivateLatency(InterferenceManager::CacheCapacity, req, latency);
}

void
MemoryOverlapEstimator::addL1Access(MemReqPtr& req, int latency, bool hit){
	if(interferenceManager->checkForStore(req)) return;
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
MemoryOverlapEstimator::updateRequestGroups(int sharedHits, int sharedMisses, int pa, Tick sl, double stallLength, double avgIssueToStall){

	double avgSLat = (double) sl / (double) (sharedHits + sharedMisses);

	for(int i=0;i<groupSignatures.size();i++){
		if(groupSignatures[i].match(sharedHits, sharedMisses)){
			groupSignatures[i].add(pa, avgSLat, stallLength, avgIssueToStall);
			return;
		}
	}

	RequestGroupSignature rgs = RequestGroupSignature(sharedHits, sharedMisses);
	rgs.add(pa, avgSLat, stallLength, avgIssueToStall);
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
	avgStallLength = 0.0;
	avgIssueToStall = 0.0;
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
MemoryOverlapEstimator::RequestGroupSignature::add(double pa, double avgSharedLat, double stallLength, double _avgIssueToStall){

	double curPrivReqTotal = avgPrivateAccesses * entries;
	double curSharedTotal = avgSharedLatency * entries;
	double curStallTotal = avgStallLength * entries;
	double curAvgIssueToStallTotal = avgIssueToStall * entries;

	entries++;
	if(curPrivReqTotal > 0 || pa > 0) avgPrivateAccesses = (curPrivReqTotal + pa) / (double) entries;
	else avgPrivateAccesses = 0;
	if(curSharedTotal > 0 || avgSharedLat > 0) avgSharedLatency = (curSharedTotal + avgSharedLat) / (double) entries;
	else avgSharedLatency = 0;

	avgStallLength = (stallLength + curStallTotal) / (double) entries;
	avgIssueToStall = (_avgIssueToStall + curAvgIssueToStallTotal) / (double) entries;
}

void
MemoryOverlapEstimator::RequestGroupSignature::populate(std::vector<RequestTraceEntry>* data){
	data->push_back(sharedCacheHits);
	data->push_back(sharedCacheMisses);
	data->push_back(avgPrivateAccesses);
	data->push_back(avgSharedLatency);
	data->push_back(avgStallLength);
	data->push_back(avgIssueToStall);
	data->push_back(entries);
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
    Param<int> id;
	Param<int> cpu_count;
	SimObjectParam<InterferenceManager *> interference_manager;
END_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

BEGIN_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
	INIT_PARAM(id, "ID of this estimator"),
	INIT_PARAM(cpu_count, "Number of cores in the system"),
	INIT_PARAM_DFLT(interference_manager, "Interference manager", NULL)
END_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

CREATE_SIM_OBJECT(MemoryOverlapEstimator)
{
	HierParams* params = new HierParams(getInstanceName(), false, true, cpu_count);

    return new MemoryOverlapEstimator(getInstanceName(), id, interference_manager, cpu_count, params);
}

REGISTER_SIM_OBJECT("MemoryOverlapEstimator", MemoryOverlapEstimator)
#endif

