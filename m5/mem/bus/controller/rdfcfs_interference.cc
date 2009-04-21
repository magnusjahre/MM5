
#include "rdfcfs_interference.hh"
#include "base/trace.hh"

#include <iostream>
#include <limits>

#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)

//#define DO_ESTIMATION_TRACE

using namespace std;

RDFCFSControllerInterference::RDFCFSControllerInterference(const string& _name,
											   TimingMemoryController* _ctlr,
											   int _rflimitAllCPUs,
											   bool _doOOOInsert,
											   int _cpu_count)
: ControllerInterference(_name, _cpu_count, _ctlr){

	privStorageInited = false;
	initialized = false;

	doOutOfOrderInsert = _doOOOInsert;

    rfLimitAllCPUs = _rflimitAllCPUs;
    if(rfLimitAllCPUs < 1){
        fatal("RF-limit must be at least 1, 1 gives private FCFS scheduling");
    }
}

void
RDFCFSControllerInterference::initialize(int cpu_count){

	initialized = true;

    headPointers.resize(cpu_count, NULL);
    tailPointers.resize(cpu_count, NULL);
    readyFirstLimits.resize(cpu_count, rfLimitAllCPUs);
    privateLatencyBuffer.resize(cpu_count, vector<PrivateLatencyBufferEntry*>());

    currentPrivateSeqNum.resize(cpu_count,0);

#ifdef DO_ESTIMATION_TRACE

    privateExecutionOrderTraces.resize(cpu_count, RequestTrace());
    for(int i=0;i<cpu_count;i++){
        stringstream filename;
        filename << "_private_execution_order_" << i;
        privateExecutionOrderTraces[i] = RequestTrace(name(), filename.str().c_str());

        vector<string> privorderparams;
        privorderparams.push_back("Address");
        privorderparams.push_back("Bank");
        privorderparams.push_back("Command");

        privateExecutionOrderTraces[i].initalizeTrace(privorderparams);
    }

    privateArrivalOrderEstimationTraces.resize(cpu_count, RequestTrace());
    for(int i=0;i<cpu_count;i++){
        stringstream filename;
        filename << "_private_estimated_arrival_order_" << i;
        privateArrivalOrderEstimationTraces[i] = RequestTrace(name(), filename.str().c_str());

        vector<string> privorderparams;
        privorderparams.push_back("Address");
        privorderparams.push_back("Bank");
        privorderparams.push_back("Command");
        privorderparams.push_back("Arrived At");
        privorderparams.push_back("Est Arrival");

        privateArrivalOrderEstimationTraces[i].initalizeTrace(privorderparams);
    }
#endif
}

void
RDFCFSControllerInterference::insertRequest(MemReqPtr& req){

	assert(req->interferenceMissAt == 0);

	if(!privStorageInited){
		privStorageInited = true;
		initializeMemDepStructures(memoryController->getMemoryInterface()->getMemoryBankCount());
	}

	DPRINTF(MemoryControllerInterference, "Recieved request from CPU %d, addr %d, cmd %s\n",
			req->adaptiveMHASenderID,
			req->paddr,
			req->cmd.toString());
	assert(req->adaptiveMHASenderID != -1);

	PrivateLatencyBufferEntry* newEntry = new PrivateLatencyBufferEntry(req);

	if(doOutOfOrderInsert){
		insertRequestOutOfOrder(req, newEntry);
	}
	else{

		privateLatencyBuffer[req->adaptiveMHASenderID].push_back(newEntry);

		if(headPointers[req->adaptiveMHASenderID] == NULL){
			headPointers[req->adaptiveMHASenderID] = newEntry;
			newEntry->headAtEntry = newEntry;
			DPRINTF(MemoryControllerInterference, "Updating head pointer is null, now pointing to %d\n", newEntry);
		}
		else{
			newEntry->headAtEntry = headPointers[req->adaptiveMHASenderID];

			DPRINTF(MemoryControllerInterference, "Setting head at entry for req %d to %d\n",
					newEntry->req->paddr,
					headPointers[req->adaptiveMHASenderID]->req->paddr);
		}
	}

}


