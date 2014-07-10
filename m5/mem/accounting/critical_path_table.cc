/*
 * critical_path_table.cc
 *
 *  Created on: Jul 18, 2013
 *      Author: jahre
 */

#include "critical_path_table.hh"

using namespace std;

CriticalPathTable::CriticalPathTable(MemoryOverlapEstimator* _moe, int _bufferSize){
    moe = _moe;

    newestValidPtr = 0;
    oldestValidPtr = 0;

    stalledOnAddr = MemReq::inval_addr;
    stalledAt = 0;

    commitIDCounter = 0;

    pendingRequests.resize(_bufferSize, CPTRequestEntry());

    pendingCommit.depth = 0;
    prevCommitDepth = 0;

    cplMeasurements = new CriticalPathTableMeasurements();

    traceSampleID = _moe->getTraceSampleID();
    currentSampleID = 0;
    initDependencyEdgeTrace();
}

int
CriticalPathTable::findRequest(Addr paddr){
    int foundIndex = -1;

    // handle oldest element
    int i = oldestValidPtr;
    if(pendingRequests[i].addr == paddr && pendingRequests[i].valid){
    	return i;
    }
    incrementBufferPointer(&i);

    // handle subsequent elements up to and including the newest valid element
    int iterbound = newestValidPtr;
    incrementBufferPointer(&iterbound);
    while(i != iterbound){
        if(pendingRequests[i].addr == paddr && pendingRequests[i].valid){
            assert(foundIndex == -1);
            foundIndex = i;
        }
        i = (i+1) % pendingRequests.size();
    }
    // May return -1 if the request is not found
    return foundIndex;
}

void
CriticalPathTable::dumpBufferContents(){
	cout << "Buffer contents @ " << curTick << "\n";
	cout << "Oldest valid pointer: " << oldestValidPtr << "\n";
	cout << "Next valid pointer; " << newestValidPtr << "\n";
	for(int i=0;i<pendingRequests.size();i++){
		cout << "Buffer Element " << i << ": " << pendingRequests[i].addr << " " << (pendingRequests[i].valid ? "valid" : "invalid") << "\n";
	}
}

void
CriticalPathTable::handleFullBuffer(){

	assert(newestValidPtr == oldestValidPtr);

	DPRINTF(CPLTable, " %s: Invalidating %s request %d (index %d, depth %d) due to full buffer\n",
			moe->name(),
			pendingRequests[newestValidPtr].isShared ? "shared" : "private",
			pendingRequests[newestValidPtr].addr,
			newestValidPtr,
			pendingRequests[newestValidPtr].depth);

	if(pendingRequests[newestValidPtr].isShared){
		bool isChild = pendingCommit.removeChild(newestValidPtr);
		if(isChild){
			traceDependencyEdge(pendingCommit.id, pendingRequests[newestValidPtr].addr, false);
		}
		if(pendingRequests[newestValidPtr].completed){
			traceDependencyEdge(pendingRequests[newestValidPtr].addr, pendingCommit.id, true);
		}
	}

	if(pendingCommit.hasChild(newestValidPtr)){
		DPRINTF(CPLTable, " %s: Removing request %d (index %d) from the child list of the current commit\n",
				moe->name(),
				pendingRequests[newestValidPtr].addr,
				newestValidPtr);

		pendingCommit.removeChild(newestValidPtr);
	}

	pendingRequests[newestValidPtr].valid = false;
	incrementBufferPointer(&oldestValidPtr);
	incrementBufferPointerToNextValid(&oldestValidPtr);

}

void
CriticalPathTable::issuedRequest(MemReqPtr& req){
    Addr addr = req->paddr & ~(MOE_CACHE_BLK_SIZE-1);

    DPRINTF(CPLTable, " %s: Got memory request for addr %d, command %s pos %d, current commit depth %d\n",
            moe->name(),
            addr,
            req->cmd,
            newestValidPtr,
            pendingCommit.depth);

    if(findRequest(addr) != -1){
        DPRINTF(CPLTable, " %s: Addr %d already allocated, skipping\n",
                    moe->name(),
                    addr);
        return;
    }

    // If the buffer is empty, the pointers will be equal and pointing to an invalid request
    if(pendingRequests[newestValidPtr].valid){
    	incrementBufferPointer(&newestValidPtr);
    	if(newestValidPtr == oldestValidPtr){
    		assert(pendingRequests[newestValidPtr].valid);
    		handleFullBuffer();
    	}
    }

    DPRINTF(CPLTable, " %s: Newest valid pointer is now %d, oldest valid pointer %d\n",
    		moe->name(),
    		newestValidPtr,
    		oldestValidPtr);

    pendingRequests[newestValidPtr].update(addr);
    assert(pendingCommit.startedAt <= curTick);
    if(pendingCommit.startedAt == curTick){
    	DPRINTF(CPLTable, " %s: Request %d is issued in the same cycle as commit resumes, child of previous commit (depth %d)\n",
    	                    moe->name(),
    	                    addr,
    	                    prevCommitDepth);

    	updateChildRequest(newestValidPtr, prevCommitDepth, pendingCommit.id-1);
    }
    else{
    	DPRINTF(CPLTable, " %s: Request %d the child of the pending commit\n",
    			moe->name(),
    			addr);

    	pendingCommit.children.push_back(newestValidPtr);
    }
}

