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

#define CPL_TABLE_OVERFLOW_VALUE 20

MemoryOverlapEstimator::MemoryOverlapEstimator(string name, int id,
		                                       InterferenceManager* _interferenceManager,
		                                       int cpu_count,
		                                       HierParams* params,
		                                       SharedStallIndentifier _ident,
		                                       bool _sharedReqTraceEnabled,
		                                       bool _graphAnalysisEnabled,
		                                       MemoryOverlapTable* _overlapTable,
		                                       int _traceSampleID,
		                                       int _cplTableBufferSize,
		                                       ITCA* _itca)
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
	interferenceManager->registerMemoryOverlapEstimator(this, cpuID);

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

	sampleID = 0;
	traceSampleID = _traceSampleID;

	computeWhilePendingAccumulator = 0;
	computeWhilePendingReqs = 0;
	computeWhilePendingTotalAccumulator = 0;

	pendingComputeNode = new ComputeNode(0, 0);
	lastComputeNode = NULL;
	nextComputeNodeID = 1;
	root = pendingComputeNode;

	overlapTable = _overlapTable;
	doGraphAnalysis = _graphAnalysisEnabled;

	currentStallFullROB = 0;
	boisAloneStallEstimate = 0;

	sharedCacheMissLoads = 0;
	sharedCacheMissStores = 0;

	criticalPathTable = new CriticalPathTable(this, _cplTableBufferSize);
	itca = _itca;
}

MemoryOverlapEstimator::~MemoryOverlapEstimator(){
	delete overlapTable;
	delete criticalPathTable;
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

	for(int i=0;i<pendingRequests.size();i++){
		pendingRequests[i]->commitCyclesWhileActive++;
	}

	criticalPathTable->addCommitCycle();

	assert(interferenceManager != NULL);
	interferenceManager->addCommitCycle(cpuID);
}

void
MemoryOverlapEstimator::addROBFullCycle(){
	currentStallFullROB++;
}

void
MemoryOverlapEstimator::initOverlapTrace(){
	overlapTrace = RequestTrace(name(), "OverlapTrace", true);

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
	headers.push_back("Full ROB While Stalled Cycles");
	headers.push_back("Not Full ROB Stall Cycles");
	headers.push_back("Private Full ROB While Stalled Cycles");
	headers.push_back("Alone Stall Estimate (Bois et al.)");
	headers.push_back("Total Memsys Interference (Bois et al.)");
	headers.push_back("Lost Stall Cycles (Bois et al.)");

	overlapTrace.initalizeTrace(headers);

	stallCycleAccumulator = 0;
	sharedStallCycleAccumulator = 0;
	totalRequestAccumulator = 0;
	sharedRequestAccumulator = 0;
	sharedLatencyAccumulator = 0;
	hiddenSharedLatencyAccumulator = 0;
	stallWithFullROBAccumulator = 0;
	privateStallWithFullROBAccumulator = 0;
	boisAloneStallEstimateTrace = 0;
	boisMemsysInterferenceTrace = 0;
	boisLostStallCycles = 0;
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

	data.push_back(stallWithFullROBAccumulator);
	data.push_back(sharedStallCycleAccumulator - stallWithFullROBAccumulator);
	data.push_back(privateStallWithFullROBAccumulator);
	data.push_back(boisAloneStallEstimateTrace);
	data.push_back(boisMemsysInterferenceTrace);
	data.push_back(boisLostStallCycles);

	assert(boisAloneStallEstimateTrace == (stallCycleAccumulator - (boisMemsysInterferenceTrace + boisLostStallCycles)));

	rss.reset();

	stallCycleAccumulator = 0;
	sharedStallCycleAccumulator = 0;
	totalRequestAccumulator = 0;
	sharedRequestAccumulator = 0;
	sharedLatencyAccumulator = 0;
	hiddenSharedLatencyAccumulator = 0;
	stallWithFullROBAccumulator = 0;
	privateStallWithFullROBAccumulator = 0;
	boisAloneStallEstimateTrace = 0;
	boisMemsysInterferenceTrace = 0;
	boisLostStallCycles = 0;

	issueToStallAccumulator = 0;
	issueToStallAccReqs = 0;

	overlapTrace.addTrace(data);
}

void
MemoryOverlapEstimator::initStallTrace(){
	stallTrace = RequestTrace(name(), "StallTrace");

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
	requestGroupTrace = RequestTrace(name(), "RequestGroupTrace", true);

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

	if(traceSampleID >= 0 && sampleID != traceSampleID) sharedReqTraceEnabled = false;

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
	}
	sharedTraceReqNum++;
}

