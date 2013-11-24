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
    stalledOnAddr = MemReq::inval_addr;
    stalledAt = 0;

    commitIDCounter = 0;
    curCommitDepth = 0;

    int bufferSize = 20;
    pendingRequests.resize(bufferSize, CPTRequestEntry());

    pendingCommit.depth = 0;
    prevCommitDepth = 0;

    traceSampleID = _moe->getTraceSampleID();
    currentSampleID = 0;
    initDependencyEdgeTrace();
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

    DPRINTF(CPLTable, " %s: Got memory request for addr %d, command %s pos %d, current commit depth %d\n",
            moe->name(),
            addr,
            req->cmd,
            nextValidPtr,
            pendingCommit.depth);

    if(hasAddress(addr)){
        DPRINTF(CPLTable, " %s: Addr %d already allocated, skipping\n",
                    moe->name(),
                    addr);
        return;
    }

    pendingRequests[nextValidPtr].update(addr);
    assert(pendingCommit.startedAt <= curTick);
    if(pendingCommit.startedAt == curTick){
    	DPRINTF(CPLTable, " %s: Request %d is issued in the same cycle as commit resumes, child of previous commit (depth %d)\n",
    	                    moe->name(),
    	                    addr,
    	                    prevCommitDepth);

    	updateChildRequest(nextValidPtr, prevCommitDepth, pendingCommit.id-1);
    }
    else{
    	pendingCommit.children.push_back(nextValidPtr);
    }

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
CriticalPathTable::handleCompletedRequestWhileCommitting(int pendingIndex){
	if(pendingRequests[pendingIndex].depth > pendingCommit.depth){
		pendingCommit.depth = pendingRequests[pendingIndex].depth;
		DPRINTF(CPLTable, " %s: Setting pending commit depth to %d\n",
				moe->name(),
				pendingCommit.depth);

		updateCommitDepthCounter(pendingCommit.depth);
	}

	DPRINTF(CPLTable, " %s: Invalidating complete request for address %d, depth %d\n",
	        				moe->name(),
	        				pendingRequests[pendingIndex].addr,
	        				pendingCommit.depth);

	pendingRequests[pendingIndex].completed = true;
	pendingRequests[pendingIndex].isShared = true;
	pendingRequests[pendingIndex].valid = false;

	bool found = pendingCommit.removeChild(pendingIndex);

	if(found) traceDependencyEdge(pendingCommit.id, pendingRequests[pendingIndex].addr, false);
	traceDependencyEdge(pendingRequests[pendingIndex].addr, pendingCommit.id, true);
}

void
CriticalPathTable::completedRequest(MemReqPtr& req, bool hiddenLoad, Tick willCompleteAt){

	DPRINTF(CPLTable, " %s: Memory request for addr %d will complete at %d, event scheduled\n",
			moe->name(),
			req->paddr,
			willCompleteAt);

	assert(willCompleteAt > curTick);
	CPTMemoryRequestCompletionEvent* event = new CPTMemoryRequestCompletionEvent(this, req, hiddenLoad);
	event->schedule(willCompleteAt);
}

void
CriticalPathTable::handleCompletedRequestEvent(MemReqPtr& req, bool hiddenLoad){

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


        // If we still committing, the request is the parent of this commit and we can update it directly
        if(!isStalled()){
        	handleCompletedRequestWhileCommitting(pendingIndex);
        }
        // If we are stalled on a shared request, the request is the parent of the next commit.
        // If we are stalled on a private request, it is the parent of the current commit. Unfortunately,
        // we don't know if this is a shared or a private stall until the stall resolves. Therefore, we have to
        // defer the processing of such requests.
        else{
        	pendingRequests[pendingIndex].deferred = true;
        	pendingRequests[pendingIndex].completed = true;
        	pendingRequests[pendingIndex].isShared = true;

        	DPRINTF(CPLTable, " %s: Processing of depth for address %d (%d) is deferred\n",
        			moe->name(),
        			req->paddr,
        			pendingIndex);
        }
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
}

bool
CriticalPathTable::CPTCommitEntry::removeChild(int index){

	int foundAt = -1;
	for(int i=0;i<children.size();i++){
		if(children[i] == index){
			assert(foundAt == -1);
			foundAt = i;
		}
	}

	if(foundAt == -1){
		DPRINTF(CPLTable, "Removed request at index %d is not a child of the current commit\n",
				index);
		return false;
	}

	children.erase(children.begin()+foundAt);

	DPRINTF(CPLTable, "Removed child at buffer entry %d at index %d\n",
		               index,
		               foundAt);

	return true;
}

