/*
 * critical_path_table.cc
 *
 *  Created on: Jul 18, 2013
 *      Author: jahre
 */

#include "critical_path_table.hh"

using namespace std;

CriticalPathTable::CriticalPathTable(MemoryOverlapEstimator* _moe){
    moe = _moe;

    nextValidPtr = 0;
    stalledOnAddr = 0;
    stalledAt = 0;

    curCommitDepth = 0;

    int bufferSize = 20;
    pendingRequests.resize(bufferSize, CPTRequestEntry());

    pendingCommit.depth = 0;

    lastCompletedRequestIndex = -1;
    lastIsShared = false;
}

int
CriticalPathTable::findRequest(Addr paddr){
    int foundIndex = -1;
    for(int i=0;i<pendingRequests.size();i++){
        if(pendingRequests[i].addr == paddr && pendingRequests[i].valid){
            assert(foundIndex == -1);
            foundIndex = i;
        }
    }
    assert(foundIndex != -1);
    return foundIndex;
}

bool
CriticalPathTable::hasAddress(Addr paddr){
    for(int i=0;i<pendingRequests.size();i++){
        if(pendingRequests[i].addr == paddr && pendingRequests[i].valid){
            return true;
        }
    }
    return false;
}

void
CriticalPathTable::issuedRequest(MemReqPtr& req){
    Addr addr = req->paddr & ~(MOE_CACHE_BLK_SIZE-1);

    DPRINTF(CPLTable, " %s: Got memory request for addr %d, command %s pos %d\n",
            moe->name(),
            addr,
            req->cmd,
            nextValidPtr);

    if(hasAddress(addr)){
        DPRINTF(CPLTable, " %s: Addr %d already allocated, skipping\n",
                    moe->name(),
                    addr);
        return;
    }

    pendingRequests[nextValidPtr].update(addr);
    pendingCommit.children.push_back(nextValidPtr);

    int checkCnt = 0;
    while(pendingRequests[nextValidPtr].valid){
        nextValidPtr = (nextValidPtr + 1) % pendingRequests.size();
        checkCnt++;
        if(checkCnt > pendingRequests.size()) fatal("Ran out of CPL buffer space");
    }

    DPRINTF(CPLTable, " %s: Next valid pointer is now %d\n",
            moe->name(),
            nextValidPtr);
}

void
CriticalPathTable::completedRequest(MemReqPtr& req, bool hiddenLoad){

    DPRINTF(CPLTable, " %s: Memory request completed for addr %d, %s, %s, %s\n",
            moe->name(),
            req->paddr,
            req->beenInSharedMemSys ? "shared request" : "private request",
            hiddenLoad ? "hidden load" : "not hidden load",
            req->isStore ? "store" : "not store");

    int pendingIndex = findRequest(req->paddr);
    if(isSharedRead(req, hiddenLoad)){
        DPRINTF(CPLTable, " %s: Request for address %d (index %d) is shared and complete\n",
                        moe->name(),
                        req->paddr,
                        pendingIndex);

        if(pendingRequests[pendingIndex].depth > pendingCommit.depth){
        	pendingCommit.depth= pendingRequests[pendingIndex].depth;
            DPRINTF(CPLTable, " %s: Setting pending commit depth to %d\n",
                    moe->name(),
                    pendingCommit.depth);

        }

        pendingRequests[pendingIndex].completed = true;
        pendingRequests[pendingIndex].isShared = true;
    }
    else{
        DPRINTF(CPLTable, " %s: Request for address %d (index %d) is not applicable, invalidating it\n",
                moe->name(),
                req->paddr,
                pendingIndex);

        pendingRequests[pendingIndex].completed = true;
        pendingRequests[pendingIndex].isShared = false;
        pendingRequests[pendingIndex].valid = false;
        pendingCommit.removeChild(pendingIndex);
    }

    lastCompletedRequestIndex = pendingIndex;
    lastIsShared = pendingRequests[pendingIndex].isShared;

    DPRINTF(CPLTable, " %s: Last completed request is address %d, %s\n",
                            moe->name(),
                            pendingRequests[lastCompletedRequestIndex].addr,
                            pendingRequests[lastCompletedRequestIndex].isShared ? "shared" : "private");
}

void
CriticalPathTable::CPTCommitEntry::removeChild(int index){

	int foundAt = -1;
	for(int i=0;i<children.size();i++){
		if(children[i] == index){
			assert(foundAt == -1);
			foundAt = i;
		}
	}
	children.erase(children.begin()+foundAt);

	DPRINTF(CPLTable, "Removed child at buffer entry %d at index %d\n",
		               index,
		               foundAt);
}