OverlapStatistics
MemoryOverlapEstimator::sampleCPU(int committedInstructions){

	OverlapStatistics ols = gatherParaMeasurements(committedInstructions);

	traceOverlap(committedInstructions, ols.graphCPL);
	traceStalls(committedInstructions);
	traceRequestGroups(committedInstructions);

	ols.cptMeasurements = criticalPathTable->getCriticalPathLength(sampleID+1);
	ols.tableCPL = ols.cptMeasurements.criticalPathLength;
	ols.itcaAccountedCycles = itca->getAccountedCycles();

	DPRINTF(CPLTableProgress, "Sample %d: Returning ols.cpl %d and tableCPL %d (request number: %d, committed instructions %d)\n",
			sampleID,
			ols.graphCPL,
			ols.tableCPL,
			sharedTraceReqNum,
			committedInstructions);

	DPRINTF(CPLTableProgress, "Sample %d CPT measurements: Table latency %d, table interference %d, table cwp %d, table cpl requests %d\n",
			sampleID,
			ols.cptMeasurements.averageCPLatency(),
			ols.cptMeasurements.averageCPInterference(),
			ols.cptMeasurements.averageCPCWP(),
			ols.cptMeasurements.criticalPathRequests);

	int error = abs(ols.graphCPL - ols.tableCPL);
	cpl_table_error.sample(error);
	if(error > CPL_TABLE_OVERFLOW_VALUE){
		warn("CPL overflow at %d, table %d, graph %d, sample number %d, committed instructions %d",
				curTick,
				ols.tableCPL,
				ols.graphCPL,
				sampleID,
				committedInstructions);
	}

	sampleID++;
	if(sampleID == traceSampleID){
		sharedReqTraceEnabled = true;
	}
	else if(sharedReqTraceEnabled && sampleID != traceSampleID && traceSampleID >= 0){
		sharedReqTraceEnabled = false;
	}

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

	using namespace Stats;

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

	cpl_table_error
		.init(0, CPL_TABLE_OVERFLOW_VALUE, 1)
		.name(name() +".cpl_table_error")
		.desc("Histogram of deviation between graph and table CPL")
		.flags(total | pdf | cdf);

	numSharedStallsForROB
		.name(name() + ".num_shared_stalls_for_rob")
		.desc("Number of stalls due to a request that visited the shared memory system");

	numSharedStallsWithFullROB
		.name(name() + ".num_shared_stalls_with_full_rob")
		.desc("Number of stalls due to a request that visited the shared memory system where the ROB filled at some point");

	sharedStallFullROBRatio
		.name(name() + ".num_shared_stalls_with_full_rob_ratio")
		.desc("Ratio of full ROB stalls to shared stalls");

	sharedStallFullROBRatio = numSharedStallsWithFullROB / numSharedStallsForROB;

	sharedStallNotFullROBRatio
		.name(name() + ".num_shared_stalls_without_full_rob_ratio")
		.desc("Ratio of stalls without full ROB stalls to shared stalls");

	sharedStallNotFullROBRatio = 1 - (numSharedStallsWithFullROB / numSharedStallsForROB);
}

void
MemoryOverlapEstimator::issuedMemoryRequest(MemReqPtr& req){
	DPRINTF(OverlapEstimator, "Issuing memory request for addr %d, command %s\n",
			(req->paddr & ~(MOE_CACHE_BLK_SIZE-1)),
			req->cmd);

	//overlapTable->requestIssued(req);
	criticalPathTable->issuedRequest(req);
	itca->l1DataMiss(req->paddr & ~(MOE_CACHE_BLK_SIZE-1));

	EstimationEntry* ee = new EstimationEntry(nextReqID, req->paddr & ~(MOE_CACHE_BLK_SIZE-1),curTick, req->cmd);
	pendingRequests.push_back(ee);

	nextReqID++;
}

void
MemoryOverlapEstimator::itcaInstructionMiss(Addr addr){
	itca->l1InstructionMiss(addr & ~(MOE_CACHE_BLK_SIZE-1));
}

void
MemoryOverlapEstimator::itcaInstructionMissResolved(Addr addr, Tick willFinishAt){
	itca->l1MissResolved(addr & ~(MOE_CACHE_BLK_SIZE-1), willFinishAt, false);
}

void
MemoryOverlapEstimator::itcaSquash(Addr addr){
	itca->squash(addr & ~(MOE_CACHE_BLK_SIZE-1));
}

void
MemoryOverlapEstimator::l1HitDetected(MemReqPtr& req, Tick finishedAt){

	Addr blkAddr = req->paddr & ~(MOE_CACHE_BLK_SIZE-1);
	DPRINTF(OverlapEstimator, "L1 hit detected for addr %d, command %s\n",
				blkAddr,
				req->cmd);
}

void
MemoryOverlapEstimator::busWritebackCompleted(MemReqPtr& req, Tick finishedAt){

	EstimationEntry* ee = new EstimationEntry(nextReqID,
			                                  req->paddr & ~(MOE_CACHE_BLK_SIZE-1),
			                                  req->writebackGeneratedAt,
			                                  req->cmd);
	nextReqID++;

	ee->completedAt = finishedAt;
	ee->isSharedReq = true;
	ee->isSharedCacheMiss = true;
	ee->isPrivModeSharedCacheMiss = true;
	ee->hidesLoad = false;
	ee->interference = 0;

	DPRINTF(OverlapEstimator, "Writeback for addr %d complete, latency %d, adding to completed reqs\n",
				ee->address,
				ee->latency());

	if(!completedRequests.empty()){
		for(int i=completedRequests.size()-1;i>=0;i--){
			if(completedRequests[i]->completedAt <= ee->completedAt){
				DPRINTF(OverlapEstimator, "Inserting writeback at position %d, currently %d completed requests\n",
								          i+1, completedRequests.size());
				completedRequests.insert(completedRequests.begin()+(i+1), ee);
				break;
			}
		}
		if(ee->completedAt < completedRequests[0]->completedAt){
			completedRequests.insert(completedRequests.begin(), ee);
		}
	}
	else{
		DPRINTF(OverlapEstimator, "Inserting writeback at front in empty list\n");
		completedRequests.push_back(ee);
	}

	// Verify that the list is in fact sorted
	for(int i=0;i<completedRequests.size()-1;i++){
		assert(completedRequests[i]->completedAt <= completedRequests[i+1]->completedAt);
	}
}