void
RDFCFSControllerInterference::insertRequestOutOfOrder(MemReqPtr& req, PrivateLatencyBufferEntry* newEntry){
	bool inserted = false;
	Tick estimatedArrival = getEstimatedArrivalTime(req);

	vector<PrivateLatencyBufferEntry*>::iterator entryIt = privateLatencyBuffer[req->adaptiveMHASenderID].begin();
	for( ; entryIt != privateLatencyBuffer[req->adaptiveMHASenderID].end(); entryIt++){
		PrivateLatencyBufferEntry* curEntry = *entryIt;

		Tick curEntryEstimatedArrival = getEstimatedArrivalTime(curEntry->req);

		if(!curEntry->scheduled && estimatedArrival < curEntryEstimatedArrival){
			DPRINTF(MemoryControllerInterference, "Entry is delayed writeback %d with seq num %d, inserting before %d, seq num %d\n",
					newEntry->req->paddr,
					newEntry->req->memCtrlGeneratingReadSeqNum,
					curEntry->req->paddr,
					curEntry->req->memCtrlGeneratingReadSeqNum);
			privateLatencyBuffer[req->adaptiveMHASenderID].insert(entryIt, newEntry);
			inserted = true;
			break;
		}
	}

	if(!inserted){
		DPRINTF(MemoryControllerInterference,
				"Addr %d, order %d is the last of the currently seen requests with order information\n",
				newEntry->req->paddr,
				newEntry->req->memCtrlGeneratingReadSeqNum);
		privateLatencyBuffer[req->adaptiveMHASenderID].push_back(newEntry);
	}


	if(headPointers[req->adaptiveMHASenderID] == NULL){
		headPointers[req->adaptiveMHASenderID] = newEntry;
		newEntry->headAtEntry = newEntry;
		DPRINTF(MemoryControllerInterference, "Updating head pointer is null, now pointing to %d\n", newEntry);
	}
	else{
		DPRINTF(MemoryControllerInterference,
				"Head pointer is set, head at entry for addr %d is addr %d, entry addr %d\n",
				req->paddr,
				(*headPointers[req->adaptiveMHASenderID]).req->paddr,
				headPointers[req->adaptiveMHASenderID]);

		int headPos = getArrivalIndex(headPointers[req->adaptiveMHASenderID], req->adaptiveMHASenderID);
		int newPos = getArrivalIndex(newEntry, newEntry->req->adaptiveMHASenderID);

		assert(headPos != newPos);
		if(headPos < newPos){
			newEntry->headAtEntry = headPointers[req->adaptiveMHASenderID];
		}
		else{
			if(headPointers[req->adaptiveMHASenderID] == headPointers[req->adaptiveMHASenderID]->headAtEntry){
				headPointers[req->adaptiveMHASenderID]->headAtEntry = newEntry;
				DPRINTF(MemoryControllerInterference, "Setting head at entry pointer for addr %d to %d due to insertion before head\n",
						headPointers[req->adaptiveMHASenderID]->req->paddr,
						newEntry->req->paddr);
			}

			headPointers[req->adaptiveMHASenderID] = newEntry;
			newEntry->headAtEntry = newEntry;
		}
	}

}

