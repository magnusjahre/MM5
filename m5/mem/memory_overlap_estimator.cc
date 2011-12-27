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

MemoryOverlapEstimator::MemoryOverlapEstimator(string name, int id)
: SimObject(name){
	isStalled = false;
	stalledAt = 0;
	resumedAt = 0;
}

void
MemoryOverlapEstimator::issuedMemoryRequest(MemReqPtr& req){
	assert(!req->isStore);

	DPRINTF(OverlapEstimator, "Issuing memory request for addr %d, command %s\n",req->paddr, req->cmd);
	pendingRequests.push_back(EstimationEntry(req->paddr & ~(CACHE_BLK_SIZE-1),curTick));
}

void
MemoryOverlapEstimator::completedMemoryRequest(MemReqPtr& req, Tick finishedAt){

	assert(!req->isStore);

	int useIndex = -1;
	for(int i=0;i<pendingRequests.size();i++){
		if((req->paddr & ~(CACHE_BLK_SIZE-1)) == pendingRequests[i].address){
			assert(useIndex == -1);
			useIndex = i;
		}
	}
	assert(useIndex != -1);

	DPRINTF(OverlapEstimator, "Memory request for addr %d complete, command %s, latency %d\n",
				req->paddr,
				req->cmd,
				finishedAt - pendingRequests[useIndex].issuedAt);

	pendingRequests[useIndex].completedAt = finishedAt;
	completedRequests.push_back(pendingRequests[useIndex]);
	pendingRequests.erase(pendingRequests.begin()+useIndex);
}

void
MemoryOverlapEstimator::stalledForMemory(){
	assert(!isStalled);
	isStalled = true;
	stalledAt = curTick;

	DPRINTF(OverlapEstimator, "Stalling...\n");
}

void
MemoryOverlapEstimator::executionResumed(){
	assert(isStalled);
	isStalled = false;
	resumedAt = curTick;

	DPRINTF(OverlapEstimator, "Resuming execution, CPU was stalled for %d cycles\n", curTick - stalledAt);

	while(!completedRequests.empty() && completedRequests.front().completedAt < stalledAt){
		DPRINTF(OverlapEstimator, "Skipping request for address %d\n", completedRequests.front().address);
		completedRequests.erase(completedRequests.begin());
	}

	vector<EstimationEntry> burst;
	while(!completedRequests.empty() && completedRequests.front().completedAt < curTick){
		DPRINTF(OverlapEstimator, "Request %d is part of burst, latency %d\n",
				completedRequests.front().address,
				completedRequests.front().latency());

		burst.push_back(completedRequests.front());
		completedRequests.erase(completedRequests.begin());

	}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
    Param<int> id;
END_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

BEGIN_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
	INIT_PARAM(id, "ID of this estimator")
END_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

CREATE_SIM_OBJECT(MemoryOverlapEstimator)
{
    return new MemoryOverlapEstimator(getInstanceName(), id);
}

REGISTER_SIM_OBJECT("MemoryOverlapEstimator", MemoryOverlapEstimator)
#endif
