/*
 * memory_overlap_estimator.cc
 *
 *  Created on: Nov 18, 2011
 *      Author: jahre
 */

#include "memory_overlap_estimator.hh"
#include "sim/builder.hh"

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

	pendingRequests[useIndex].completedAt = finishedAt;
	completedRequests.push_back(pendingRequests[useIndex]);
	pendingRequests.erase(pendingRequests.begin()+useIndex);
}

void
MemoryOverlapEstimator::stalledForMemory(){
	assert(!isStalled);
	isStalled = true;
	stalledAt = curTick;
}

void
MemoryOverlapEstimator::executionResumed(){
	assert(isStalled);
	isStalled = false;
	resumedAt = curTick;

	while(!completedRequests.empty() && completedRequests.front().completedAt < stalledAt){
		completedRequests.erase(completedRequests.begin());
	}

	vector<EstimationEntry> burst;
	while(!completedRequests.empty() && completedRequests.front().completedAt < curTick){
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