void
RDFCFSControllerInterference::estimatePrivateLatency(MemReqPtr& req){

	assert(req->interferenceMissAt == 0);

    int fromCPU = req->adaptiveMHASenderID;
    assert(fromCPU != -1);
    assert(req->cmd == Read || req->cmd == Writeback);

    // check for corrupt request pointers
    for(int i=0;i<privateLatencyBuffer.size();i++){
        for(int j=0;j<privateLatencyBuffer[i].size();j++){
            assert(privateLatencyBuffer[i][j] != NULL);
            assert(privateLatencyBuffer[i][j]->req);
            assert(privateLatencyBuffer[i][j]->req->paddr != MemReq::inval_addr);
        }
    }

    // 1. Service requests ready-first within RF-limit until the current request is serviced
    DPRINTF(MemoryControllerInterference, "--- Step 1, scheduling request(s) for CPU %d, cur addr %d\n", fromCPU, req->paddr);

    PrivateLatencyBufferEntry* curLBE = NULL;

    for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
        if(privateLatencyBuffer[fromCPU][i]->req == req && privateLatencyBuffer[fromCPU][i]->scheduled){
            assert(curLBE == NULL);
            curLBE = privateLatencyBuffer[fromCPU][i];
            DPRINTF(MemoryControllerInterference, "Request for addr %d allready scheduled, head for req is addr %d\n", curLBE->req->paddr, curLBE->headAtEntry->req->paddr);
        }
    }

    if(curLBE == NULL){
        curLBE = schedulePrivateRequest(fromCPU);
        while(curLBE->req != req){
            curLBE = schedulePrivateRequest(fromCPU);
        }
    }

    curLBE->latencyRetrieved = true;

    // 2. compute queue delay by traversing history
    DPRINTF(MemoryControllerInterference, "--- Step 2, traverse history\n");

    // 2.1 find the oldest scheduled request that was in the queue when the current request arrived
    PrivateLatencyBufferEntry* headPtr = curLBE->headAtEntry;
    int headIndex = -1;
    int curIndex = -1;
    int oldestPosition = -1;
    int oldestArrivalID = -1;

    bool searching = false;
    for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
        if(privateLatencyBuffer[fromCPU][i] == headPtr){
            DPRINTF(MemoryControllerInterference, "Found head addr %d at buffer index %d\n",
                    headPtr->req->paddr,
                    i);
            headIndex = i;
            searching = true;
        }

        if(searching && privateLatencyBuffer[fromCPU][i]->scheduled){
            int tmpPosition = 0;
            PrivateLatencyBufferEntry* searchPtr = tailPointers[fromCPU];
            while(searchPtr != privateLatencyBuffer[fromCPU][i]){
                assert(searchPtr->scheduled);
                tmpPosition++;
                searchPtr = searchPtr->previous;
                assert(searchPtr != NULL);
            }

            if(tmpPosition > oldestPosition){
                oldestPosition = tmpPosition;
                oldestArrivalID = i;

                DPRINTF(MemoryControllerInterference, "Updating oldest position to %d and oldest id to %d, oldest element between head %d and current element %d is now %d\n",
                        oldestPosition,
                        oldestArrivalID,
                        headPtr->req->paddr,
                        curLBE->req->paddr,
                        privateLatencyBuffer[fromCPU][i]->req->paddr);
            }
        }

        if(privateLatencyBuffer[fromCPU][i] == curLBE){
            curIndex = i;
            searching = false;
            DPRINTF(MemoryControllerInterference, "Current entry found at position %d, search finished\n", i);
        }
    }

    assert(oldestPosition >= 0);
    assert(oldestArrivalID >= 0);

    DPRINTF(MemoryControllerInterference, "Search finished, oldest position is %d and oldest id is %d\n",
            oldestPosition,
            oldestArrivalID);

    assert(headIndex != -1);
    assert(curIndex != -1);
    assert(curIndex >= headIndex);

    // 2.2 Compute the queue latency
    Tick queueLatency = 0;
    PrivateLatencyBufferEntry* latSearchPtr = privateLatencyBuffer[fromCPU][oldestArrivalID];
    while(latSearchPtr != curLBE){
        assert(latSearchPtr->scheduled);
        assert(latSearchPtr->req);

        DPRINTF(MemoryControllerInterference, "Retrieving latency %d from addr %d, next buffer entry addr is %d\n",
                latSearchPtr->req->busAloneServiceEstimate,
                latSearchPtr->req->paddr,
                latSearchPtr->next);

        queueLatency += latSearchPtr->req->busAloneServiceEstimate;
        latSearchPtr = latSearchPtr->next;
        assert(latSearchPtr != NULL);
    }

    req->busAloneWriteQueueEstimate = 0;
    req->busAloneReadQueueEstimate = queueLatency;

    DPRINTF(MemoryControllerInterference, "History traversal finished, estimated %d cycles of queue latency\n", queueLatency);

#ifdef DO_ESTIMATION_TRACE
    for(int i=0;i<privateLatencyBuffer[req->adaptiveMHASenderID].size();i++){
    	PrivateLatencyBufferEntry* curTraceCandidate = privateLatencyBuffer[req->adaptiveMHASenderID][i];

    	if(!curTraceCandidate->scheduled) break;

    	if(!curTraceCandidate->inAccessTrace){

    		curTraceCandidate->inAccessTrace = true;

    		assert(privateExecutionOrderTraces[req->adaptiveMHASenderID].isInitialized());
    		vector<RequestTraceEntry> privorderparams;
    		privorderparams.push_back(RequestTraceEntry(curTraceCandidate->req->paddr));
    		privorderparams.push_back(RequestTraceEntry(memoryController->getMemoryBankID(curTraceCandidate->req->paddr)));
    		privorderparams.push_back(RequestTraceEntry(curTraceCandidate->req->cmd.toString()));
    		privateExecutionOrderTraces[curTraceCandidate->req->adaptiveMHASenderID].addTrace(privorderparams);
    	}
    }
