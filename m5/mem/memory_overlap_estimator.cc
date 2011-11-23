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
}

void
MemoryOverlapEstimator::issuedMemoryRequest(MemReqPtr& req){
	assert(!req->isStore);
	pendingRequests.push_back(EstimationEntry(req->paddr & ~(CACHE_BLK_SIZE-1),curTick));
	cout << curTick << ": issued request for addr " << (req->paddr & ~(CACHE_BLK_SIZE-1)) << ", store " << (req->isStore ? "set" : "not set") << "\n";
}

void
MemoryOverlapEstimator::completedMemoryRequest(MemReqPtr& req, Tick finishedAt){

	assert(!req->isStore);

	cout << curTick << ": Request for addr " << (req->paddr & ~(CACHE_BLK_SIZE-1)) << ", cmd "<< req->cmd <<", completed at " << finishedAt << "\n";

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

	cout << curTick << ": Pending " << pendingRequests.size() << ", completed " << completedRequests.size() << "\n";
	if(curTick == 928){
		for(int i=0;i<pendingRequests.size();i++){
			cout << (pendingRequests[i].address & ~(CACHE_BLK_SIZE-1)) << "\n";
		}
	}
}

void
MemoryOverlapEstimator::stalledForMemory(){
	assert(!isStalled);
	isStalled = true;
	stalledAt = curTick;
	cout << curTick << ": STALLING\n";
}

void
MemoryOverlapEstimator::executionResumed(){
	assert(isStalled);
	isStalled = false;

	cout << curTick << ": RESUMING\n";

	while(!completedRequests.empty() && completedRequests.front().completedAt < stalledAt){
		cout << "Req " << completedRequests.front().address << " completed before stall\n";
		completedRequests.erase(completedRequests.begin());
	}

	vector<EstimationEntry> burst;
	while(!completedRequests.empty() && completedRequests.front().completedAt < curTick){
		burst.push_back(completedRequests.front());
		cout << "Request in burst, start at " << completedRequests.front().issuedAt << " fin at " << completedRequests.front().completedAt << ", latency " << completedRequests.front().latency() << "\n";
		completedRequests.erase(completedRequests.begin());
	}

	cout << curTick << ": burst of size " << burst.size() << "\n";
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