void
MemoryOverlapEstimator::completedMemoryRequest(MemReqPtr& req, Tick finishedAt, bool hiddenLoad){

	int useIndex = -1;
	for(int i=0;i<pendingRequests.size();i++){
		if((req->paddr & ~(MOE_CACHE_BLK_SIZE-1)) == pendingRequests[i]->address){
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
	pendingRequests[useIndex]->interference = req->boisInterferenceSum;

	//overlapTable->requestCompleted(req, hiddenLoad);
	criticalPathTable->completedRequest(req, hiddenLoad, finishedAt);
	itca->l1MissResolved(req->paddr, finishedAt, true);

	totalRequestAccumulator++;

	if(pendingRequests[useIndex]->isSharedReq){
		sharedRequestCount++;
		interferenceManager->addSharedReqTotalRoundtrip(req, pendingRequests[useIndex]->latency());

		computeWhilePendingAccumulator += pendingRequests[useIndex]->commitCyclesWhileActive;
		computeWhilePendingReqs++;

		if(pendingRequests[useIndex]->isSharedCacheMiss){
			if(pendingRequests[useIndex]->isStore()) sharedCacheMissStores++;
			else sharedCacheMissLoads++;
		}
	}

	if(pendingRequests[useIndex]->isStore() && hiddenLoad){
		if(pendingRequests[useIndex]->isSharedReq) hiddenSharedLoads++;
		else hiddenPrivateLoads++;

		if(pendingRequests[useIndex]->isSharedReq){
			interferenceManager->hiddenLoadDetected(cpuID);
		}
	}

	DPRINTF(OverlapEstimator, "Memory request for addr %d complete at %d, command %s (original %s), latency %d, %s, adding to completed reqs\n",
			req->paddr,
			finishedAt,
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

	DPRINTF(OverlapEstimatorGraph, "Processing parallelism measurements with %d computes and %d requests\n",
			completedComputeNodes.size(),
			completedRequestNodes.size());

	std::list<MemoryGraphNode* > cycleNodes = findCycleNodes(root);
	assert(checkReachability());
	std::list<MemoryGraphNode* > topologicalOrder = findTopologicalOrder(root);
	ols.graphCPL = findCriticalPathLength(root, 0, topologicalOrder);
	assert(checkReachability());
	findAvgMemoryBusParallelism(topologicalOrder, &cycleNodes, &ols);

	DPRINTF(OverlapEstimatorGraph, "Critical path length is %d\n", ols.graphCPL);

	double burstLenSum = 0.0;
	double burstSizeSum = 0.0;
	double interBurstOverlapSum = 0.0;

	double comWhileBurst = 0.0;

	if(doGraphAnalysis){
		comWhileBurst = findComputeBurstOverlap();

		populateBurstInfo();
		for(int i=0;i<burstInfo.size();i++){

			if(burstInfo[i].finishedAt > 0){
				burstLenSum += burstInfo[i].finishedAt - burstInfo[i].startedAt;
				burstSizeSum += burstInfo[i].numRequests;

				DPRINTF(OverlapEstimatorGraph, "Processing burst %d of length %d with %d requests\n",
						i,
						burstInfo[i].finishedAt - burstInfo[i].startedAt,
						burstInfo[i].numRequests);
			}

			if(i>0){
				if(burstInfo[i].finishedAt > 0 && burstInfo[i-1].finishedAt > 0){
					double overlap = burstInfo[i-1].finishedAt - burstInfo[i].startedAt;
					if(overlap > 0) interBurstOverlapSum += overlap;
				}
			}
		}
	}

	DPRINTF(OverlapEstimatorGraph, "Got burst length %d, burst overlap %d and commit while burst %d\n",
			burstLenSum,
			interBurstOverlapSum,
			comWhileBurst);

	if(ols.graphCPL > 0){
		ols.avgBurstLength = burstLenSum / (double) ols.graphCPL;
		ols.avgBurstSize = burstSizeSum / (double) ols.graphCPL;
		ols.avgInterBurstOverlap = interBurstOverlapSum / (double) ols.graphCPL;
		ols.avgTotalComWhilePend =  (double) computeWhilePendingTotalAccumulator / (double) ols.graphCPL;
		ols.avgComWhileBurst = comWhileBurst / (double) ols.graphCPL;
	}
	else{
		ols.avgBurstLength = 0;
		ols.avgBurstSize = 0;
		ols.avgInterBurstOverlap = 0;
		ols.avgTotalComWhilePend =  0;
		ols.avgComWhileBurst = 0;
	}

	computeWhilePendingTotalAccumulator = 0;
	sharedCacheMissLoads = 0;
	sharedCacheMissStores = 0;
	clearData();

    if(isStalled){
    	assert(lastComputeNode != NULL && pendingComputeNode == NULL);
    	root = lastComputeNode;
    }
    else{
    	assert(lastComputeNode == NULL && pendingComputeNode != NULL);
    	root = pendingComputeNode;
    }
    root->children->clear();
    root->parents->clear();

	DPRINTF(OverlapEstimatorGraph, "Commit %d (%p) is new root\n", root->id, root);

	return ols;
}

void
MemoryOverlapEstimator::clearData(){
	for(int i=0;i<completedComputeNodes.size();i++){
		if(completedComputeNodes[i] == pendingComputeNode){
			assert(lastComputeNode == NULL);
			continue;
		}
		if(completedComputeNodes[i] == lastComputeNode){
			assert(pendingComputeNode == NULL);
			continue;
		}
		DPRINTF(OverlapEstimatorGraph, "Deleting compute %d, ptr %p\n", completedComputeNodes[i]->id, completedComputeNodes[i]);
		for(int j=0;j<completedComputeNodes[i]->children->size();j++) completedComputeNodes[i]->children->at(j) = NULL;
		completedComputeNodes[i]->children->clear();
		delete completedComputeNodes[i];
	}
	for(int i=0;i<completedRequestNodes.size();i++){
		DPRINTF(OverlapEstimatorGraph, "Deleting request %d, ptr %p\n", completedRequestNodes[i]->id, completedRequestNodes[i]);
		for(int j=0;j<completedRequestNodes[i]->children->size();j++) completedRequestNodes[i]->children->at(j) = NULL;
		completedRequestNodes[i]->children->clear();
		delete completedRequestNodes[i];
	}
	//for(int i=0;i<pendingNodes.size();i++) delete pendingNodes[i];

	completedComputeNodes.clear();
	assert(completedComputeNodes.empty());
	completedRequestNodes.clear();
	assert(completedRequestNodes.empty());

	//pendingNodes.clear();
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

void
MemoryGraphNode::addChild(MemoryGraphNode* child){
	DPRINTF(OverlapEstimatorGraph, "Adding child %s-%d (%d) for node %s-%d (%d), currently %d children\n",
					child->name(), child->id, child->getAddr(),
					name(), id, getAddr(),
					children->size());

	children->push_back(child);
	child->addParent(this);

	DPRINTF(OverlapEstimatorGraph, "Added child %s-%d (%d) for node %s-%d (%d), %d children in total\n",
				child->name(), child->id, child->getAddr(),
				name(), id, getAddr(),
				children->size());
}

void
MemoryGraphNode::addParent(MemoryGraphNode* parent){

	DPRINTF(OverlapEstimatorGraph, "Adding parent link to %s-%d (%d), processing %d children\n",
			name(), id, getAddr(), children->size());

	for(int i=0;i<children->size();i++){
		if(children->at(i) == parent){
			DPRINTF(OverlapEstimatorGraph, "Node %s-%d (%d) and node %s-%d (%d) creates a cycle, not storing parent link\n",
					parent->name(), parent->id, parent->getAddr(),
					name(), id, getAddr());
			return;
		}
	}

	parents->push_back(parent);
	validParents++;

	DPRINTF(OverlapEstimatorGraph, "Added parent %s-%d (%d) for node %s-%d (%d), %d parents in total\n",
			parent->name(), parent->id, parent->getAddr(),
			name(), id, getAddr(),
			validParents);

	assert(validParents == parents->size());
}

void
MemoryGraphNode::removeParent(MemoryGraphNode* parent){
	validParents--;
	DPRINTF(OverlapEstimatorGraph, "Removed parent %s-%d of node %s-%d, %d parents left\n",
			parent->name(), parent->id, name(), id, validParents);
	assert(validParents >= 0);
}

std::list<MemoryGraphNode* >
MemoryOverlapEstimator::findCycleNodes(MemoryGraphNode* root){
	list<MemoryGraphNode* > readyNodes;
	list<MemoryGraphNode* > cycleNodes;

	readyNodes.push_back(root);
	while(!readyNodes.empty()){
		MemoryGraphNode* n = readyNodes.front();
		readyNodes.pop_front();

		if(n->isRequest()){
			assert(!n->visited);
			assert(n->children->size() == 1);
			assert(n->parents->size() == 1);
			if(n->children[0] == n->parents[0]){
				DPRINTF(OverlapEstimatorGraph, "Node %s-%d for address %d is a cycle, removing it\n",
							n->name(), n->id, n->getAddr());
				cycleNodes.push_back(n);
				n->children->clear();
			}
		}
		n->visited = true;

		for(int i=0;i<n->children->size();i++){
			MemoryGraphNode* child = n->children->at(i);
			if(!child->visited && !nodeIsInList(child, &readyNodes)){
				readyNodes.push_back(child);
			}
		}

	}
	return cycleNodes;
}

std::list<MemoryGraphNode* >
MemoryOverlapEstimator::findTopologicalOrder(MemoryGraphNode* root){
	list<MemoryGraphNode* > topologicalOrder;
	list<MemoryGraphNode* > readyNodes;

	DPRINTF(OverlapEstimatorGraph, "Topological sort: Adding root node %s-%d (%p) to ready nodes list\n",
			root->name(),
			root->id,
			root);
	readyNodes.push_back(root);

	while(!readyNodes.empty()){
		MemoryGraphNode* n = readyNodes.front();
		readyNodes.pop_front();
		topologicalOrder.push_back(n);

		DPRINTF(OverlapEstimatorGraph, "Adding node %s-%d (addr %d) to the topological order\n",
				n->name(),
				n->id,
				n->getAddr());

		for(int i=0;i<n->children->size();i++){
			MemoryGraphNode* curchild = n->children->at(i);

			DPRINTF(OverlapEstimatorGraph, "Processing child %s-%d (addr %d), parents left %d\n",
					curchild->name(),
					curchild->id,
					curchild->getAddr(),
					curchild->validParents);

			curchild->removeParent(n);
			if(curchild->validParents == 0){
				readyNodes.push_back(curchild);
				DPRINTF(OverlapEstimatorGraph, "Added child %s-%d (addr %d) to the ready nodes list\n",
									curchild->name(),
									curchild->id,
									curchild->getAddr());
			}
		}
	}

	return topologicalOrder;
}

void
MemoryOverlapEstimator::incrementBusParaCounters(MemoryGraphNode* curNode, int* curLoads, int* curStores){
	assert(curNode->isLLCMiss());
	if(curNode->isLoadReq()){
		*curLoads += 1;
	}
	else{
		*curStores += 1;
	}
}

bool
MemoryOverlapEstimator::nodeIsInList(MemoryGraphNode* node, std::list<MemoryGraphNode* >* cycleNodes){
	list<MemoryGraphNode* >::iterator it = cycleNodes->begin();
	for( ; it != cycleNodes->end() ; it++){
		if(*it == node){
			DPRINTF(OverlapEstimatorGraph, "Node %s-%d (addr %d) is in the list\n",
					node->name(), node->id, node->getAddr());
			return true;
		}
	}
	return false;
}

void
MemoryOverlapEstimator::findAvgMemoryBusParallelism(std::list<MemoryGraphNode* > topologicalOrder,
		                                            std::list<MemoryGraphNode* >* cycleNodes,
													OverlapStatistics* ols){

	if(ols->graphCPL == 0.0){
		DPRINTF(OverlapEstimatorGraph, "CPL is 0, returning average bus parallelism 0\n");
		ols->globalAvgMemBusPara = 0.0;
		return;
	}

	// Method 1: Global average burst
	int cycleLLCMisses = 0;
	for(list<MemoryGraphNode* >::iterator it = cycleNodes->begin(); it != cycleNodes->end();it++){
		if((*it)->isLLCMiss()) cycleLLCMisses++;
	}

	ols->globalAvgMemBusPara = ((double) sharedCacheMissLoads + (double) sharedCacheMissStores - (double) cycleLLCMisses) / (double) ols->graphCPL;

	DPRINTF(OverlapEstimatorGraph, "%d shared cache miss loads, %d shared cache miss stores, %d cycle LLC misses, cpl %d, avg bus parallelism %f\n",
			sharedCacheMissLoads,
			sharedCacheMissStores,
			cycleLLCMisses,
			ols->graphCPL,
			ols->globalAvgMemBusPara);

	// Method 2: Burst size histogram
	int curDepth = 0;
	int curLoads = 0;
	int curStores = 0;
	while(!topologicalOrder.empty()){
		MemoryGraphNode* curNode = topologicalOrder.front();
		curNode->visited = true; //for reachability analysis
		topologicalOrder.pop_front();

		if(curNode->isRequest() && curNode->isLLCMiss() && !nodeIsInList(curNode, cycleNodes)){
			if(curNode->depth > curDepth){
				if(curLoads+curStores > 0){
					DPRINTF(OverlapEstimatorGraph, "Detected burst size at depth %d, loads %d, stores %d\n",
							curDepth,
							curLoads,
							curStores);
					ols->addHistorgramEntry(OverlapStatisticsHistogramEntry(curLoads, curStores));
				}
				curDepth = curNode->depth;

				curLoads = 0;
				curStores = 0;
				incrementBusParaCounters(curNode, &curLoads, &curStores);
			}
			else{
				incrementBusParaCounters(curNode, &curLoads, &curStores);
			}
		}
	}

	if(curLoads+curStores > 0){
		DPRINTF(OverlapEstimatorGraph, "Detected burst size at depth %d, loads %d, stores %d\n",
				curDepth,
				curLoads,
				curStores);
		ols->addHistorgramEntry(OverlapStatisticsHistogramEntry(curLoads, curStores));
	}
}

int
MemoryOverlapEstimator::findCriticalPathLength(MemoryGraphNode* node,
											   int depth,
											   std::list<MemoryGraphNode* > topologicalOrder){

	assert(!node->addToCPL());
	node->depth = depth;

	int maxdepth = 0;
	while(!topologicalOrder.empty()){
		MemoryGraphNode* curNode = topologicalOrder.front();
		curNode->visited = true; //for reachability analysis
		topologicalOrder.pop_front();

		DPRINTF(OverlapEstimatorGraph, "Processing node %s-%d (addr %d), depth %d\n",
				curNode->name(),
				curNode->id,
				curNode->getAddr(),
				curNode->depth);

		for(int i=0;i<curNode->parents->size();i++){
			MemoryGraphNode* curParent = curNode->parents->at(i);
			DPRINTF(OverlapEstimatorGraph, "Parent %s-%d (addr %d) has depth %d\n",
					curParent->name(),
					curParent->id,
					curParent->getAddr(),
					curParent->depth);

			int newDepth = curParent->depth;
			if(curNode->isRequest()) newDepth++;

			if(newDepth > curNode->depth){
				DPRINTF(OverlapEstimatorGraph, "Depth of %s-%d (addr %d) updated to depth %d\n",
						curNode->name(),
						curNode->id,
						curNode->getAddr(),
						newDepth);

				curNode->depth = newDepth;
			}
		}

		if(curNode->depth > maxdepth){
			maxdepth = curNode->depth;
			DPRINTF(OverlapEstimatorGraph, "New maxdepth %d\n",
					maxdepth);
		}
	}

	return maxdepth;
}

void
MemoryOverlapEstimator::populateBurstInfo(){
	for(int i=0;i<completedComputeNodes.size();i++){
		vector<BurstStats> bsvec;

		for(int j=0;j<completedComputeNodes[i]->children->size();j++){

			DPRINTF(OverlapEstimatorGraph, "Processing children of compute node %d\n",
					completedComputeNodes[i]->id);

			if(completedComputeNodes[i]->children->at(j)->finishedAt > 0){
				assert(completedComputeNodes[i]->children->at(j)->children->size() < 2);

				DPRINTF(OverlapEstimatorGraph, "Processing request %d, start at %d, finished at%d\n",
						completedComputeNodes[i]->children->at(j)->id,
						completedComputeNodes[i]->children->at(j)->startedAt,
						completedComputeNodes[i]->children->at(j)->startedAt);

				bool added = false;
				for(int k=0;k<bsvec.size();k++){
					if(bsvec[k].overlaps(completedComputeNodes[i]->children->at(j))){
						if(!added){
							bsvec[k].addRequest(completedComputeNodes[i]->children->at(j));
							added = true;
						}
					}
				}

				if(!added){
					BurstStats bs = BurstStats();
					bs.addRequest(completedComputeNodes[i]->children->at(j));
					bsvec.push_back(bs);
				}
			}
		}

		for(int k=0;k<bsvec.size();k++){
			assert(bsvec[k].numRequests > 0);
			burstInfo.push_back(bsvec[k]);
		}
	}
}

void
MemoryOverlapEstimator::stalledForMemory(Addr stalledOnCoreAddr){
	assert(!isStalled);
	assert(currentStallFullROB == 0);
	isStalled = true;
	stalledAt = curTick;
	cacheBlockedCycles = 0;
	totalStalls++;

	itca->setROBHeadAddr(stalledOnCoreAddr);

	assert(pendingComputeNode != NULL);
	pendingComputeNode->finishedAt = curTick;
	lastComputeNode = pendingComputeNode;
	pendingComputeNode = NULL;

	stalledOnAddr = relocateAddrForCPU(cpuID, stalledOnCoreAddr, cpuCount);
	//overlapTable->executionStalled();
	criticalPathTable->commitPeriodEnded(stalledOnAddr);

	DPRINTF(OverlapEstimator, "Stalling, oldest core address is %d, relocated to %d\n", stalledOnCoreAddr, stalledOnAddr);
}

Tick
MemoryOverlapEstimator::getBoisAloneStallEstimate(){
    Tick tmp = boisAloneStallEstimate;
    boisAloneStallEstimate = 0;
    return tmp;
}

void
MemoryOverlapEstimator::addBoisEstimateCycles(Tick aloneStallTicks){
    boisAloneStallEstimateTrace += aloneStallTicks;
    boisAloneStallEstimate += aloneStallTicks;
}

void
MemoryOverlapEstimator::executionResumed(bool endedBySquash){
	assert(isStalled);
	isStalled = false;
	resumedAt = curTick;

	Tick stallLength = curTick - stalledAt;
	stallCycleAccumulator += stallLength;

	itca->clearROBHeadAddr();

	DPRINTF(OverlapEstimator, "Resuming execution, CPU was stalled for %d cycles, due to %s, stalled on %d, blocked for %d cycles, pending completed requests is %d, full ROB for %d cycles\n",
			stallLength,
			(endedBySquash ? "squash" : "memory response"),
			stalledOnAddr,
			cacheBlockedCycles,
			completedRequests.size(),
			currentStallFullROB);

	int sharedCacheHits = 0;
	int sharedCacheMisses = 0;
	Tick sharedLatency = 0;
	Tick issueToStallLat = 0;
	int privateRequests = 0;
	bool stalledOnShared = false;
	bool stalledOnPrivate = false;

	vector<RequestNode* > completedSharedReqs;

	//overlapTable->executionResumed();
	criticalPathTable->commitPeriodStarted();

	assert(stallLength > currentStallFullROB);
	addBoisEstimateCycles(stallLength - currentStallFullROB);
	DPRINTF(OverlapEstimator, "Bois estimate: adding %d pre full-ROB stall cycles\n",
	        stallLength - currentStallFullROB);

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
				if(completedRequests.front()->address == stalledOnAddr
				   && completedRequests.front()->completedAt >= stalledAt){
					assert(!stalledOnShared);
					stalledOnShared = true;
					issueToStallLat = stalledAt - completedRequests.front()->issuedAt;
					DPRINTF(OverlapEstimator, "This request caused the stall, issue to stall %d, stall is shared\n",
							issueToStallLat);

					hiddenSharedLatencyAccumulator += issueToStallLat;

					issueToStallAccumulator += issueToStallLat;
					issueToStallAccReqs++;

					traceSharedRequest(completedRequests.front(), stalledAt, curTick);
					innerCausedStall = true;

					Tick totalInterference = completedRequests.front()->interference;
					// Du Bois defines interference as any additional latency when the request is
					// at the head of a full ROB. We define any additional latency to be interference.
					// Consequently, Du Bois have the ability to detect stalls that are removed by the
					// CPUs latency hiding techniques
					if(currentStallFullROB > totalInterference){
						boisMemsysInterferenceTrace += totalInterference;
						Tick estAloneStall = currentStallFullROB - totalInterference;
						addBoisEstimateCycles(estAloneStall);

						DPRINTF(OverlapEstimator, "Bois estimate: adding %d alone stall cycles (full ROB stall %d, interference %d), addr %d\n",
								estAloneStall,
								currentStallFullROB,
								totalInterference,
								completedRequests.front()->address);
					}
					else{
						boisLostStallCycles += currentStallFullROB;
					}
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
					DPRINTF(OverlapEstimator, "This request involved in the stall, stall is private\n");
				}
				privateRequests++;
			}
		}
		else{
			assert(completedRequests.front()->isStore());
			if(completedRequests.front()->isSharedReq){
				completedSharedReqs.push_back(buildRequestNode(completedRequests.front(), false));
				traceSharedRequest(completedRequests.front(), 0, 0);
			}
		}

		delete completedRequests.front();
		completedRequests.erase(completedRequests.begin());
	}

	for(int i=0;i<completedRequests.size();i++){
		assert(completedRequests[i]->completedAt >= curTick);
	}


	bool boisAdded = false;
	if(!stalledOnShared){
		boisAdded = true;
		addBoisEstimateCycles(currentStallFullROB);
		DPRINTF(OverlapEstimator, "Bois estimate: adding %d private stall cycles\n", currentStallFullROB);
	}

	if(endedBySquash && !(stalledOnShared || stalledOnPrivate)){
		DPRINTF(OverlapEstimator, "Stall ended with squash and there is no completed memory request to blame, defaulting to private stall\n");
		stalledOnPrivate = true;

		if(!boisAdded){
			boisAdded = true;
			addBoisEstimateCycles(currentStallFullROB);
			DPRINTF(OverlapEstimator, "Bois estimate: end by squash, adding whole stall %d\n", currentStallFullROB);
		}
	}

	// Note: both might be true since multiple accesses to a cache block generates one shared request
	if(!stalledOnShared && !stalledOnPrivate){
	    stalledOnPrivate = true; // stall was due to an L1 hit, but we don't record those

	    if(!boisAdded){
	    	boisAdded = true;
	    	addBoisEstimateCycles(currentStallFullROB);
	    	DPRINTF(OverlapEstimator, "Bois estimate: L1 stall, adding whole stall %d\n", currentStallFullROB);
	    }
	}
	assert(stalledOnShared || stalledOnPrivate);

	assert(boisAloneStallEstimateTrace == stallCycleAccumulator - (boisMemsysInterferenceTrace + boisLostStallCycles));

	processCompletedRequests(stalledOnShared, completedSharedReqs);

	Tick reportStall = stallLength;
	Tick blockedStall = 0;

	if(isSharedStall(stalledOnShared, sharedCacheHits+sharedCacheMisses, 0)){

		burstAccumulator += sharedCacheHits+sharedCacheMisses;
		numSharedStalls++;

		sharedStallCycles += stallLength;
		sharedStallCycleAccumulator += stallLength;
		stallWithFullROBAccumulator += currentStallFullROB;

		if(currentStallFullROB > 0) numSharedStallsWithFullROB++;
		numSharedStallsForROB++;

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
		privateStallWithFullROBAccumulator += currentStallFullROB;

		assert(stallLength >= cacheBlockedCycles);
		reportStall = stallLength - cacheBlockedCycles;
		blockedStall = cacheBlockedCycles;

		addStall(STALL_DMEM_PRIVATE, stallLength, true);
	}

	currentStallFullROB = 0;

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
MemoryOverlapEstimator::processCompletedRequests(bool stalledOnShared, std::vector<RequestNode* > reqs){
	assert(pendingComputeNode == NULL && lastComputeNode != NULL);
	if(!stalledOnShared){
		pendingComputeNode = lastComputeNode;
		lastComputeNode = NULL;
	}
	else{

		assert(!pointerExists(lastComputeNode));

		completedComputeNodes.push_back(lastComputeNode);
		try{
			pendingComputeNode = new ComputeNode(nextComputeNodeID, curTick);
		}
		catch(bad_alloc& ba){
			fatal("Could not allocate new compute node");
		}
		assert(pendingComputeNode != NULL);

		DPRINTF(OverlapEstimatorGraph, "PROCESSING SHARED STALL: compute node id %d complete (%d - %d), node %d pending (%p)\n",
				lastComputeNode->id,
				lastComputeNode->startedAt,
				lastComputeNode->finishedAt,
				pendingComputeNode->id,
				pendingComputeNode);

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

		assert(!reqs[i]->privateMemsysReq);

		setParent(reqs[i]);
		setChild(reqs[i]);


		assert(!pointerExists(reqs[i]));
		completedRequestNodes.push_back(reqs[i]);

		if(reqs[i]->causedStall) causedStallCnt++;
	}
	if(stalledOnShared) assert(causedStallCnt == 1);
	else assert(causedStallCnt == 0);
}

bool
MemoryOverlapEstimator::pointerExists(MemoryGraphNode* ptr){
	bool found = false;
	for(int j=0;j<completedRequestNodes.size();j++){
		if(completedRequestNodes[j] == ptr) found = true;
	}

	for(int i=0;i<completedComputeNodes.size();i++){
		if(completedComputeNodes[i] == ptr) found = true;
	}
	return found;
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
	if(!completedComputeNodes.empty()) assert(minid >= 0 && minid < completedComputeNodes.size());

	if(mindist == TICK_MAX){
		if(completedComputeNodes.size() > 0){
			DPRINTF(OverlapEstimatorGraph, "Request node id %d (%p) started before first compute, setting parent to oldest compute %d (%p)\n",
					node->id,
					node,
					completedComputeNodes[0]->id,
					completedComputeNodes[0]);

			completedComputeNodes[0]->addChild(node);
			}
		else{
			assert(lastComputeNode == NULL && pendingComputeNode != NULL);
			DPRINTF(OverlapEstimatorGraph, "Request node id %d (%p) started during first compute, setting parent to oldest compute %d (%p)\n",
					node->id,
					node,
					pendingComputeNode->id,
					pendingComputeNode);
			pendingComputeNode->addChild(node);
		}
		return;
	}

	assert(pendingComputeNode != NULL);
	if(node->distanceToParent(completedComputeNodes[minid]) < node->distanceToParent(pendingComputeNode)){

		DPRINTF(OverlapEstimatorGraph, "Request node id %d (%p) is the child of compute %d (%p), distance %d\n",
				node->id,
				node,
				completedComputeNodes[minid]->id,
				completedComputeNodes[minid],
				mindist);

		completedComputeNodes[minid]->addChild(node);
	}
	else{
		DPRINTF(OverlapEstimatorGraph, "Distance %d to pending is less than %d to completed. Request node id %d (%p) is the child of compute %d (%p)\n",
				node->distanceToParent(pendingComputeNode),
				node->distanceToParent(completedComputeNodes[minid]),
				node->id,
				node,
				pendingComputeNode->id,
				pendingComputeNode);
		pendingComputeNode->addChild(node);
	}
}

void
MemoryOverlapEstimator::setChild(RequestNode* node){
	if(node->causedStall){
		DPRINTF(OverlapEstimatorGraph, "Request %d caused stall, pending compute id %d (%p) is the child of request %d (%p)\n",
				node->id,
				pendingComputeNode->id,
				pendingComputeNode,
				node->id,
				node);

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
		if(!completedComputeNodes.empty()) assert(minid >= 0 && minid < completedComputeNodes.size());

		assert(pendingComputeNode != NULL);
		if(mindist == TICK_MAX){
			DPRINTF(OverlapEstimatorGraph, "No finished compute after request %d, compute node id %d (%p) is the child of request %d (%p)\n",
					node->id,
					pendingComputeNode->id,
					pendingComputeNode,
					node->id,
					node);

			node->addChild(pendingComputeNode);
		}
		else{
			DPRINTF(OverlapEstimatorGraph, "Compute node id %d (%p) is the child of request %d (%p), distance %d\n",
							completedComputeNodes[minid]->id,
							completedComputeNodes[minid],
							node->id,
							node,
							minid);

			node->addChild(completedComputeNodes[minid]);
		}
	}
}

RequestNode*
MemoryOverlapEstimator::buildRequestNode(EstimationEntry* entry, bool causedStall){

	RequestNode* rn = NULL;
	try{
		rn = new RequestNode(reqNodeID, entry->address, entry->issuedAt);
	}
	catch(bad_alloc& ba){
		fatal("Could not allocate new request node");
	}
	assert(rn != NULL);

	rn->finishedAt = entry->completedAt;
	rn->isLoad = !entry->isStore();
	rn->isSharedCacheMiss = entry->isSharedCacheMiss;
	rn->privateMemsysReq = false;
	rn->causedStall = causedStall;
	reqNodeID++;

	DPRINTF(OverlapEstimatorGraph, "New request node created with id %d (%p), address %d, issued at %d, completed at %d, %s, %s\n",
			rn->id,
			rn,
			rn->addr,
			rn->startedAt,
			rn->finishedAt,
			(rn->causedStall ? "caused stall" : "did not cause stall"),
			rn->isSharedCacheMiss ? "shared cache miss" : "shared cache hit");

	return rn;
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
BurstStats::addRequest(MemoryGraphNode* node){
	if(node->finishedAt > 0){
		assert(node->startedAt < node->finishedAt);
		if(startedAt > 0 && finishedAt > 0) assert(overlaps(node));

		if(node->startedAt < startedAt) startedAt = node->startedAt;
		if(node->finishedAt > finishedAt) finishedAt = node->finishedAt;
		numRequests++;

		assert(startedAt < finishedAt);
	}
}

bool
BurstStats::overlaps(MemoryGraphNode* node){
	if(node->finishedAt < startedAt || node->startedAt > finishedAt){
		DPRINTF(OverlapEstimatorGraph, "No overlap: Burst %d, %d and request %d, %d\n",
				startedAt,
				finishedAt,
				node->startedAt,
				node->finishedAt);
		return false;
	}

	DPRINTF(OverlapEstimatorGraph, "Overlap detected: Burst %d, %d and request %d, %d\n",
					startedAt,
					finishedAt,
					node->startedAt,
					node->finishedAt);
	return true;
}

OverlapStatistics::OverlapStatistics(){
	graphCPL = 0;
	tableCPL = 0;
	avgBurstSize = 0.0;
	avgBurstLength = 0.0;
	avgInterBurstOverlap = 0.0;
	avgTotalComWhilePend = 0.0;
	avgComWhileBurst = 0.0;
	globalAvgMemBusPara = 0.0;
	itcaAccountedCycles = 0;
}

void
OverlapStatistics::addHistorgramEntry(OverlapStatisticsHistogramEntry entry){
	assert(entry.parallelism() > 0);

	bool found = false;
	for(int i=0;i<memBusParaHistogram.size();i++){
		if(memBusParaHistogram[i].parallelism() == entry.parallelism()){
			assert(!found);
			memBusParaHistogram[i].numBursts += 1;
			DPRINTF(OverlapEstimatorGraph, "Found existing entry with parallelism %d, number of bursts is now %d\n",
					memBusParaHistogram[i].parallelism(),
					memBusParaHistogram[i].numBursts);
			found = true;
		}
	}
	if(!found){
		DPRINTF(OverlapEstimatorGraph, "New histogram entry with parallelism %d, number of bursts %d\n",
				entry.parallelism(),
				entry.numBursts);

		memBusParaHistogram.push_back(entry);
	}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
    Param<int> id;
	Param<int> cpu_count;
	SimObjectParam<InterferenceManager *> interference_manager;
	Param<string> shared_stall_heuristic;
	Param<bool> shared_req_trace_enabled;
	Param<bool> graph_analysis_enabled;
	SimObjectParam<MemoryOverlapTable *> overlapTable;
	Param<int> trace_sample_id;
	Param<int> cpl_table_size;
	SimObjectParam<ITCA *> itca;
END_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

BEGIN_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
	INIT_PARAM(id, "ID of this estimator"),
	INIT_PARAM(cpu_count, "Number of cores in the system"),
	INIT_PARAM_DFLT(interference_manager, "Interference manager", NULL),
	INIT_PARAM_DFLT(shared_stall_heuristic, "The heuristic that decides if a processor stall is due to a shared event", "rob"),
	INIT_PARAM_DFLT(shared_req_trace_enabled, "Trace all requests (warning: will create large files)", false),
	INIT_PARAM_DFLT(graph_analysis_enabled, "Analyze miss graph to determine data (warning: performance overhead)", false),
	INIT_PARAM_DFLT(overlapTable, "Overlap table", NULL),
	INIT_PARAM_DFLT(trace_sample_id, "The id of the sample to trace, traces all if -1 (default)", -1),
	INIT_PARAM_DFLT(cpl_table_size, "The size of the CPL table", 64),
	INIT_PARAM(itca, "A pointer to the class implementing the ITCA accounting scheme")
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
    		                          shared_req_trace_enabled,
    		                          graph_analysis_enabled,
    		                          overlapTable,
    		                          trace_sample_id,
    		                          cpl_table_size,
    		                          itca);
}

REGISTER_SIM_OBJECT("MemoryOverlapEstimator", MemoryOverlapEstimator)
#endif


