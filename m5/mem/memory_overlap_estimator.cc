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

MemoryOverlapEstimator::MemoryOverlapEstimator(string name, int id,
		                                       InterferenceManager* _interferenceManager,
		                                       int cpu_count,
		                                       HierParams* params,
		                                       SharedStallIndentifier _ident,
		                                       bool _sharedReqTraceEnabled)
: BaseHier(name, params){
	isStalled = false;
	stalledAt = 0;
	resumedAt = 0;
	stalledOnAddr = 0;
	cacheBlockedCycles = 0;

	stallIdentifyAlg = _ident;

	nextReqID = 0;
	reqNodeID = 0;

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

	issueToStallAccumulator = 0;
	issueToStallAccReqs = 0;

	isStalledOnWrite = false;
	numWriteStalls = 0;

	sharedReqTraceEnabled = _sharedReqTraceEnabled;
	initSharedRequestTrace();

	computeWhilePendingAccumulator = 0;
	computeWhilePendingTotalAccumulator = 0;

	pendingComputeNode = new ComputeNode(0, 0);
	lastComputeNode = NULL;
	nextComputeNodeID = 1;
	root = pendingComputeNode;
}

void
MemoryOverlapEstimator::cpuStarted(Tick firstTick){
	lastActivityCycle = firstTick-1;
	stallCycles[STALL_OTHER] += firstTick-1;
}

void
MemoryOverlapEstimator::addStall(StallCause cause, Tick cycles, bool memStall){

	assert(!isStalled);

	if(cause != STALL_STORE_BUFFER) isStalledOnWrite = false;

	if(memStall){
		//memory stalls are detected one cycle late
		assert(lastActivityCycle == curTick-cycles-1);
		lastActivityCycle = curTick-1;
	}
	else{
		assert(lastActivityCycle == curTick-1);
		lastActivityCycle = curTick;

		assert(interferenceManager != NULL);
		if(cause == STALL_STORE_BUFFER){
			if(!isStalledOnWrite) numWriteStalls++;
			isStalledOnWrite = true;
			interferenceManager->addStallCycles(cpuID, 0, false, false, cycles, 0, 0);
		}
		else if(cause == STALL_EMPTY_ROB){
			interferenceManager->addStallCycles(cpuID, 0, false, false, 0, 0, cycles);
		}
		else{
			interferenceManager->addMemIndependentStallCycle(cpuID);
		}
	}

	stallCycles[cause] += cycles;
}

void
MemoryOverlapEstimator::addCommitCycle(){

	assert(!isStalled);
	assert(lastActivityCycle == curTick-1);
	lastActivityCycle = curTick;
	commitCycles++;
	isStalledOnWrite = false;

	if(!pendingNodes.empty()){
		// FIXME: this may potentially include overlap with private system requests
		//        but we don't know if a request is private or shared before is
		//        completes
		computeWhilePendingTotalAccumulator++;
	}

	for(int i=0;i<pendingNodes.size();i++)pendingNodes[i]->commitCyclesWhileActive++;

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
	headers.push_back("Avg Issue to Stall");
	headers.push_back("Num Shared Stalls");
	headers.push_back("CPL");
	headers.push_back("Average Fan Out");
	headers.push_back("Shared Miss Rate");
	headers.push_back("Private Miss Rate");
	headers.push_back("RSS Requests");
	headers.push_back("Avg CWP");

	overlapTrace.initalizeTrace(headers);

	stallCycleAccumulator = 0;
	sharedStallCycleAccumulator = 0;
	totalRequestAccumulator = 0;
	sharedRequestAccumulator = 0;
	sharedLatencyAccumulator = 0;
	hiddenSharedLatencyAccumulator = 0;
}