#endif

    // 3. Delete any requests that are no longer needed
    // i.e. are older than the oldest head element still needed
    DPRINTF(MemoryControllerInterference, "--- Step 3, delete old entries\n");

    int oldestNeededHeadIndex = INT_MAX;

    for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
        if(!privateLatencyBuffer[fromCPU][i]->canDelete()
           || privateLatencyBuffer[fromCPU][i] == tailPointers[fromCPU]
           || privateLatencyBuffer[fromCPU][i] == headPointers[fromCPU]){

            int headIndex = getArrivalIndex(privateLatencyBuffer[fromCPU][i]->headAtEntry, fromCPU);
            DPRINTF(MemoryControllerInterference,
                    "Queue element %d addr %d cannot be deleted, head index is %d addr %d\n",
                    i,
                    privateLatencyBuffer[fromCPU][i]->req->paddr,
                    headIndex,
                    privateLatencyBuffer[fromCPU][headIndex]->req->paddr);

            if(headIndex < oldestNeededHeadIndex){
                oldestNeededHeadIndex = headIndex;
                DPRINTF(MemoryControllerInterference, "Updating oldest needed head index to %d\n", headIndex);
            }
        }
    }

	assert(oldestNeededHeadIndex != INT_MAX);

    DPRINTF(MemoryControllerInterference, "Setting oldest head index to %d, addr %d\n",
    		oldestNeededHeadIndex,
    		privateLatencyBuffer[fromCPU][oldestNeededHeadIndex]->req->paddr);

    if(oldestNeededHeadIndex > 0 && oldestNeededHeadIndex < privateLatencyBuffer[fromCPU].size()){

#ifdef DO_ESTIMATION_TRACE
    	for(int i=0;i<oldestNeededHeadIndex;i++){
			assert(privateArrivalOrderEstimationTraces[fromCPU].isInitialized());

			PrivateLatencyBufferEntry* traceEntry = privateLatencyBuffer[fromCPU][i];

			vector<RequestTraceEntry> privorderparams;
			privorderparams.push_back(RequestTraceEntry(traceEntry->req->paddr));
			privorderparams.push_back(RequestTraceEntry(memoryController->getMemoryBankID(traceEntry->req->paddr)));
			privorderparams.push_back(RequestTraceEntry(traceEntry->req->cmd.toString()));
			privorderparams.push_back(RequestTraceEntry(traceEntry->req->inserted_into_memory_controller));
			privorderparams.push_back(RequestTraceEntry(getEstimatedArrivalTime(traceEntry->req)));

			privateArrivalOrderEstimationTraces[fromCPU].addTrace(privorderparams);
    	}
#endif

        deleteBufferRange(oldestNeededHeadIndex, fromCPU);
    }
}

void
RDFCFSControllerInterference::dumpBufferStatus(int CPUID){

    cout << "\nDumping buffer contents for CPU " << CPUID << " at " << curTick << "\n";

    for(int i=0;i<privateLatencyBuffer[CPUID].size();i++){
        PrivateLatencyBufferEntry* curEntry = privateLatencyBuffer[CPUID][i];
        cout << i << ": ";
        if(curEntry->scheduled){
            cout << "Scheduled " << (curEntry->canDelete() ? "can delete" : "no del") << " " << curEntry->req->paddr << " "
            << (curEntry->headAtEntry == NULL ? 0 : curEntry->headAtEntry->req->paddr) << " <- ";
        }
        else{
            cout << "Not scheduled " << (curEntry->canDelete() ? "can delete" : "no del") << " " << curEntry->req->paddr << " "
            << (curEntry->headAtEntry == NULL ? 0 : curEntry->headAtEntry->req->paddr) << " " ;
        }

        int pos = 0;
        PrivateLatencyBufferEntry* tmp = curEntry->previous;
        while(tmp != NULL){
            cout << "(" << pos << ", " << tmp->req->paddr << ", "
                 << (tmp->headAtEntry == NULL ? 0 : tmp->headAtEntry->req->paddr) << ") <- ";
            tmp = tmp->previous;
            pos++;
        }

        cout << "\n";
    }
    cout << "\n";
}