void
CriticalPathTable::handleCompletedRequestWhileCommitting(int pendingIndex){
	DPRINTF(CPLTable, " %s: Request for address %d completed in the last commit period, depth %d\n",
			moe->name(),
			pendingRequests[pendingIndex].addr,
			pendingCommit.depth);

	if(pendingRequests[pendingIndex].depth > pendingCommit.depth){
		pendingCommit.depth = pendingRequests[pendingIndex].depth;
		DPRINTF(CPLTable, " %s: Setting pending commit depth to %d\n",
				moe->name(),
				pendingCommit.depth);

		updateCommitDepthCounter(pendingCommit.depth, pendingIndex);
	}

	bool isChild = pendingCommit.removeChild(pendingIndex);
	if(isChild){
		traceDependencyEdge(pendingCommit.id, pendingRequests[pendingIndex].addr, false);
	}
	traceDependencyEdge(pendingRequests[pendingIndex].addr, pendingCommit.id, true);

	DPRINTF(CPLTable, " %s: Invalidating entry %d, address %d \n",
			moe->name(),
			pendingIndex,
			pendingRequests[pendingIndex].addr);

	pendingRequests[pendingIndex].valid = false;
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
    if(pendingIndex == -1){
    	DPRINTF(CPLTable, " %s: Request for address %d was already removed, returning\n",
    			moe->name(),
    			req->paddr);
    	return;
    }

    if(isSharedRead(req, hiddenLoad)){

    	pendingRequests[pendingIndex].completedAt = curTick;
    	pendingRequests[pendingIndex].completed = true;
    	pendingRequests[pendingIndex].isShared = true;
    	pendingRequests[pendingIndex].interference = req->boisInterferenceSum;

    	DPRINTF(CPLTable, " %s: Request for address %d (index %d) is shared and complete, latency %d, interference %d\n",
                        moe->name(),
                        req->paddr,
                        pendingIndex,
                        pendingRequests[pendingIndex].latency(),
                        pendingRequests[pendingIndex].interference);
    }
    else{
    	DPRINTF(CPLTable, " %s: Request for address %d (index %d) is not applicable, invalidating it, oldest valid pointer %d\n",
                moe->name(),
                req->paddr,
                pendingIndex,
                oldestValidPtr);

        pendingRequests[pendingIndex].completed = true;
        pendingRequests[pendingIndex].isShared = false;
        pendingRequests[pendingIndex].valid = false;
        pendingCommit.removeChild(pendingIndex);

        if(pendingIndex == oldestValidPtr){
        	incrementBufferPointerToNextValid(&oldestValidPtr);
        	DPRINTF(CPLTable, " %s: Request was the oldest valid, new oldest valid is %d\n",
        			moe->name(),
        			oldestValidPtr);
        }
    }
}

void
CriticalPathTable::incrementBufferPointer(int* bufferPtr){
	*bufferPtr = (*bufferPtr + 1) % pendingRequests.size();
}