void
MemoryOverlapEstimator::traceOverlap(int committedInstructions, int cpl){
	vector<RequestTraceEntry> data;

	data.push_back(committedInstructions);
	data.push_back(stallCycleAccumulator);
	data.push_back(sharedStallCycleAccumulator);
	data.push_back(totalRequestAccumulator);
	data.push_back(sharedRequestAccumulator);

	double avgSharedLat = 0.0;

	if(sharedRequestAccumulator > 0){
		avgSharedLat = (double) sharedLatencyAccumulator / (double) sharedRequestAccumulator;
		data.push_back(avgSharedLat);
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

	if(issueToStallAccumulator > 0){
		data.push_back((double) issueToStallAccumulator / (double) issueToStallAccReqs);
	}
	else{
		data.push_back(0.0);
	}
	data.push_back(issueToStallAccReqs);

	assert(rss.smSharedCacheHits + rss.smSharedCacheMisses == rss.sharedRequests);
	assert(rss.pmSharedCacheHits + rss.pmSharedCacheMisses == rss.sharedRequests);

	double pmMissRate = (double) rss.smSharedCacheMisses / (double) rss.sharedRequests;
	double avgFanOut = (double) rss.sharedRequests / (double) cpl;

	data.push_back(cpl);
	data.push_back(avgFanOut);
	data.push_back((double) rss.pmSharedCacheMisses / (double) rss.sharedRequests);
	data.push_back(pmMissRate);
	data.push_back(rss.sharedRequests);

	data.push_back((double) computeWhilePendingAccumulator / (double) computeWhilePendingReqs);

	rss.reset();

	stallCycleAccumulator = 0;
	sharedStallCycleAccumulator = 0;
	totalRequestAccumulator = 0;
	sharedRequestAccumulator = 0;
	sharedLatencyAccumulator = 0;
	hiddenSharedLatencyAccumulator = 0;

	issueToStallAccumulator = 0;
	issueToStallAccReqs = 0;

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
	headers.push_back("Empty ROB Stalls");
	headers.push_back("Unknown Stalls");
	headers.push_back("Other Stalls");
	headers.push_back("Num write stalls");

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
	data.push_back(stallCycles[STALL_EMPTY_ROB]);
	data.push_back(stallCycles[STALL_UNKNOWN]);
	data.push_back(stallCycles[STALL_OTHER]);
	data.push_back(numWriteStalls);

	stallTrace.addTrace(data);

	Tick sum = commitCycles;
	for(int i=0;i<NUM_STALL_CAUSES;i++){
		sum += stallCycles[i];
	}
	assert(sum == cyclesSinceLast);

	for(int i=0;i<NUM_STALL_CAUSES;i++) stallCycles[i] = 0;
	commitCycles = 0;

	numWriteStalls = 0;
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
MemoryOverlapEstimator::initSharedRequestTrace(){
	sharedTraceReqNum = 0;

	if(sharedReqTraceEnabled){
		sharedRequestTrace = RequestTrace(name(), "SharedRequestTrace");

		vector<string> headers;
		headers.push_back("Number");
		headers.push_back("Address");
		headers.push_back("Issued At");
		headers.push_back("Completed At");
		headers.push_back("Shared Cache Miss");
		headers.push_back("Private Mode Shared Cache Miss");
		headers.push_back("Caused Stall At");
		headers.push_back("Caused Resume At");

		sharedRequestTrace.initalizeTrace(headers);
	}

}

void
MemoryOverlapEstimator::traceSharedRequest(EstimationEntry* entry, Tick stalledAt, Tick resumedAt){
	if(sharedReqTraceEnabled){
		vector<RequestTraceEntry> data;

		data.push_back(sharedTraceReqNum);
		data.push_back(entry->address);
		data.push_back(entry->issuedAt);
		data.push_back(entry->completedAt);
		data.push_back(entry->isSharedCacheMiss ? 1 : 0);
		data.push_back(entry->isPrivModeSharedCacheMiss ? 1 : 0);
		data.push_back(stalledAt);
		data.push_back(resumedAt);

		sharedRequestTrace.addTrace(data);

		sharedTraceReqNum++;
	}
}

OverlapStatistics
MemoryOverlapEstimator::sampleCPU(int committedInstructions){

	OverlapStatistics ols = gatherParaMeasurements(committedInstructions);

	traceOverlap(committedInstructions, ols.cpl);
	traceStalls(committedInstructions);
	traceRequestGroups(committedInstructions);

	return ols;
}

int
MemoryOverlapEstimator::getNumWriteStalls(){
	// is reset in traceStalls
	return numWriteStalls;
}

double
MemoryOverlapEstimator::getAvgCWP(){
	if(computeWhilePendingReqs == 0) return 0.0;
	double cwp = (double) computeWhilePendingAccumulator / (double) computeWhilePendingReqs;
	computeWhilePendingAccumulator = 0;
	computeWhilePendingReqs = 0;
	return cwp;
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

	EstimationEntry* ee = new EstimationEntry(nextReqID, req->paddr & ~(CACHE_BLK_SIZE-1),curTick, req->cmd);
	pendingRequests.push_back(ee);

	nextReqID++;
}


void
MemoryOverlapEstimator::l1HitDetected(MemReqPtr& req, Tick finishedAt){

	Addr blkAddr = req->paddr & ~(CACHE_BLK_SIZE-1);
	DPRINTF(OverlapEstimator, "L1 hit detected for addr %d, command %s\n",
				blkAddr,
				req->cmd);

//	EstimationEntry ee = EstimationEntry(nextReqID, blkAddr, curTick, req->cmd);
//	ee.completedAt = finishedAt;
//	ee.isL1Hit = true;
//	nextReqID++;
//
//	DPRINTF(OverlapEstimator, "Request is not a store, adding to completed requests with complete at %d\n",
//			finishedAt);
//	if(!completedRequests.empty()) assert(completedRequests.back().completedAt <= ee.completedAt);
//	completedRequests.push_back(ee);

}

//RequestNode*
//MemoryOverlapEstimator::findPendingNode(int id){
//  for(int i=0;i<pendingNodes.size();i++){
//    if(pendingNodes[i]->id == id) return pendingNodes[i];
//  }
//  return NULL;
//}
//
//
//void
//MemoryOverlapEstimator::removePendingNode(int id, bool sharedreq){
//  int removeIndex = -1;
//  for(int i=0;i<pendingNodes.size();i++){
//    if(pendingNodes[i]->id == id){
//      assert(removeIndex == -1);
//      removeIndex = i;
//    }
//  }
//  assert(removeIndex != -1);
//  if(sharedreq){
//	  computeWhilePendingAccumulator += pendingNodes[removeIndex]->commitCyclesWhileActive;
//	  computeWhilePendingReqs++;
//  }
//  pendingNodes.erase(pendingNodes.begin()+removeIndex);
//}


void
MemoryOverlapEstimator::completedMemoryRequest(MemReqPtr& req, Tick finishedAt, bool hiddenLoad){

	int useIndex = -1;
	for(int i=0;i<pendingRequests.size();i++){
		if((req->paddr & ~(CACHE_BLK_SIZE-1)) == pendingRequests[i]->address){
			assert(useIndex == -1);
			useIndex = i;
		}
	}
	assert(useIndex != -1);

	pendingRequests[useIndex]->completedAt = finishedAt;
	pendingRequests[useIndex]->isSharedReq = req->beenInSharedMemSys;
	pendingRequests[useIndex]->isSharedCacheMiss = req->isSharedCacheMiss;
	pendingRequests[useIndex]->isPrivModeSharedCacheMiss = req->isPrivModeSharedCacheMiss;
	pendingRequests[useIndex]->hidesLoad = hiddenLoad;

	totalRequestAccumulator++;

	if(pendingRequests[useIndex]->isSharedReq){
		sharedRequestCount++;
		interferenceManager->addSharedReqTotalRoundtrip(req, pendingRequests[useIndex]->latency());
	}

	if(pendingRequests[useIndex]->isStore() && hiddenLoad){
		if(pendingRequests[useIndex]->isSharedReq) hiddenSharedLoads++;
		else hiddenPrivateLoads++;

		if(pendingRequests[useIndex]->isSharedReq){
			interferenceManager->hiddenLoadDetected(cpuID);
		}
	}

	DPRINTF(OverlapEstimator, "Memory request for addr %d complete, command %s (original %s), latency %d, %s, adding to completed reqs\n",
			req->paddr,
			req->cmd,
			pendingRequests[useIndex]->origCmd,
			pendingRequests[useIndex]->latency(),
			(hiddenLoad ? "hidden load" : "no hidden load"));

	if(!completedRequests.empty()) assert(completedRequests.back()->completedAt <= pendingRequests[useIndex]->completedAt);
	completedRequests.push_back(pendingRequests[useIndex]);

	assert(useIndex < pendingRequests.size());
	assert(useIndex >= 0);
	vector<EstimationEntry*>::iterator useIterator = pendingRequests.begin()+useIndex;
	assert(useIterator != pendingRequests.end());
	pendingRequests.erase(useIterator);
}

void
MemoryOverlapEstimator::RequestSampleStats::addStats(EstimationEntry entry){
	if(entry.isSharedCacheMiss) smSharedCacheMisses++;
	else smSharedCacheHits++;

	if(entry.isPrivModeSharedCacheMiss) pmSharedCacheMisses++;
	else pmSharedCacheHits++;

	sharedRequests++;
}

double
MemoryOverlapEstimator::findComputeBurstOverlap(){
	double totalOverlap = 0.0;
	for(int i=0;i<completedComputeNodes.size();i++){
		vector<bool> occupied = vector<bool>(completedComputeNodes[i]->lat(), false);
		for(int j=0;j<completedRequestNodes.size();j++){
			if(completedRequestNodes[j]->finishedAt == 0) continue;
			if(!completedRequestNodes[j]->isLoad) continue;
			if(completedRequestNodes[j]->finishedAt < completedComputeNodes[i]->startedAt) continue;
			if(completedRequestNodes[j]->startedAt > completedComputeNodes[i]->finishedAt) continue;

			for(int k=0;k<occupied.size();k++){
				if(completedRequestNodes[j]->during(k+completedComputeNodes[i]->startedAt)){
					occupied[k] = true;
				}
			}
		}

		for(int k=0;k<occupied.size();k++){
			if(occupied[k]){
				totalOverlap += 1.0;
			}
		}
	}

	return totalOverlap;
}

OverlapStatistics
MemoryOverlapEstimator::gatherParaMeasurements(int committedInsts){
	OverlapStatistics ols = OverlapStatistics();

	ols.cpl = findCriticalPathLength(root, root->children, 0) + 1;
	assert(checkReachability());

	populateBurstInfo();

	double burstLenSum = 0.0;
	double burstSizeSum = 0.0;
	double interBurstOverlapSum = 0.0;

	double comWhileBurst = findComputeBurstOverlap();

	for(int i=0;i<burstInfo.size();i++){

		if(burstInfo[i].finishedAt > 0){
			burstLenSum += burstInfo[i].finishedAt - burstInfo[i].startedAt;
			burstSizeSum += burstInfo[i].numRequests;
		}

		if(i>0){
			if(burstInfo[i].finishedAt > 0 && burstInfo[i-1].finishedAt > 0){
				double overlap = burstInfo[i-1].finishedAt - burstInfo[i].startedAt;
				if(overlap > 0) interBurstOverlapSum += overlap;
			}
		}
	}

	cout << "got cpl " << ols.cpl << ", interburst overlap " << interBurstOverlapSum << " and comWhileBurst " << comWhileBurst << ", sum burst lat " << burstLenSum << "\n";

	if(ols.cpl > 0){
		ols.avgBurstLength = burstLenSum / (double) ols.cpl;
		ols.avgBurstSize = burstSizeSum / (double) ols.cpl;
		ols.avgInterBurstOverlap = interBurstOverlapSum / (double) ols.cpl;
		ols.avgTotalComWhilePend =  (double) computeWhilePendingTotalAccumulator / (double) ols.cpl;
		ols.avgComWhileBurst = comWhileBurst / (double) ols.cpl;
	}
	else{
		ols.avgBurstLength = 0;
		ols.avgBurstSize = 0;
		ols.avgInterBurstOverlap = 0;
		ols.avgTotalComWhilePend =  0;
		ols.avgComWhileBurst = 0;
	}

	computeWhilePendingTotalAccumulator = 0;
	clearData();

    if(isStalled){
    	assert(lastComputeNode != NULL && pendingComputeNode == NULL);
    	root = lastComputeNode;
    }
    else{
    	assert(lastComputeNode == NULL && pendingComputeNode != NULL);
    	root = pendingComputeNode;
    }
    root->children.clear();

	return ols;
}

void
MemoryOverlapEstimator::clearData(){
	for(int i=0;i<completedComputeNodes.size();i++){
		if(completedComputeNodes[i] == pendingComputeNode) continue;
		if(completedComputeNodes[i] == lastComputeNode) continue;
		delete completedComputeNodes[i];
	}
	for(int i=0;i<completedRequestNodes.size();i++) delete completedRequestNodes[i];
	for(int i=0;i<pendingNodes.size();i++) delete pendingNodes[i];
	completedComputeNodes.clear();
	completedRequestNodes.clear();
	pendingNodes.clear();
	burstInfo.clear();
	root = NULL;
}

bool
MemoryOverlapEstimator::checkReachability(){
	bool allReachable = true;

	for(int i=0;i<completedComputeNodes.size();i++){
		if(!completedComputeNodes[i]->visited){
			DPRINTF(OverlapEstimatorGraph, "Compute node %d is not reachable\n", completedComputeNodes[i]->id);
			allReachable = false;
		}
	}
	for(int i=0;i<completedRequestNodes.size();i++){
		if(!completedRequestNodes[i]->visited){
			DPRINTF(OverlapEstimatorGraph, "Request node %d is not reachable\n", completedRequestNodes[i]->id);
			allReachable = false;
		}
	}

	unsetVisited();
	return allReachable;
}

void
MemoryOverlapEstimator::unsetVisited(){
	for(int i=0;i<completedComputeNodes.size();i++) completedComputeNodes[i]->visited = false;
	for(int i=0;i<completedRequestNodes.size();i++) completedRequestNodes[i]->visited = false;
}

int
MemoryOverlapEstimator::findCriticalPathLength(MemoryGraphNode* node, std::vector<MemoryGraphNode*> children, int depth){
	int maxdepth = depth;
	node->visited = true;

	for(int i=0;i<children.size();i++){
		if(!children[i]->visited){

			int tmp = -1;
			if(children[i]->addToCPL()){
				if(children[i]->finishedAt == 0) assert(children[i]->children.size() == 0);

				tmp = findCriticalPathLength(children[i], children[i]->children, depth+1);
			}
			else{
				tmp = findCriticalPathLength(children[i], children[i]->children, depth);
			}

			assert(tmp != -1);
			if(tmp > maxdepth) maxdepth = tmp;
		}
	}
	return maxdepth;
}

void
MemoryOverlapEstimator::populateBurstInfo(){
	for(int i=0;i<completedComputeNodes.size();i++){
		BurstStats bs = BurstStats();
		int numAdded = 0;
		for(int j=0;j<completedComputeNodes[i]->children.size();j++){
			if(completedComputeNodes[i]->children[j]->finishedAt > 0){
				assert(completedComputeNodes[i]->children[j]->children.size() < 2);
				bs.addRequest(completedComputeNodes[i]->children[j]->startedAt, completedComputeNodes[i]->children[j]->finishedAt);
				numAdded++;
			}
		}
		if(numAdded > 0) burstInfo.push_back(bs);
	}
}

void
MemoryOverlapEstimator::stalledForMemory(Addr stalledOnCoreAddr){
	assert(!isStalled);
	isStalled = true;
	stalledAt = curTick;
	cacheBlockedCycles = 0;
	totalStalls++;

	assert(pendingComputeNode != NULL);
	pendingComputeNode->finishedAt = curTick;
	lastComputeNode = pendingComputeNode;
	pendingComputeNode = NULL;

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

	DPRINTF(OverlapEstimator, "Resuming execution, CPU was stalled for %d cycles, due to %s, stalled on %d, blocked for %d cycles, pending completed requests is %d\n",
			stallLength,
			(endedBySquash ? "squash" : "memory response"),
			stalledOnAddr,
			cacheBlockedCycles,
			completedRequests.size());

	int sharedCacheHits = 0;
	int sharedCacheMisses = 0;
	Tick sharedLatency = 0;
	Tick issueToStallLat = 0;
	int privateRequests = 0;
	bool stalledOnShared = false;
	bool stalledOnPrivate = false;

	vector<RequestNode* > completedSharedReqs;

	while(!completedRequests.empty() && completedRequests.front()->completedAt < curTick){
		DPRINTF(OverlapEstimator, "Request %d, cmd %s, is part of burst, latency %d, %s, %s, %s%s\n",
				completedRequests.front()->address,
				completedRequests.front()->origCmd,
				completedRequests.front()->latency(),
				(completedRequests.front()->isSharedReq ? "shared": "private"),
				(completedRequests.front()->isL1Hit ? "L1 hit": "L1 miss"),
				(completedRequests.front()->isStore() ? "store": "load"),
				(completedRequests.front()->hidesLoad ? ", hides load": ""));

		if(!completedRequests.front()->isStore() || completedRequests.front()->hidesLoad){
			if(completedRequests.front()->isSharedReq){
				bool innerCausedStall = false;
				if(completedRequests.front()->address == stalledOnAddr){
					assert(!stalledOnShared);
					stalledOnShared = true;
					issueToStallLat = stalledAt - completedRequests.front()->issuedAt;
					DPRINTF(OverlapEstimator, "This request caused the stall, issue to stall %d, stall is shared\n", issueToStallLat);

					hiddenSharedLatencyAccumulator += issueToStallLat;

					issueToStallAccumulator += issueToStallLat;
					issueToStallAccReqs++;

					traceSharedRequest(completedRequests.front(), stalledAt, curTick);
					innerCausedStall = true;
				}
				else{
					hiddenSharedLatencyAccumulator += completedRequests.front()->latency();
					traceSharedRequest(completedRequests.front(), 0, 0);
				}

				sharedLoadCount++;
				sharedRequestAccumulator++;
				totalLoadLatency += completedRequests.front()->latency();
				sharedLatencyAccumulator += completedRequests.front()->latency();

				if(completedRequests.front()->isSharedCacheMiss) sharedCacheMisses++;
				else sharedCacheHits++;
				sharedLatency += completedRequests.front()->latency();

				completedSharedReqs.push_back(buildRequestNode(completedRequests.front(), innerCausedStall));
			}
			else{
				if(completedRequests.front()->address == stalledOnAddr){
					stalledOnPrivate = true;
					DPRINTF(OverlapEstimator, "This request involved in the store, stall is private\n");
				}
				privateRequests++;
			}
		}

		delete completedRequests.front();
		completedRequests.erase(completedRequests.begin());
	}

	for(int i=0;i<completedRequests.size();i++){
		assert(completedRequests[i]->completedAt >= curTick);
	}

	if(endedBySquash && !(stalledOnShared || stalledOnPrivate)){
		DPRINTF(OverlapEstimator, "Stall ended with squash and there is no completed memory request to blame, defaulting to private stall\n");
		stalledOnPrivate = true;
	}

	// Note: both might be true since multiple accesses to a cache block generates one shared request
	if(!stalledOnShared && !stalledOnPrivate) stalledOnPrivate = true; // stall was due to an L1 hit, but we don't record those
	assert(stalledOnShared || stalledOnPrivate);

	processCompletedRequests(stalledOnPrivate, completedSharedReqs);

	Tick reportStall = stallLength;
	Tick blockedStall = 0;

	if(isSharedStall(stalledOnShared, sharedCacheHits+sharedCacheMisses, 0)){

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

		assert(stallLength >= cacheBlockedCycles);
		reportStall = stallLength - cacheBlockedCycles;
		blockedStall = cacheBlockedCycles;

		addStall(STALL_DMEM_PRIVATE, stallLength, true);
	}


	assert(interferenceManager != NULL);
	interferenceManager->addStallCycles(cpuID,
			                            reportStall,
			                            isSharedStall(stalledOnShared, sharedCacheHits+sharedCacheMisses, 0),
			                            true,
			                            0,
			                            blockedStall,
			                            0);
}

void
MemoryOverlapEstimator::processCompletedRequests(bool stalledOnPrivate, std::vector<RequestNode* > reqs){
	assert(pendingComputeNode == NULL && lastComputeNode != NULL);
	if(stalledOnPrivate){
		pendingComputeNode = lastComputeNode;
		lastComputeNode = NULL;
	}
	else{
		completedComputeNodes.push_back(lastComputeNode);
		pendingComputeNode = new ComputeNode(nextComputeNodeID, curTick);

		DPRINTF(OverlapEstimatorGraph, "PROCESSING SHARED STALL: compute node id %d complete (%d - %d), node %d pending\n",
				lastComputeNode->id,
				lastComputeNode->startedAt,
				lastComputeNode->finishedAt,
				pendingComputeNode->id);

		lastComputeNode = NULL;
		nextComputeNodeID++;
	}
	assert(pendingComputeNode != NULL && lastComputeNode == NULL);

	int causedStallCnt = 0;
	for(int i=0;i<reqs.size();i++){
		DPRINTF(OverlapEstimatorGraph, "Processing request node id %d, address %d, issued at %d, completed at %d\n",
				reqs[i]->id,
				reqs[i]->addr,
				reqs[i]->startedAt,
				reqs[i]->finishedAt);
		setParent(reqs[i]);
		setChild(reqs[i]);
		completedRequestNodes.push_back(reqs[i]);
		if(reqs[i]->causedStall) causedStallCnt++;
	}
	if(stalledOnPrivate) assert(causedStallCnt == 0);
	else assert(causedStallCnt == 1);
}

void
MemoryOverlapEstimator::setParent(RequestNode* node){
	Tick mindist = TICK_MAX;
	int minid = 0;
	for(int i=0;i<completedComputeNodes.size();i++){
		if(node->distanceToParent(completedComputeNodes[i]) < mindist){
			mindist = node->distanceToParent(completedComputeNodes[i]);
			minid = i;
		}
	}

	assert(mindist != TICK_MAX);
	assert(pendingComputeNode != NULL);
	if(node->distanceToParent(completedComputeNodes[minid]) < node->distanceToParent(pendingComputeNode)){

		DPRINTF(OverlapEstimatorGraph, "Request node id %d is the child of compute %d, distance %d\n",
				node->id,
				completedComputeNodes[minid]->id,
				mindist);

		completedComputeNodes[minid]->addChild(node);
	}
	else{
		DPRINTF(OverlapEstimatorGraph, "Distance %d to pending is less than %d to completed. Request node id %d is the child of compute %d\n",
				node->distanceToParent(pendingComputeNode),
				node->distanceToParent(completedComputeNodes[minid]),
				node->id,
				pendingComputeNode->id);
		pendingComputeNode->addChild(node);
	}
}

void
MemoryOverlapEstimator::setChild(RequestNode* node){
	if(node->causedStall){
		DPRINTF(OverlapEstimatorGraph, "Request %d caused stall, pending compute id %d is the child of request %d\n",
				node->id,
				pendingComputeNode->id,
				node->id);

		node->addChild(pendingComputeNode);
	}
	else{
		Tick mindist = TICK_MAX;
		int minid = 0;
		for(int i=0;i<completedComputeNodes.size();i++){
			if(node->distanceToChild(completedComputeNodes[i]) < mindist){
				mindist = node->distanceToChild(completedComputeNodes[i]);
				minid = i;
			}
		}

		assert(pendingComputeNode != NULL);
		if(mindist == TICK_MAX){
			DPRINTF(OverlapEstimatorGraph, "No finished compute after request %d, compute node id %d is the child of request %d\n",
					node->id,
					pendingComputeNode->id,
					node->id);

			node->addChild(pendingComputeNode);
		}
		else{
			DPRINTF(OverlapEstimatorGraph, "Compute node id %d is the child of request %d, distance %d\n",
							completedComputeNodes[minid]->id,
							node->id,
							mindist);

			node->addChild(completedComputeNodes[mindist]);
		}
	}
}

RequestNode*
MemoryOverlapEstimator::buildRequestNode(EstimationEntry* entry, bool causedStall){
	RequestNode* rn = new RequestNode(reqNodeID, entry->address, entry->issuedAt);
	rn->finishedAt = entry->completedAt;
	rn->isLoad = true;
	rn->privateMemsysReq = false;
	rn->causedStall = causedStall;
	reqNodeID++;

	DPRINTF(OverlapEstimatorGraph, "New request node created with id %d, address %d, issued at %d, completed at %d, %s\n",
			rn->id,
			rn->addr,
			rn->startedAt,
			rn->finishedAt,
			(rn->causedStall ? "caused stall" : "did not cause stall"));

	return rn;
}

void
MemoryOverlapEstimator::printGraph(){
	for(int i=0;i<completedComputeNodes.size();i++){
		for(int j = 0;j<completedComputeNodes[i]->children.size();j++){
			cout << "comp-" << completedComputeNodes[i]->id << " --> req-" << completedComputeNodes[i]->children[j]->id << "\n";
		}
	}

	for(int i=0;i<completedRequestNodes.size();i++){
		for(int j=0;j<completedRequestNodes[i]->children.size();j++){
			cout << "req-" << completedRequestNodes[i]->id << " --> comp-" << completedRequestNodes[i]->children[j]->id << "\n";
		}
	}

}

bool
MemoryOverlapEstimator::isSharedStall(bool oldestInstIsShared, int sharedReqs, int numSharedWrites){
	switch(stallIdentifyAlg){
	case SHARED_STALL_EXISTS:
		if(sharedReqs > 0) return true;
		return false;
		break;
	case SHARED_STALL_ROB:
		return oldestInstIsShared;
		break;
	case SHARED_STALL_ROB_WRITE:
		return oldestInstIsShared || numSharedWrites > 0;
		break;
	default:
		fatal("Unknown stall identification algorithm");
		break;
	}

	return false;
}

void
MemoryOverlapEstimator::addDcacheStallCycle(){
	cacheBlockedCycles++;
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

BurstStats::BurstStats(){
	startedAt = 100000000000;
	finishedAt = 0;
	numRequests = 0;
}

void
BurstStats::addRequest(Tick start, Tick end){
	if(end > 0){
		assert(start < end);

		if(start < startedAt) startedAt = start;
		if(end > finishedAt) finishedAt = end;
		numRequests++;

		assert(startedAt < finishedAt);
	}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
    Param<int> id;
	Param<int> cpu_count;
	SimObjectParam<InterferenceManager *> interference_manager;
	Param<string> shared_stall_heuristic;
	Param<bool> shared_req_trace_enabled;
END_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

BEGIN_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
	INIT_PARAM(id, "ID of this estimator"),
	INIT_PARAM(cpu_count, "Number of cores in the system"),
	INIT_PARAM_DFLT(interference_manager, "Interference manager", NULL),
	INIT_PARAM_DFLT(shared_stall_heuristic, "The heuristic that decides if a processor stall is due to a shared event", "rob"),
	INIT_PARAM_DFLT(shared_req_trace_enabled, "Trace all requests (warning: will create large files)", false)
END_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

CREATE_SIM_OBJECT(MemoryOverlapEstimator)
{
	HierParams* params = new HierParams(getInstanceName(), false, true, cpu_count);

	MemoryOverlapEstimator::SharedStallIndentifier ident;
	if((string) shared_stall_heuristic == "shared-exists"){
		ident = MemoryOverlapEstimator::SHARED_STALL_EXISTS;
	}
	else if((string) shared_stall_heuristic == "rob"){
		ident = MemoryOverlapEstimator::SHARED_STALL_ROB;
	}
	else if((string) shared_stall_heuristic == "rob-write"){
		ident = MemoryOverlapEstimator::SHARED_STALL_ROB_WRITE;
	}
	else{
		fatal("Unknown shared stall heuristic");
	}

    return new MemoryOverlapEstimator(getInstanceName(),
    		                          id,
    		                          interference_manager,
    		                          cpu_count,
    		                          params,
    		                          ident,
    		                          shared_req_trace_enabled);
}

REGISTER_SIM_OBJECT("MemoryOverlapEstimator", MemoryOverlapEstimator)
#endif