void
RDFCFSControllerInterference::deleteBufferRange(int toIndex, int fromCPU){

    assert(toIndex > 0);
    assert(toIndex < privateLatencyBuffer[fromCPU].size());

    DPRINTF(MemoryControllerInterference, "Deleting elements from index 0 to %d\n", toIndex-1);

    for(int i=0;i<toIndex;i++){
        for(int j=toIndex;j<privateLatencyBuffer[fromCPU].size();j++){
            if(privateLatencyBuffer[fromCPU][i]->next == privateLatencyBuffer[fromCPU][j]){
                // found border between entries to delete and entries not deleted
                assert(privateLatencyBuffer[fromCPU][j]->previous == privateLatencyBuffer[fromCPU][i]);

                DPRINTF(MemoryControllerInterference,
                        "Deleted element %d at pos %d points to non-del element %d, pos %d, nulling previous pointer\n",
                        privateLatencyBuffer[fromCPU][i]->req->paddr,
                        i,
                        privateLatencyBuffer[fromCPU][j]->req->paddr,
                        j);

                privateLatencyBuffer[fromCPU][j]->previous = NULL;
            }

            if(privateLatencyBuffer[fromCPU][j]->next == privateLatencyBuffer[fromCPU][i]){

                assert(privateLatencyBuffer[fromCPU][i]->previous == privateLatencyBuffer[fromCPU][j]);

                DPRINTF(MemoryControllerInterference,
                        "Non-del element %d at pos %d points to deleted element %d, pos %d, nulling next pointer\n",
                        privateLatencyBuffer[fromCPU][j]->req->paddr,
                        j,
                        privateLatencyBuffer[fromCPU][i]->req->paddr,
                        i);

                privateLatencyBuffer[fromCPU][j]->next = NULL;
            }
        }
    }

    list<PrivateLatencyBufferEntry*> deletedPtrs;

    for(int i=0;i<toIndex;i++){

        DPRINTF(MemoryControllerInterference,
                "Deleting element at position %d address %d\n",
                i,
                privateLatencyBuffer[fromCPU].front()->req->paddr);

        PrivateLatencyBufferEntry* delLBE = privateLatencyBuffer[fromCPU].front();
        deletedPtrs.push_back(delLBE);
        privateLatencyBuffer[fromCPU].erase(privateLatencyBuffer[fromCPU].begin());

        delLBE->next = NULL;
        delLBE->previous = NULL;
        delete delLBE;
    }

    while(!deletedPtrs.empty()){
        for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
            if(privateLatencyBuffer[fromCPU][i]->headAtEntry == deletedPtrs.front()){
                assert(privateLatencyBuffer[fromCPU][i]->canDelete());
                privateLatencyBuffer[fromCPU][i]->headAtEntry = NULL;
                DPRINTF(MemoryControllerInterference,
                        "Head at entry for scheduled element address %d is deleted, nulling head at entry pointer\n",
                        privateLatencyBuffer[fromCPU][i]->req->paddr);
            }
        }
        deletedPtrs.pop_front();
    }

    // delete unreachable elements (if any)
    vector<bool> reachable = vector<bool>(privateLatencyBuffer[fromCPU].size(),false);
    PrivateLatencyBufferEntry* searchPtr = tailPointers[fromCPU];
    while(searchPtr != NULL){
        for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
            if(searchPtr == privateLatencyBuffer[fromCPU][i]){
                assert(!reachable[i]);
                reachable[i] = true;
            }
        }
        searchPtr = searchPtr->previous;
    }
    for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
        if(!privateLatencyBuffer[fromCPU][i]->scheduled){
            reachable[i] = true;
        }
    }

    vector<PrivateLatencyBufferEntry*>::iterator it = privateLatencyBuffer[fromCPU].begin();
    int pos = 0;
    while(it != privateLatencyBuffer[fromCPU].end()){
        if(!reachable[pos]){

            PrivateLatencyBufferEntry* delLBE = *it;

            DPRINTF(MemoryControllerInterference,
                    "Deleting unreachable element address %d\n",
                    delLBE->req->paddr);

            assert(delLBE->canDelete());

            it = privateLatencyBuffer[fromCPU].erase(it);

            delLBE->next = NULL;
            delLBE->previous = NULL;
            delete delLBE;
        }
        else{
            it++;
        }
        pos++;
    }
}