bool
CriticalPathTable::CPTCommitEntry::hasChild(int index){
    for(int i=0;i<children.size();i++){
        if(index == children[i]) return true;
    }
    return false;
}

void
CriticalPathTable::updateChildRequest(int bufferIndex, int depth, int commitID){
	assert(pendingRequests[bufferIndex].valid);
	assert(pendingRequests[bufferIndex].depth == -1);

	int newdepth = depth + 1;

	pendingRequests[bufferIndex].depth = newdepth;
	DPRINTF(CPLTable, " %s: Setting depth of child request %d (index %d) to %d (is %s)\n",
			moe->name(),
			pendingRequests[bufferIndex].addr,
			bufferIndex,
			newdepth,
			pendingRequests[bufferIndex].completed ? "complete" : "not complete");

	traceDependencyEdge(commitID, pendingRequests[bufferIndex].addr, false);
}

void
CriticalPathTable::commitPeriodStarted(){
    DPRINTF(CPLTable, " %s: RESUME, commit period started, previous depth %d, stalled on address %d\n",
    		moe->name(),
    		pendingCommit.depth,
    		stalledOnAddr);

    // Identify if this is a shared or a private stall
    int causedStallIndex = -1;
    for(int i=0;i<pendingRequests.size();i++){
    	if(pendingRequests[i].valid && pendingRequests[i].addr == stalledOnAddr){
    		if(pendingRequests[i].isShared){
    			DPRINTF(CPLTable, " %s: Address %d (index %d) is a shared request, concluding shared stall\n",
    					moe->name(),
    					pendingRequests[i].addr,
    					i);

    			assert(causedStallIndex == -1);
    			assert(pendingRequests[i].completed);
    			causedStallIndex = i;
    		}
    		else{
    			DPRINTF(CPLTable, " %s: Address %d (index %d) is a private request, concluding private stall\n",
    			    					moe->name(),
    			    					pendingRequests[i].addr,
    			    					i);
    		}
    	}
    }

    if(causedStallIndex != -1){
    	assert(pendingRequests[causedStallIndex].valid);

    	// 1. Process the completed commit node
    	prevCommitDepth = pendingCommit.depth;

    	// 1.1 Update children of resolved commit node (that completed before the stall)
    	for(int i=0;i<pendingCommit.children.size();i++){
    		updateChildRequest(pendingCommit.children[i], pendingCommit.depth, pendingCommit.id);
    	}

    	// 2. Process the new commit node
    	DPRINTF(CPLTable, " %s: shared request stall, updating last completed commit %d (duration %d to %d), pending commit start at %d\n",
    			moe->name(),
    			pendingCommit.id,
    			pendingCommit.startedAt,
    			stalledAt,
    			curTick);

    	pendingCommit.reset();
    	pendingCommit.startedAt = curTick;
    	pendingCommit.id = commitIDCounter++;

    	// 2.1 Handle the request that committed last (and cleared the stall)
    	assert(pendingRequests[causedStallIndex].depth != -1);
    	assert(pendingCommit.depth == -1);

    	DPRINTF(CPLTable, " %s: Initializing pending commit %d depth to depth of last completed request %d (address %d)\n",
    			moe->name(),
    			pendingCommit.id,
    			pendingRequests[causedStallIndex].depth,
    			pendingRequests[causedStallIndex].addr);

    	pendingCommit.depth = pendingRequests[causedStallIndex].depth;

    	// 2.2 Handle other requests that completed while we where stalled (and which this commit node is the child of)
    	//     We now know that this is a shared stall, and deferred requests can be handled like any other request
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
    				traceDependencyEdge(pendingRequests[i].addr, pendingCommit.id, true);
    			}

    			pendingRequests[i].valid = false;
    			DPRINTF(CPLTable, " %s: Invalidating complete request %d (index %d, depth %d)\n",
    					moe->name(),
    					pendingRequests[i].addr,
    					i,
    					pendingRequests[i].depth);
    		}
    	}

    	// 2.3 Update the global depth counter if necessary
    	updateCommitDepthCounter(pendingCommit.depth);
    }
    else{
    	DPRINTF(CPLTable, " %s: private request stall\n", moe->name());

    	// Check for deferred requests and update the pending commit accordingly
    	for(int i=0;i<pendingRequests.size();i++){
    		if(pendingRequests[i].valid
    		   && pendingRequests[i].completed
    		   && pendingRequests[i].isShared
    		   && pendingRequests[i].deferred){

    			handleCompletedRequestWhileCommitting(i);
    		}
    	}
    }

    assert(stalledOnAddr != MemReq::inval_addr);
    stalledOnAddr = MemReq::inval_addr;

    assert(stalledAt != 0);
    stalledAt = 0;
}