void
CriticalPathTable::incrementBufferPointerToNextValid(int* bufferPtr){
	while(!pendingRequests[*bufferPtr].valid && *bufferPtr != newestValidPtr){
		DPRINTF(CPLTable, " %s: Buffer element %d is not valid, incrementing pointer\n",
				moe->name(),
				*bufferPtr);
		incrementBufferPointer(bufferPtr);
	}

	DPRINTF(CPLTable, " %s: Oldest valid pointer is %d, next valid pointer is %d\n",
			moe->name(),
			oldestValidPtr,
			newestValidPtr);
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
CriticalPathTable::updateStallState(){
    assert(stalledOnAddr != MemReq::inval_addr);
    assert(stalledAt != 0);
    stalledOnAddr = MemReq::inval_addr;
    stalledAt = 0;
}

void
CriticalPathTable::commitPeriodStarted(){
    DPRINTF(CPLTable, " %s: RESUME, commit period started, previous depth %d, stalled on address %d\n",
    		moe->name(),
    		pendingCommit.depth,
    		stalledOnAddr);

    assert(stalledOnAddr != MemReq::inval_addr);
    assert(stalledAt != 0);

    // Identify if this is a shared or a private stall
    int causedStallIndex = findRequest(stalledOnAddr);

    if(causedStallIndex == -1){
    	DPRINTF(CPLTable, " %s: Address %d not found, handling it as a private stall\n",
    			moe->name(),
    			stalledOnAddr);
    }

    if(causedStallIndex != -1 && !pendingRequests[causedStallIndex].isShared){
    	DPRINTF(CPLTable, " %s: Address %d (index %d) is a private request, concluding private stall\n",
    			moe->name(),
    			pendingRequests[causedStallIndex].addr,
    			causedStallIndex);
    }

    // Shared stall processing algorithm
    if(causedStallIndex != -1 && pendingRequests[causedStallIndex].isShared){

		DPRINTF(CPLTable, " %s: Address %d (index %d) is a shared request, concluding shared stall\n",
				moe->name(),
				pendingRequests[causedStallIndex].addr,
				causedStallIndex);

		assert(pendingRequests[causedStallIndex].completed);
    	assert(pendingRequests[causedStallIndex].valid);

    	// 1. Process the completed commit node

    	// 1.1 Update completed requests that completed before the stall
    	for(int i=0;i<pendingRequests.size();i++){
    		if(pendingRequests[i].valid
    		   && pendingRequests[i].completed
    		   && pendingRequests[i].isShared
    		   && pendingRequests[i].completedAt < stalledAt){

    			handleCompletedRequestWhileCommitting(i);
    		}
    	}
    	incrementBufferPointerToNextValid(&oldestValidPtr);

    	if(!pendingRequests[causedStallIndex].valid){
    		DPRINTF(CPLTable, " %s: The request that caused the stall was invalidated, commit period %d is still pending at depth %d\n",
    				moe->name(),
    				pendingCommit.id,
    				pendingCommit.depth);
    		updateStallState();
    		return;
    	}

    	prevCommitDepth = pendingCommit.depth;

    	// 1.2 Update children of resolved commit node (that completed before the stall)
    	for(int i=0;i<pendingCommit.children.size();i++){
    		updateChildRequest(pendingCommit.children[i], pendingCommit.depth, pendingCommit.id);
    	}

    	// 2. Process the new commit node

    	// 2.1 Initialize the new commit and handle the request that committed last (and cleared the stall)
    	DPRINTF(CPLTable, " %s: shared request stall, updating last completed commit %d (duration %d to %d), pending commit start at %d\n",
    			moe->name(),
    			pendingCommit.id,
    			pendingCommit.startedAt,
    			stalledAt,
    			curTick);


    	pendingCommit.reset();
    	pendingCommit.startedAt = curTick;
    	pendingCommit.id = commitIDCounter++;

    	assert(pendingRequests[causedStallIndex].depth != -1);
    	assert(pendingCommit.depth == -1);

    	DPRINTF(CPLTable, " %s: Initializing pending commit %d depth to depth of last completed request %d (address %d)\n",
    			moe->name(),
    			pendingCommit.id,
    			pendingRequests[causedStallIndex].depth,
    			pendingRequests[causedStallIndex].addr);

    	pendingCommit.depth = pendingRequests[causedStallIndex].depth;
    	updateCommitDepthCounter(pendingCommit.depth, causedStallIndex);

    	// 2.2 Handle other requests that completed while we where stalled (and which this commit node is the child of)
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
    					updateCommitDepthCounter(pendingCommit.depth, i);
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
    	incrementBufferPointerToNextValid(&oldestValidPtr);
    }

    updateStallState();
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

void
CriticalPathTable::addCommitCycle(){
	for(int i=0;i<pendingRequests.size();i++){
		if(pendingRequests[i].valid){
			pendingRequests[i].cwp++;
		}
	}
}

CriticalPathTableMeasurements
CriticalPathTable::getCriticalPathLength(int nextSampleID){

	DPRINTF(CPLTable, "%s: Returning current commit depth %d, resetting commit depth\n",
			moe->name(),
			cplMeasurements->criticalPathLength);

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
    currentSampleID = nextSampleID;

    CriticalPathTableMeasurements cplMeasurementCopy = *cplMeasurements;
    cplMeasurements->reset();

    return cplMeasurementCopy;
}

void
CriticalPathTable::updateCommitDepthCounter(int newdepth, int criticalPathBufferEntry){
	assert(newdepth != -1);
	if(newdepth > cplMeasurements->criticalPathLength){
		cplMeasurements->criticalPathLength = newdepth;

		DPRINTF(CPLTable, " %s: this commit is the current deepest at depth %d\n",
				moe->name(),
				cplMeasurements->criticalPathLength);

		assert(pendingRequests[criticalPathBufferEntry].latency() >= pendingRequests[criticalPathBufferEntry].interference);

		cplMeasurements->criticalPathLatency += pendingRequests[criticalPathBufferEntry].latency();
		cplMeasurements->criticalPathInterference += pendingRequests[criticalPathBufferEntry].interference;
		cplMeasurements->criticalPathCommitWhilePending += pendingRequests[criticalPathBufferEntry].cwp;
		cplMeasurements->criticalPathRequests++;
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
		CPTDependencyEdgeTrace = RequestTrace(name(), "CPTEdgeTrace");

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