int
RDFCFSControllerInterference::getArrivalIndex(PrivateLatencyBufferEntry* entry, int fromCPU){
    for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
        if(entry == privateLatencyBuffer[fromCPU][i]){
            return i;
        }
    }
    fatal("entry not found");
    return -1;
}

int
RDFCFSControllerInterference::getQueuePosition(PrivateLatencyBufferEntry* entry, int fromCPU){

	int pos = 0;
	PrivateLatencyBufferEntry* searchPtr = tailPointers[fromCPU];
	while(entry != searchPtr){
		pos++;
		searchPtr = searchPtr->previous;
		assert(searchPtr != NULL);
	}
	return pos;
}

RDFCFSControllerInterference::PrivateLatencyBufferEntry*
RDFCFSControllerInterference::schedulePrivateRequest(int fromCPU){

    int headPos = -1;
    for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
        if(privateLatencyBuffer[fromCPU][i] == headPointers[fromCPU]){
            assert(headPos == -1);
            headPos = i;
        }
    }
    assert(headPos != -1);

    int limit = headPos + readyFirstLimits[fromCPU] < privateLatencyBuffer[fromCPU].size() ?
                headPos + readyFirstLimits[fromCPU] :
                privateLatencyBuffer[fromCPU].size();

    PrivateLatencyBufferEntry* curEntry = NULL;
    DPRINTF(MemoryControllerInterference, "Searching for ready first requests from position %d to %d\n",headPos,limit);
    for(int i=headPos;i<limit;i++){
        curEntry = privateLatencyBuffer[fromCPU][i];
        if(!curEntry->scheduled && isPageHitOnPrivateSystem(curEntry->req)){
            DPRINTF(MemoryControllerInterference, "Scheduling ready first request for addr %d, pos %d\n", curEntry->req->paddr, i);
            executePrivateRequest(curEntry, fromCPU, headPos);
            return curEntry;
        }
    }

    curEntry = headPointers[fromCPU];
    DPRINTF(MemoryControllerInterference, "Scheduling oldest request for addr %d\n", curEntry->req->paddr);
    assert(!curEntry->scheduled);
    executePrivateRequest(curEntry, fromCPU, headPos);
    return curEntry;
}

void
RDFCFSControllerInterference::executePrivateRequest(PrivateLatencyBufferEntry* entry, int fromCPU, int headPos){
    estimatePageResult(entry->req);

    assert(!entry->scheduled);
    entry->scheduled = true;

    entry->req->busDelay = 0;
    Tick privateLatencyEstimate = 0;
    if(entry->req->privateResultEstimate == DRAM_RESULT_HIT){
    	estimatedNumberOfHits[fromCPU]++;
    	if(entry->req->cmd == Read) privateLatencyEstimate = 40;
    	else privateLatencyEstimate = 30;
    }
    else if(entry->req->privateResultEstimate == DRAM_RESULT_CONFLICT){
    	estimatedNumberOfConflicts[fromCPU]++;
        if(entry->req->cmd == Read) privateLatencyEstimate = 218;
        else privateLatencyEstimate = 186;
    }
    else{
    	assert(entry->req->privateResultEstimate == DRAM_RESULT_MISS);
    	estimatedNumberOfMisses[fromCPU]++;
        if(entry->req->cmd == Read) privateLatencyEstimate = 120;
        else privateLatencyEstimate = 110;
    }

    assert(entry->req->cmd == Read || entry->req->cmd == Writeback);
    if(entry->req->cmd == Read){
    	entry->req->memCtrlPrivateSeqNum = currentPrivateSeqNum[entry->req->adaptiveMHASenderID];
    	currentPrivateSeqNum[entry->req->adaptiveMHASenderID]++;
    }

    DPRINTF(MemoryControllerInterference, "Estimated service latency for addr %d is %d\n", entry->req->paddr, privateLatencyEstimate);

    entry->req->busAloneServiceEstimate += privateLatencyEstimate;

    if(tailPointers[fromCPU] != NULL){
        tailPointers[fromCPU]->next = entry;
        entry->previous = tailPointers[fromCPU];
    }
    DPRINTF(MemoryControllerInterference,
            "Updating tail pointer, previous was %d, changing to %d, new tail addr is %d, new tail is scheduled behind %d, old tail scheduled before %d\n",
            tailPointers[fromCPU], entry, entry->req->paddr,
            (entry->previous == NULL ? 0 : entry->previous->req->paddr),
            (tailPointers[fromCPU] == NULL ? 0 :
                    (tailPointers[fromCPU]->next == NULL ? 0 :
                    tailPointers[fromCPU]->next->req->paddr)));
    tailPointers[fromCPU] = entry;

    updateHeadPointer(entry, headPos, fromCPU);

    DPRINTF(MemoryControllerInterference, "Request %d (%d) beforepointer %d (%d), behindpointer %d (%d)\n",
	    entry,
	    entry->req->paddr,
	    entry->next,
	    (entry->next == NULL ? 0 : entry->next->req->paddr),
	    entry->previous,
	    (entry->previous == NULL ? 0 : entry->previous->req->paddr) );

    if(entry->headAtEntry != entry){
        assert( !(entry->previous == NULL && entry->next == NULL) );
    }

}