void
CriticalPathTable::commitPeriodEnded(Addr stalledOn){

    DPRINTF(CPLTable, " %s: STALL, commit period %d ended, current commit depth %d, stalled on %d\n",
            moe->name(),
            pendingCommit.id,
            pendingCommit.depth,
            stalledOn);

    assert(stalledOnAddr == MemReq::inval_addr);
    stalledOnAddr = stalledOn;

    assert(stalledAt == 0);
    stalledAt = curTick;

    assert(pendingCommit.stalledAt == 0);
}

bool
CriticalPathTable::isStalled(){
	return stalledOnAddr != MemReq::inval_addr;
}

int
CriticalPathTable::getCriticalPathLength(int nextSampleID){

	DPRINTF(CPLTable, "%s: Returning current commit depth %d, resetting commit depth\n",
			moe->name(),
			curCommitDepth);

    for(int i=0;i<pendingRequests.size();i++){
        if(pendingRequests[i].valid){

            DPRINTF(CPLTable, "%s: Resetting depth for request in buffer %d (depth %d), resetting commit depth\n",
                    moe->name(),
                    i,
                    pendingRequests[i].depth);

            pendingRequests[i].depth = -1;
            if(!pendingCommit.hasChild(i)){
                DPRINTF(CPLTable, "%s: Resetting depth for request in buffer %d, was the child of an older request, setting as child of current\n",
                                    moe->name(),
                                    i);
                pendingCommit.children.push_back(i);
            }
        }
    }
    pendingCommit.depth = 0;
    prevCommitDepth = 0;

    int tmpCommitDepth = curCommitDepth;
    curCommitDepth = 0;

    currentSampleID = nextSampleID;

    return tmpCommitDepth;
}
void
CriticalPathTable::updateCommitDepthCounter(int newdepth){
	assert(newdepth != -1);
	if(newdepth > curCommitDepth){
		curCommitDepth = newdepth;

		DPRINTF(CPLTable, " %s: this commit is the current deepest at depth %d\n",
				moe->name(),
				curCommitDepth);
	}
}

bool
CriticalPathTable::isSharedRead(MemReqPtr& req, bool hiddenLoad){
    if(req->beenInSharedMemSys){
        if(hiddenLoad){
            DPRINTF(CPLTable, "%s: Request %d hides a load, add it\n", moe->name(), req->paddr);
            return true;
        }
        if(req->isStore){
            DPRINTF(CPLTable, "%s: Request %d is a store, skip it\n", moe->name(), req->paddr);
            return false;
        }
        DPRINTF(CPLTable, "%s: Request %d is a regular load, add it\n", moe->name(), req->paddr);
        return true;
    }
    DPRINTF(CPLTable, "%s: Request %d is private, skip it\n", moe->name(), req->paddr);
    return false;
}

void
CriticalPathTable::initDependencyEdgeTrace(){
	if(traceSampleID != -1){
		CPTDependencyEdgeTrace = RequestTrace(name(), "CPTEdgeTrace", 1);

		vector<string> headers;
		headers.push_back("From Address");
		headers.push_back("To Address");
		headers.push_back("From Request?");

		CPTDependencyEdgeTrace.initalizeTrace(headers);
	}
}

void
CriticalPathTable::traceDependencyEdge(Addr from, Addr to, bool fromIsRequest){
	if(traceSampleID == currentSampleID){
		vector<RequestTraceEntry> data;

		data.push_back(from);
		data.push_back(to);
		data.push_back(fromIsRequest ? 1 : 0);

		CPTDependencyEdgeTrace.addTrace(data);
	}
}
