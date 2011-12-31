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

	lastActivityCycle = 0;
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
	}

	stallCycles[cause] += cycles;
}

void
MemoryOverlapEstimator::addCommitCycle(){

	assert(!isStalled);
	assert(lastActivityCycle == curTick-1);
	lastActivityCycle = curTick;
	commitCycles++;
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
	headers.push_back("Store BW Stalls");
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
	data.push_back(stallCycles[STALL_PRIVATE_DMEM]);
	data.push_back(stallCycles[STALL_SHARED_DMEM]);
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
MemoryOverlapEstimator::sampleCPU(int committedInstructions){
	traceOverlap(committedInstructions);
	traceStalls(committedInstructions);
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

	int burstSize = 0;
	while(!completedRequests.empty() && completedRequests.front().completedAt < curTick){
		DPRINTF(OverlapEstimator, "Request %d is part of burst, latency %d, %s\n",
				completedRequests.front().address,
				completedRequests.front().latency(),
				(completedRequests.front().isSharedReq ? "shared": "private") );

		if(completedRequests.front().isSharedReq){
			burstSize++;
		}
		completedRequests.erase(completedRequests.begin());

	}

	if(burstSize > 0){
		burstAccumulator += burstSize;
		numSharedStalls++;

		sharedStallCycles += stallLength;
		sharedStallCycleAccumulator += stallLength;

		assert(interferenceManager != NULL);
		interferenceManager->addStallCycles(cpuID, stallLength, true);
		addStall(STALL_SHARED_DMEM, stallLength, true);
	}
	else{
		privateStallCycles += stallLength;
		addStall(STALL_PRIVATE_DMEM, stallLength, true);
	}
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