void
RDFCFSControllerInterference::updateHeadPointer(PrivateLatencyBufferEntry* entry, int headPos, int fromCPU){
    int newHeadPos = headPos+1;
    if(newHeadPos < privateLatencyBuffer[fromCPU].size()){
        if(headPointers[fromCPU] == entry){

            while(newHeadPos < privateLatencyBuffer[fromCPU].size()){
                if(!privateLatencyBuffer[fromCPU][newHeadPos]->scheduled){
                    DPRINTF(MemoryControllerInterference,
                            "Updating head pointer, previous was %d (%d), changing to %d, new head addr is %d\n",
                            headPointers[fromCPU],
                            (headPointers[fromCPU] == NULL ? 0 : headPointers[fromCPU]->req->paddr),
                             privateLatencyBuffer[fromCPU][newHeadPos],
                             privateLatencyBuffer[fromCPU][newHeadPos]->req->paddr);
                    headPointers[fromCPU] = privateLatencyBuffer[fromCPU][newHeadPos];
                    return;
                }
                newHeadPos++;
            }
        }
        else{
            DPRINTF(MemoryControllerInterference, "Issued request was not head, no change of head pointer needed\n");
            return;
        }
    }

    DPRINTF(MemoryControllerInterference, "No more queued requests, nulling head pointer\n");
    headPointers[fromCPU] = NULL;
}

Tick
RDFCFSControllerInterference::getEstimatedArrivalTime(MemReqPtr& req){

	assert(req->cmd == Read || req->cmd == Writeback);

	if(req->cmd == Read){
		return req->inserted_into_memory_controller -
		(req->interferenceBreakdown[INTERCONNECT_ENTRY_LAT] +
		 req->interferenceBreakdown[INTERCONNECT_TRANSFER_LAT] +
		 req->interferenceBreakdown[INTERCONNECT_DELIVERY_LAT]+
		 req->interferenceBreakdown[MEM_BUS_ENTRY_LAT]);
	}

	return req->inserted_into_memory_controller - req->memCtrlGenReadInterference;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(RDFCFSControllerInterference)
	SimObjectParam<TimingMemoryController*> memory_controller;
    Param<int> rf_limit_all_cpus;
    Param<bool> do_ooo_insert;
    Param<int> cpu_count;
END_DECLARE_SIM_OBJECT_PARAMS(RDFCFSControllerInterference)

BEGIN_INIT_SIM_OBJECT_PARAMS(RDFCFSControllerInterference)
	INIT_PARAM_DFLT(memory_controller, "Associated memory controller", NULL),
    INIT_PARAM_DFLT(rf_limit_all_cpus, "Private latency estimation ready first limit", 5),
    INIT_PARAM_DFLT(do_ooo_insert, "If true, a reordering step is applied to the recieved requests (experimental)", false),
    INIT_PARAM_DFLT(cpu_count, "number of cpus",-1)
END_INIT_SIM_OBJECT_PARAMS(RDFCFSControllerInterference)

CREATE_SIM_OBJECT(RDFCFSControllerInterference)
{
    return new RDFCFSControllerInterference(getInstanceName(),
									  memory_controller,
									  rf_limit_all_cpus,
									  do_ooo_insert,
									  cpu_count);
}

REGISTER_SIM_OBJECT("RDFCFSControllerInterference", RDFCFSControllerInterference)

#endif