void
CriticalPathTable::commitPeriodStarted(){
    DPRINTF(CPLTable, " %s: RESUME, commit period started\n", moe->name());

    if(lastIsShared){
    	assert(pendingRequests[lastCompletedRequestIndex].valid);

    	// 1. Update children of resolved commit node (that completed before the stall)

    	// 1.1 Updated the children that are still pending
    	for(int i=0;i<pendingCommit.children.size();i++){
    		int curIndex = pendingCommit.children[i];
    		assert(pendingRequests[curIndex].valid);
    		assert(pendingRequests[curIndex].depth == -1);

    		pendingRequests[curIndex].depth = pendingCommit.depth+1;
    		DPRINTF(CPLTable, " %s: Setting depth of child request %d (index %d) to %d (is %s)\n",
    		    			moe->name(),
    		    			pendingRequests[curIndex].addr,
    		    			curIndex,
    		    			pendingCommit.depth+1,
    		    			pendingRequests[curIndex].completed ? "complete" : "not complete");
    	}

    	// 1.3 Update global maximum depth (i.e. critical path) counter
    	assert(pendingCommit.depth != -1);
    	if(pendingCommit.depth > curCommitDepth){
    		curCommitDepth = pendingCommit.depth;

    		DPRINTF(CPLTable, " %s: this commit is the current deepest at depth %d\n",
    				moe->name(),
    				curCommitDepth);
    	}

    	// 2. Initialize new commit node
    	DPRINTF(CPLTable, " %s: shared request stall, updating last completed commit (duration %d to %d), pending commit start at %d\n",
    			moe->name(),
    			pendingCommit.startedAt,
    			stalledAt,
    			curTick);

    	pendingCommit.reset();
    	pendingCommit.startedAt = curTick;

    	// 2.1 Handle the request that committed last (and cleared the stall)
    	assert(pendingRequests[lastCompletedRequestIndex].depth != -1);
    	assert(pendingCommit.depth == -1);

    	DPRINTF(CPLTable, " %s: Initializing pending commit depth to depth of last completed request %d (address %d)\n",
    			moe->name(),
    			pendingRequests[lastCompletedRequestIndex].depth,
    			pendingRequests[lastCompletedRequestIndex].addr);

    	pendingCommit.depth = pendingRequests[lastCompletedRequestIndex].depth;

    	//2.2 Handle other requests that completed while we where stalled (and which this commit node is the child of)
    	for(int i=0;i<pendingRequests.size();i++){
    		if(pendingRequests[i].valid && pendingRequests[i].completed){

    			if(pendingRequests[i].isShared){
    				if(pendingRequests[i].depth > pendingCommit.depth){
    					DPRINTF(CPLTable, " %s: Request %d (index %d) was deeper, updating pending commit depth to %d\n",
    							moe->name(),
    							pendingRequests[i].addr,
    							i,
    							pendingRequests[i].depth);
    					pendingCommit.depth = pendingRequests[i].depth;
    				}
    			}

    			pendingRequests[i].valid = false;
    			DPRINTF(CPLTable, " %s: Invalidating complete request %d (index %d, depth %d)\n",
    					moe->name(),
    					pendingRequests[i].addr,
    					i,
    					pendingRequests[i].depth);
    		}
    	}

    }
    else{
    	DPRINTF(CPLTable, " %s: private request stall\n", moe->name());
    }

    assert(stalledOnAddr != 0);
    stalledOnAddr = 0;

    assert(stalledAt != 0);
    stalledAt = 0;
}

void
CriticalPathTable::commitPeriodEnded(Addr stalledOn){

    DPRINTF(CPLTable, " %s: STALL, commit period ended, current commit depth %d, stalled on %d\n",
            moe->name(),
            pendingCommit.depth,
            stalledOn);

    assert(stalledOnAddr == 0);
    stalledOnAddr = stalledOn;

    assert(stalledAt == 0);
    stalledAt = curTick;

    assert(pendingCommit.stalledAt == 0);
}

int
CriticalPathTable::getCriticalPathLength(){

    DPRINTF(CPLTable, "%s: Returning current commit depth %d, resetting commit depth\n",
            moe->name(),
            curCommitDepth);

    for(int i=0;i<pendingRequests.size();i++){
        if(pendingRequests[i].valid){
            pendingRequests[i].depth = 0;
            DPRINTF(CPLTable, "%s: Resetting depth for request %d, resetting commit depth\n",
                    moe->name(),
                    pendingRequests[i].depth);
        }
    }

    int tmpCommitDepth = curCommitDepth;
    curCommitDepth = 0;
    return tmpCommitDepth;
}

bool
CriticalPathTable::isSharedRead(MemReqPtr& req, bool hiddenLoad){
    if(req->beenInSharedMemSys){
        if(hiddenLoad){
            DPRINTF(CPLTable, "Request %d hides a load, add it\n", req->paddr);
            return true;
        }
        if(req->isStore){
            DPRINTF(CPLTable, "Request %d is a store, skip it\n", req->paddr);
            return false;
        }
        DPRINTF(CPLTable, "Request %d is a regular load, add it\n", req->paddr);
        return true;
    }
    DPRINTF(CPLTable, "Request %d is private, skip it\n", req->paddr);
    return false;
}
