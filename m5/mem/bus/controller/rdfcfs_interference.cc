
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
							   int _cpu_count,
							   int _buffer_size,
							   bool _use_avg_lats,
							   bool _pure_head_ptr_model)
: ControllerInterference(_name, _cpu_count, _ctlr){

	privStorageInited = false;
	initialized = false;

	doOutOfOrderInsert = _doOOOInsert;
	useAverageLatencies = _use_avg_lats;
	usePureHeadPointerQueueModel = _pure_head_ptr_model;

	if(_buffer_size == -1){
		infiniteBuffer = true;
		bufferSize = -1;
	}
	else{
		infiniteBuffer = false;
		bufferSize = _buffer_size;
	}

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
    oldestEntryPointer.resize(cpu_count, NULL);
    readyFirstLimits.resize(cpu_count, rfLimitAllCPUs);
    privateLatencyBuffer.resize(cpu_count, vector<PrivateLatencyBufferEntry*>());

    currentPrivateSeqNum.resize(cpu_count,0);

    lastQueueDelay.resize(cpu_count, 0);
    lastServiceDelay.resize(cpu_count, 0);

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
    if(req->interferenceMissAt != 0 || req->adaptiveMHASenderID == -1) return;

	assert(req->interferenceMissAt == 0);
	numRequests[req->adaptiveMHASenderID]++;

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

	if(!infiniteBuffer && privateLatencyBuffer[req->adaptiveMHASenderID].size() >= bufferSize){
		assert(privateLatencyBuffer[req->adaptiveMHASenderID].size() == bufferSize);
		removeOldestRequest(req);
	}

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
RDFCFSControllerInterference::removeOldestRequest(MemReqPtr& req){

	assert(privateLatencyBuffer[req->adaptiveMHASenderID].size() > 2);

	droppedRequests[req->adaptiveMHASenderID]++;

	PrivateLatencyBufferEntry* oldest = privateLatencyBuffer[req->adaptiveMHASenderID][0];
	PrivateLatencyBufferEntry* newOldest = privateLatencyBuffer[req->adaptiveMHASenderID][1];

	DPRINTF(MemoryControllerInterference, "Removing oldest element %d (%d) due to lack of buffer space, new oldest is %d (%d)\n",
			oldest->req->paddr,
			oldest,
			newOldest->req->paddr,
			newOldest);

	vector<PrivateLatencyBufferEntry*>::iterator entryIt = privateLatencyBuffer[req->adaptiveMHASenderID].begin();
	for( ; entryIt != privateLatencyBuffer[req->adaptiveMHASenderID].end(); entryIt++){
		PrivateLatencyBufferEntry* curEntry = *entryIt;
		if(curEntry->headAtEntry == oldest){
			curEntry->headAtEntry = newOldest;
			DPRINTF(MemoryControllerInterference, "Element %d has deleted element %d as headAtEntry, updating pointer to %d\n",
					curEntry->req->paddr,
					oldest->req->paddr,
					newOldest->req->paddr);
		}
	}

	if(oldest->next != NULL){
		oldest->next->previous = oldest->previous;
		DPRINTF(MemoryControllerInterference, "Removing oldest element %d, the previous pointer of %d is now %d\n",
				oldest->req->paddr,
				oldest->next->req->paddr,
				(oldest->previous == NULL ? 0 : oldest->previous->req->paddr));
	}
	if(oldest->previous != NULL){
		oldest->previous->next = oldest->next;
		DPRINTF(MemoryControllerInterference, "Removing oldest element %d, the next pointer of %d is now %d\n",
				oldest->req->paddr,
				oldest->previous->req->paddr,
				(oldest->next == NULL ? 0 : oldest->next->req->paddr));
	}

	if(oldest == headPointers[req->adaptiveMHASenderID]){
		PrivateLatencyBufferEntry* newHead = NULL;

		for(int i=1;i<privateLatencyBuffer[req->adaptiveMHASenderID].size();i++){
			if(!privateLatencyBuffer[req->adaptiveMHASenderID][i]->scheduled){
				newHead = privateLatencyBuffer[req->adaptiveMHASenderID][i];
				break;
			}
		}
		assert(newHead != NULL);

		headPointers[req->adaptiveMHASenderID] = newHead;
		DPRINTF(MemoryControllerInterference, "Oldest element %d is the current head, new head is %d \n",
				oldest->req->paddr,
				newHead->req->paddr);
	}


	if(oldest == tailPointers[req->adaptiveMHASenderID]){

		tailPointers[req->adaptiveMHASenderID] = oldest->previous;

		if(oldest->previous != NULL) assert(oldest->previous->scheduled);

		DPRINTF(MemoryControllerInterference, "Oldest element %d is the current tail, new tail is %d \n",
				oldest->req->paddr,
				(oldest->previous == NULL) ? 0 : oldest->previous->req->paddr);
	}

	if(oldest == oldestEntryPointer[req->adaptiveMHASenderID]){

		oldestEntryPointer[req->adaptiveMHASenderID] = oldest->next;

		DPRINTF(MemoryControllerInterference, "Oldest element %d is the oldest entry, new oldest entry is %d \n",
				oldest->req->paddr,
			    (oldest->next == NULL) ? 0 : oldest->next->req->paddr);
	}

	privateLatencyBuffer[req->adaptiveMHASenderID].erase(privateLatencyBuffer[req->adaptiveMHASenderID].begin());
	oldest->next = NULL;
	oldest->previous = NULL;
	oldest->headAtEntry = NULL;
	delete oldest;
}

void
RDFCFSControllerInterference::insertPrivateVirtualRequest(MemReqPtr& req){

	if(!this->isInitialized()){
		initialize(contIntCpuCount);
	}

	insertRequest(req);
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
RDFCFSControllerInterference::estimatePrivateLatency(MemReqPtr& req, Tick busOccupiedFor){

    if(req->interferenceMissAt != 0 || req->adaptiveMHASenderID == -1) return;

	assert(req->interferenceMissAt == 0);

    int fromCPU = req->adaptiveMHASenderID;
    assert(fromCPU != -1);
    assert(req->cmd == Read || req->cmd == Writeback);

    // check for corrupt request pointers
    for(int i=0;i<privateLatencyBuffer.size();i++){
        for(int j=0;j<privateLatencyBuffer[i].size();j++){
            assert(privateLatencyBuffer[i][j] != NULL);
            assert(privateLatencyBuffer[i][j]->req);
        }
    }

    if(!infiniteBuffer){
    	bool found = false;
		for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
			if(privateLatencyBuffer[fromCPU][i]->req == req){
				found = true;
			}
		}

		if(!found){
			req->busAloneServiceEstimate = lastServiceDelay[req->adaptiveMHASenderID];
			req->busAloneWriteQueueEstimate = 0;
			req->busAloneReadQueueEstimate = lastQueueDelay[req->adaptiveMHASenderID];
			prematurelyDroppedRequests[req->adaptiveMHASenderID]++;

			DPRINTF(MemoryControllerInterference, "Request %d from CPU %d has been dropped from the queue, returning estimated service time %d and queue latency %d\n",
					req->paddr,
					fromCPU,
					req->busAloneServiceEstimate,
					req->busAloneReadQueueEstimate);

			return;
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

    Tick queueLatency = 0;

    if(usePureHeadPointerQueueModel){
    	PrivateLatencyBufferEntry* headPtr = curLBE->headAtEntry;
    	PrivateLatencyBufferEntry* queueSearchPtr = headPtr;


    	if(!queueSearchPtr->scheduled){
    		DPRINTF(MemoryControllerInterference, "Head for addr %d has not been executed (head is %d), setting queue latency to 0\n",
    				curLBE->req->paddr,
    				headPtr->req->paddr);
    	}
    	else{
    		while(queueSearchPtr != NULL){
    			if(queueSearchPtr == curLBE) break;
    			assert(queueSearchPtr->scheduled);
    			queueLatency += queueSearchPtr->req->busAloneServiceEstimate;
    			queueSearchPtr = queueSearchPtr->next;
    		}

    		if(queueSearchPtr == NULL){
    			DPRINTF(MemoryControllerInterference, "Request for addr %d was executed before its head %d, setting queue latency to 0\n",
    					curLBE->req->paddr,
    					headPtr->req->paddr);
    			queueLatency = 0;
    		}
    	}
    }
    else{

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
    }

    req->busAloneWriteQueueEstimate = 0;
    req->busAloneReadQueueEstimate = queueLatency;

    lastQueueDelay[req->adaptiveMHASenderID] = queueLatency;
    lastServiceDelay[req->adaptiveMHASenderID] = req->busAloneServiceEstimate;

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
	freeUsedEntries(fromCPU);
}

void
RDFCFSControllerInterference::dumpBufferStatus(int CPUID){

    cout << "\nDumping buffer contents for CPU " << CPUID << " at " << curTick << "\n";

    for(int i=0;i<privateLatencyBuffer[CPUID].size();i++){
        PrivateLatencyBufferEntry* curEntry = privateLatencyBuffer[CPUID][i];
        cout << i << ": ";
        if(curEntry->scheduled){
            cout << "Scheduled " << (curEntry->canDelete() ? "can delete" : "no del") << " " << curEntry->req->paddr << " "
            << (curEntry->headAtEntry == NULL ? 0 : curEntry->headAtEntry->req->paddr);
        }
        else{
            cout << "Not scheduled " << (curEntry->canDelete() ? "can delete" : "no del") << " " << curEntry->req->paddr << " "
            << (curEntry->headAtEntry == NULL ? 0 : curEntry->headAtEntry->req->paddr);
        }
        cout << "\n";
    }


    int pos = 0;
    cout << "\nCurrent execution order (from tail to front, youngest to oldest)\n";
	PrivateLatencyBufferEntry* curEntry = tailPointers[CPUID];
	while(curEntry != NULL){
		cout << pos << ": " << curEntry->req->paddr << "\n";
		curEntry = curEntry->previous;
		pos++;
	}
	cout << "\n";

}

void
RDFCFSControllerInterference::freeUsedEntries(int fromCPU){

	int oldestUndeletableAccessOrderIndex = -1;
	for(int i = 0; i < privateLatencyBuffer[fromCPU].size(); i++){
		DPRINTF(MemoryControllerInterference, "Checking element %d address %d\n",
				i,
				privateLatencyBuffer[fromCPU][i]->req->paddr);

		if(!privateLatencyBuffer[fromCPU][i]->canDelete()
		   || privateLatencyBuffer[fromCPU][i] == tailPointers[fromCPU]
		   || privateLatencyBuffer[fromCPU][i] == headPointers[fromCPU]){
			assert(oldestUndeletableAccessOrderIndex == -1);
			oldestUndeletableAccessOrderIndex = i;
			break;
		 }
	}

	if(oldestUndeletableAccessOrderIndex == -1){
		DPRINTF(MemoryControllerInterference, "No elements can be deleted, returning\n");
		return;
	}

	DPRINTF(MemoryControllerInterference, "Found oldest undeletable element at position %d, address %d, head is address %d, oldest pointer %d\n",
			oldestUndeletableAccessOrderIndex,
			privateLatencyBuffer[fromCPU][oldestUndeletableAccessOrderIndex]->req->paddr,
			privateLatencyBuffer[fromCPU][oldestUndeletableAccessOrderIndex]->headAtEntry->req->paddr,
			oldestEntryPointer[fromCPU]->req->paddr);

	assert(!privateLatencyBuffer[fromCPU].empty());
	PrivateLatencyBufferEntry* oldestUndeletable = privateLatencyBuffer[fromCPU][oldestUndeletableAccessOrderIndex];
	PrivateLatencyBufferEntry* headEntry = privateLatencyBuffer[fromCPU][oldestUndeletableAccessOrderIndex]->headAtEntry;

	assert(oldestEntryPointer[fromCPU] != 0);
	PrivateLatencyBufferEntry* currentEntry = oldestEntryPointer[fromCPU];
	assert(oldestEntryPointer[fromCPU] != NULL);

	while(currentEntry != headEntry && currentEntry != oldestUndeletable && currentEntry != NULL){

		if(!currentEntry->canDelete()
			   || currentEntry == tailPointers[fromCPU]
			   || currentEntry == headPointers[fromCPU]){
			DPRINTF(MemoryControllerInterference, "Element %d cannot be deleted, stopping deletions\n",
					currentEntry->req->paddr);
			break;
		}

		assert(currentEntry->canDelete());
		assert(headPointers[fromCPU] != currentEntry);
		assert(tailPointers[fromCPU] != currentEntry);

		PrivateLatencyBufferEntry* next = currentEntry->next;
		assert(next != NULL);
		currentEntry->next->previous = NULL;

		int delIndex = getArrivalIndex(currentEntry,fromCPU);
		DPRINTF(MemoryControllerInterference, "Deleting element %d, index %i, next is %d\n",
				currentEntry->req->paddr,
				delIndex,
				(currentEntry->next == 0 ? 0 :currentEntry->next->req->paddr));

		currentEntry->next = NULL;
		currentEntry->previous = NULL;
		currentEntry->headAtEntry = NULL;

		// required to avoid segfaults when debuging
		for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
			if(privateLatencyBuffer[fromCPU][i]->headAtEntry == currentEntry){
				assert(privateLatencyBuffer[fromCPU][i]->canDelete());
				privateLatencyBuffer[fromCPU][i]->headAtEntry = NULL;
			}
		}

		privateLatencyBuffer[fromCPU].erase(privateLatencyBuffer[fromCPU].begin()+delIndex);
		delete currentEntry;

		currentEntry = next;
		oldestEntryPointer[fromCPU] = next;
	}

	// Runtime test code
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
		if(privateLatencyBuffer[fromCPU][i]->scheduled && !reachable[i]){
			fatal("Element at entry position %d is no longer reachable", i);
		}
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
    assert(headPointers[fromCPU] != NULL);
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

    assert(!entry->scheduled);
    entry->scheduled = true;

    entry->req->busDelay = 0;
    Tick privateLatencyEstimate = 0;


    // 1. Update tail pointer and previous pointer (needed for latency estimation)

    if(tailPointers[fromCPU] != NULL){
    	tailPointers[fromCPU]->next = entry;
    	entry->previous = tailPointers[fromCPU];
    }

    if(oldestEntryPointer[fromCPU] == NULL){
    	DPRINTF(MemoryControllerInterference, "Initializing oldest entry pointer to addr %d\n",
				entry->req);

    	oldestEntryPointer[fromCPU] = entry;
    }


    DPRINTF(MemoryControllerInterference,
    		"Updating tail pointer, previous was %d, changing to %d, new tail addr is %d, new tail is scheduled behind %d, old tail scheduled before %d\n",
    		tailPointers[fromCPU], entry, entry->req->paddr,
    		(entry->previous == NULL ? 0 : entry->previous->req->paddr),
    		(tailPointers[fromCPU] == NULL ? 0 :
    		(tailPointers[fromCPU]->next == NULL ? 0 :
    		tailPointers[fromCPU]->next->req->paddr)));
    tailPointers[fromCPU] = entry;


    // 2. Estimate latency
    estimatePageResult(entry->req);

    if(entry->req->privateResultEstimate == DRAM_RESULT_HIT){
    	estimatedNumberOfHits[fromCPU]++;
    	privateLatencyEstimate = 40;
    }
    else if(entry->req->privateResultEstimate == DRAM_RESULT_CONFLICT){
    	estimatedNumberOfConflicts[fromCPU]++;
    	if(useAverageLatencies){
    		if(entry->req->cmd == Read) privateLatencyEstimate = 218;
    		else privateLatencyEstimate = 186;
    	}
    	else{

    		bool previousIsWrite = false;
    		if(entry->previous != NULL){
    			assert(entry->previous->req->cmd == Read
    					|| entry->previous->req->cmd == Writeback
    					|| entry->previous->req->cmd == VirtualPrivateWriteback);

    			if(entry->previous->req->cmd == Writeback
    					|| entry->previous->req->cmd == VirtualPrivateWriteback){
    				previousIsWrite = true;
    			}
    		}

    		int thisBank = memoryController->getMemoryBankID(entry->req->paddr);

    		bool previousIsForSameBank = false;
    		if(entry->previous != NULL){
    			int previousBank = memoryController->getMemoryBankID(entry->previous->req->paddr);
    			if(thisBank == previousBank) previousIsForSameBank = true;
    		}

    		if(entry->req->cmd == Read){
    			if(previousIsWrite) privateLatencyEstimate = 260;
    			else{
    				if(previousIsForSameBank)privateLatencyEstimate = 200;
    				else privateLatencyEstimate = 170;
    			}
    		}
    		else{
    			if(previousIsWrite) privateLatencyEstimate = 250;
    			else{
    				if(previousIsForSameBank) privateLatencyEstimate = 190;
    				else privateLatencyEstimate = 160;
    			}
    		}
    	}

    }
    else{
    	assert(entry->req->privateResultEstimate == DRAM_RESULT_MISS);
    	estimatedNumberOfMisses[fromCPU]++;
    	if(entry->req->cmd == Read) privateLatencyEstimate = 120;
    	else privateLatencyEstimate = 110;
    }

    assert(entry->req->cmd == Read || entry->req->cmd == Writeback || entry->req->cmd == VirtualPrivateWriteback);
    if(entry->req->cmd == Read){
    	entry->req->memCtrlPrivateSeqNum = currentPrivateSeqNum[entry->req->adaptiveMHASenderID];
    	currentPrivateSeqNum[entry->req->adaptiveMHASenderID]++;
    }

    DPRINTF(MemoryControllerInterference, "Estimated service latency for addr %d is %d\n", entry->req->paddr, privateLatencyEstimate);

    entry->req->busAloneServiceEstimate += privateLatencyEstimate;


    // 3. Update head pointer
    updateHeadPointer(entry, headPos, fromCPU);

    DPRINTF(MemoryControllerInterference, "Request %d (%d) beforepointer %d (%d), behindpointer %d (%d), head at entry %d\n",
	    entry,
	    entry->req->paddr,
	    entry->next,
	    (entry->next == NULL ? 0 : entry->next->req->paddr),
	    entry->previous,
	    (entry->previous == NULL ? 0 : entry->previous->req->paddr),
	    entry->headAtEntry->req->paddr);

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
    Param<int> buffer_size;
    Param<bool> use_average_lats;
    Param<bool> pure_head_pointer_model;
END_DECLARE_SIM_OBJECT_PARAMS(RDFCFSControllerInterference)

BEGIN_INIT_SIM_OBJECT_PARAMS(RDFCFSControllerInterference)
	INIT_PARAM_DFLT(memory_controller, "Associated memory controller", NULL),
    INIT_PARAM_DFLT(rf_limit_all_cpus, "Private latency estimation ready first limit", 5),
    INIT_PARAM_DFLT(do_ooo_insert, "If true, a reordering step is applied to the recieved requests (experimental)", false),
    INIT_PARAM_DFLT(cpu_count, "number of cpus",-1),
	INIT_PARAM_DFLT(buffer_size, "buffer size per cpu", -1),
	INIT_PARAM_DFLT(use_average_lats, "if true, the average latencies are used", false),
	INIT_PARAM_DFLT(pure_head_pointer_model, "if true, the queue is traversed from the headptr to the current item", true)

END_INIT_SIM_OBJECT_PARAMS(RDFCFSControllerInterference)

CREATE_SIM_OBJECT(RDFCFSControllerInterference)
{
    return new RDFCFSControllerInterference(getInstanceName(),
					    memory_controller,
					    rf_limit_all_cpus,
					    do_ooo_insert,
					    cpu_count,
					    buffer_size,
					    use_average_lats,
					    pure_head_pointer_model);
}

REGISTER_SIM_OBJECT("RDFCFSControllerInterference", RDFCFSControllerInterference)

#endif



