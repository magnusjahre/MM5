/**
 * @file
 * Describes the Memory Controller API.
 */

#include <sstream>
#include <limits>

#include "mem/bus/controller/rdfcfs_memory_controller.hh"

#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)

#define DO_ESTIMATION_TRACE

using namespace std;

RDFCFSTimingMemoryController::RDFCFSTimingMemoryController(std::string _name,
                                                           int _readqueue_size,
                                                           int _writequeue_size,
                                                           int _reserved_slots,
                                                           bool _infinite_write_bw,
                                                           priority_scheme _priority_scheme,
                                                           page_policy _page_policy,
                                                           int _rflimitAllCPUs)
    : TimingMemoryController(_name) {

    num_active_pages = 0;
    max_active_pages = 4;

    readqueue_size = _readqueue_size;
    writequeue_size = _writequeue_size;
    reserved_slots = _reserved_slots;

    lastIsWrite = false;

    infiniteWriteBW = _infinite_write_bw;

    currentDeliveredReqAt = 0;
    currentOccupyingCPUID = -1;

    lastDeliveredReqAt = 0;
    lastOccupyingCPUID = -1;

    starvationPreventionThreshold = -1; //FIXME: parameterize
    numReqsPastOldest = 0;
    privateBankActSeqNum = 0;

    if(_priority_scheme == FCFS){
        equalReadWritePri = true;
    }
    else if(_priority_scheme == RoW){
        equalReadWritePri = false;
    }
    else{
        fatal("Unsupported priority scheme");
    }

    if(_page_policy == OPEN_PAGE){
        closedPagePolicy = false;
    }
    else if(_page_policy == CLOSED_PAGE){
        closedPagePolicy = true;
    }
    else{
        fatal("Unsupported page policy");
    }

    rfLimitAllCPUs = _rflimitAllCPUs;
    if(rfLimitAllCPUs < 1){
        fatal("RF-limit must be at least 1, 1 gives private FCFS scheduling");
    }

}

void
RDFCFSTimingMemoryController::initializeTraceFiles(Bus* regbus){

    headPointers.resize(regbus->adaptiveMHA->getCPUCount(), NULL);
    tailPointers.resize(regbus->adaptiveMHA->getCPUCount(), NULL);
    readyFirstLimits.resize(regbus->adaptiveMHA->getCPUCount(), rfLimitAllCPUs); //FIXME: parameterize
    privateLatencyBuffer.resize(regbus->adaptiveMHA->getCPUCount(), vector<PrivateLatencyBufferEntry*>());

    requestSequenceNumbers.resize(regbus->adaptiveMHA->getCPUCount(),0);
    currentPrivateSeqNum.resize(regbus->adaptiveMHA->getCPUCount(),0);

#ifdef DO_ESTIMATION_TRACE

    pageResultTraces.resize(regbus->adaptiveMHA->getCPUCount(), RequestTrace());

    for(int i=0;i<regbus->adaptiveMHA->getCPUCount();i++){
        string tmpstr = "";
        stringstream filename;
        filename << "estimation_access_trace_" << i;
        pageResultTraces[i] = RequestTrace(tmpstr, filename.str().c_str());

        vector<string> params;
        params.push_back("Address");
        params.push_back("Bank");
        params.push_back("Result");
        params.push_back("Inserted At");
        params.push_back("Old Address");
        params.push_back("Position");
        params.push_back("Queued Reads");
        params.push_back("Queued Writes");
        params.push_back("Sequence Number");
        params.push_back("Command");

        pageResultTraces[i].initalizeTrace(params);
    }

    privateExecutionOrderTraces.resize(regbus->adaptiveMHA->getCPUCount(), RequestTrace());
    for(int i=0;i<regbus->adaptiveMHA->getCPUCount();i++){
        stringstream filename;
        filename << "private_access_order_" << i;
        privateExecutionOrderTraces[i] = RequestTrace("", filename.str().c_str());

        vector<string> privorderparams;
        privorderparams.push_back("Address");
        privorderparams.push_back("Bank");
        privorderparams.push_back("Command");

        privateExecutionOrderTraces[i].initalizeTrace(privorderparams);
    }

    if(regbus->adaptiveMHA->getCPUCount() == 1){

    	aloneAccessOrderTraces = RequestTrace("", "private_access_order");
    	vector<string> aloneparams;
    	aloneparams.push_back("Address");
    	aloneparams.push_back("Band");
    	aloneparams.push_back("Command");
    	aloneAccessOrderTraces.initalizeTrace(aloneparams);
    }
#endif
}

/** Frees locally allocated memory. */
RDFCFSTimingMemoryController::~RDFCFSTimingMemoryController(){
}

int RDFCFSTimingMemoryController::insertRequest(MemReqPtr &req) {

    req->inserted_into_memory_controller = curTick;

    assert(req->adaptiveMHASenderID != -1);
    req->memCtrlSequenceNumber = requestSequenceNumbers[req->adaptiveMHASenderID];
    requestSequenceNumbers[req->adaptiveMHASenderID]++;

    DPRINTF(MemoryController, "Inserting new request, cmd %s addr %d bank %d, cmd %s\n",
    		req->cmd,
    		req->paddr,
    		getMemoryBankID(req->paddr),
    		req->cmd.toString());

    if(bus->adaptiveMHA->getCPUCount() > 1){
        int privReadCnt = 0;
        for(queueIterator = readQueue.begin();queueIterator != readQueue.end(); queueIterator++){
            MemReqPtr tmp = *queueIterator;
            if(tmp->adaptiveMHASenderID == req->adaptiveMHASenderID) privReadCnt++;
        }

        int privWriteCnt = 0;
        for(queueIterator = writeQueue.begin();queueIterator != writeQueue.end(); queueIterator++){
            MemReqPtr tmp = *queueIterator;
            if(tmp->adaptiveMHASenderID == req->adaptiveMHASenderID) privWriteCnt++;
        }

        req->entryReadCnt = privReadCnt;
        req->entryWriteCnt = privWriteCnt;
    }
    else{
        req->entryReadCnt = readQueue.size();
        req->entryWriteCnt = writeQueue.size();
    }

    if (req->cmd == Read) {
        readQueue.push_back(req);
        if (readQueue.size() > readqueue_size) { // full queue + one in progress
            setBlocked();
        }
    }
    if (req->cmd == Writeback && !infiniteWriteBW) {
        writeQueue.push_back(req);
        if (writeQueue.size() > writequeue_size) { // full queue + one in progress
            setBlocked();
        }
    }

    if(memCtrCPUCount > 1){
        DPRINTF(MemoryControllerInterference, "Recieved request from CPU %d, addr %d, cmd %s\n",
                req->adaptiveMHASenderID,
                req->paddr,
                req->cmd.toString());
        assert(req->adaptiveMHASenderID != -1);

        PrivateLatencyBufferEntry* newEntry = new PrivateLatencyBufferEntry(req);

        if(req->memCtrlGeneratingReadSeqNum == -1){
        	DPRINTF(MemoryControllerInterference,
        			"No order information is known for addr %d, adding to end of queue\n",
        			req->paddr);
        	privateLatencyBuffer[req->adaptiveMHASenderID].push_back(newEntry);
        }
        else{
        	assert(req->cmd == Writeback);

        	vector<PrivateLatencyBufferEntry*>::iterator entryIt = privateLatencyBuffer[req->adaptiveMHASenderID].begin();
        	bool inserted = false;

        	for( ; entryIt != privateLatencyBuffer[req->adaptiveMHASenderID].end(); entryIt++){
        		PrivateLatencyBufferEntry* curEntry = *entryIt;
        		if(!curEntry->scheduled && curEntry->req->memCtrlGeneratingReadSeqNum != -1){
					if(req->memCtrlGeneratingReadSeqNum < curEntry->req->memCtrlGeneratingReadSeqNum){
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
        	}

        	if(!inserted){
        		DPRINTF(MemoryControllerInterference,
        		        			"Addr %d, order %d is the last of the currently seen requests with order information\n",
        		        			newEntry->req->paddr,
									newEntry->req->memCtrlGeneratingReadSeqNum);
        		privateLatencyBuffer[req->adaptiveMHASenderID].push_back(newEntry);
        	}
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
				headPointers[req->adaptiveMHASenderID] = newEntry;
				newEntry->headAtEntry = newEntry;
			}
        }


    }


#ifdef DO_ESTIMATION_TRACE
	if(memCtrCPUCount == 1){
		assert(aloneAccessOrderTraces.isInitialized());
    	vector<RequestTraceEntry> aloneparams;
    	aloneparams.push_back(RequestTraceEntry(req->paddr));
    	aloneparams.push_back(RequestTraceEntry(getMemoryBankID(req->paddr)));
    	aloneparams.push_back(RequestTraceEntry(req->cmd.toString()));
    	aloneAccessOrderTraces.addTrace(aloneparams);
	}
#endif

    return 0;
}

bool RDFCFSTimingMemoryController::hasMoreRequests() {

    if (readQueue.empty() && writeQueue.empty()
        && (closedPagePolicy ? num_active_pages == 0 : true)) {
      return false;
    } else {
      return true;
    }
}

MemReqPtr RDFCFSTimingMemoryController::getRequest() {

    MemReqPtr retval = new MemReq();
    retval->cmd = InvalidCmd;
    retval->paddr = MemReq::inval_addr;

    bool activateFound = false;
    bool closeFound = false;
    bool readyFound = false;

    if (num_active_pages < max_active_pages) {
        activateFound = getActivate(retval);
        if(activateFound) DPRINTF(MemoryController, "Found activate request, cmd %s addr %d bank %d\n", retval->cmd, retval->paddr, getMemoryBankID(retval->paddr));
    }

    if(!activateFound){
        if(closedPagePolicy){
            closeFound = getClose(retval);
            if(closeFound) DPRINTF(MemoryController, "Found close request, cmd %s addr %d bank %d\n", retval->cmd, retval->paddr, getMemoryBankID(retval->paddr));
        }
    }

    if(!activateFound && !closeFound){

        readyFound = getReady(retval);
        if(readyFound){
            DPRINTF(MemoryController, "Found ready request, cmd %s addr %d bank %d\n", retval->cmd, retval->paddr, getMemoryBankID(retval->paddr));
            assert(!bankIsClosed(retval));
            assert(retval->cmd != InvalidCmd);
        }
        else{
            assert(retval->cmd == InvalidCmd);
        }


    }

    if(!activateFound && !closeFound && !readyFound){

        bool otherFound = getOther(retval);
        if(otherFound){
            DPRINTF(MemoryController, "Found other request, cmd %s addr %d bank %d\n", retval->cmd, retval->paddr, getMemoryBankID(retval->paddr));
        }
        assert(otherFound);
        if(closedPagePolicy) assert(!bankIsClosed(retval));
    }

    // Remove request from queue (if read or write)
    if(lastIssuedReq->cmd == Read || lastIssuedReq->cmd == Writeback){
        if(lastIsWrite){
            writeQueue.remove(lastIssuedReq);
        }
        else{
            readQueue.remove(lastIssuedReq);
        }
    }

    // estimate interference caused by this request
    if((retval->cmd == Read || retval->cmd == Writeback) && !isShadow){
        assert(!isShadow);

        // store the ID of the CPU that used the bus prevoiusly and update current vals
        lastDeliveredReqAt = currentDeliveredReqAt;
        lastOccupyingCPUID = currentOccupyingCPUID;
        currentDeliveredReqAt = curTick;
        currentOccupyingCPUID = retval->adaptiveMHASenderID;
    }

    return retval;
}

bool
RDFCFSTimingMemoryController::getActivate(MemReqPtr& req){

    if(equalReadWritePri){
        list<MemReqPtr> mergedQueue = mergeQueues();

        for (queueIterator = mergedQueue.begin(); queueIterator != mergedQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;

            if (!isActive(tmp) && bankIsClosed(tmp)) {
                //Request is not active and bank is closed. Activate it

                MemReqPtr activate = new MemReq();

                activate->cmd = Activate;
                activate->paddr = tmp->paddr;
                activate->flags &= ~SATISFIED;

                assert(activePages.find(getMemoryBankID(tmp->paddr)) == activePages.end());
                activePages[getMemoryBankID(tmp->paddr)] = ActivationEntry(getPage(tmp), curTick);
                num_active_pages++;

                lastIssuedReq = activate;

                req = activate;
                return true;
            }
        }
    }
    else{
        // Go through all lists to see if we can activate anything
        for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (!isActive(tmp) && bankIsClosed(tmp)) {
                //Request is not active and bank is closed. Activate it

                MemReqPtr activate = new MemReq();

                activate->cmd = Activate;
                activate->paddr = tmp->paddr;
                activate->flags &= ~SATISFIED;
                assert(activePages.find(getMemoryBankID(tmp->paddr)) == activePages.end());
                activePages[getMemoryBankID(tmp->paddr)] = ActivationEntry(getPage(tmp),curTick);
                num_active_pages++;

                lastIssuedReq = activate;

                req = activate;
                return true;
            }
        }
        for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (!isActive(tmp) && bankIsClosed(tmp)) {

                //Request is not active and bank is closed. Activate it
                MemReqPtr activate = new MemReq();

                activate->cmd = Activate;
                activate->paddr = tmp->paddr;
                activate->flags &= ~SATISFIED;
                assert(activePages.find(getMemoryBankID(tmp->paddr)) == activePages.end());
                activePages[getMemoryBankID(tmp->paddr)] = ActivationEntry(getPage(tmp),curTick);
                num_active_pages++;

                lastIssuedReq = activate;

                req = activate;
                return true;
            }
        }
    }

    return false;
}

bool
RDFCFSTimingMemoryController::getClose(MemReqPtr& req){
    // Check if we can close the first page (eg, there is no active requests to this page

    for (pageIterator = activePages.begin(); pageIterator != activePages.end(); pageIterator++) {
        Addr Active = pageIterator->second.address;
        bool canClose = true;
        for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (getPage(tmp) == Active) {
                canClose = false;
            }
        }
        for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;

            if (getPage(tmp) == Active) {
                canClose = false;
            }
        }

        if (canClose) {

            MemReqPtr close = new MemReq();

            close->cmd = Close;
            close->paddr = getPageAddr(Active);
            close->flags &= ~SATISFIED;
            activePages.erase(pageIterator);
            num_active_pages--;

            lastIssuedReq = close;

            req = close;
            return true;
        }
    }
    return false;
}

bool
RDFCFSTimingMemoryController::getReady(MemReqPtr& req){

    if(equalReadWritePri){

    	if(starvationPreventionThreshold != -1){
			if(numReqsPastOldest == starvationPreventionThreshold) return false;
			assert(numReqsPastOldest < starvationPreventionThreshold);
    	}

        int position = 0;

        list<MemReqPtr> mergedQueue = mergeQueues();

        for (queueIterator = mergedQueue.begin(); queueIterator != mergedQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;

            if (isReady(tmp)) {

            	if(starvationPreventionThreshold != -1){
					if(position == 0) numReqsPastOldest = 0;
					else numReqsPastOldest++;
            	}

                if(isBlocked() &&
                   readQueue.size() <= readqueue_size &&
                   writeQueue.size() <= writequeue_size){
                   setUnBlocked();
                }

                lastIssuedReq = tmp;
                lastIsWrite = (tmp->cmd == Writeback);

                req = tmp;
                req->memCtrlIssuePosition = position;

                return true;
            }

            position++;
        }
    }
    else{
    	if(starvationPreventionThreshold != -1){
    		fatal("starvation prevention threshold not implemented for RoW scheduling");
    	}

        // Go through the active pages and find a ready operation
        for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (isReady(tmp)) {

                if(isBlocked() &&
                    readQueue.size() <= readqueue_size &&
                    writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                }

                lastIssuedReq = tmp;
                lastIsWrite = false;

                req = tmp;
                return true;
            }
        }
        for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (isReady(tmp)) {

                if(isBlocked() &&
                    readQueue.size() <= readqueue_size &&
                    writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                    }

                    lastIssuedReq = tmp;
                    lastIsWrite = true;

                    req = tmp;
                    return true;
            }
        }
    }

    return false;
}

bool
RDFCFSTimingMemoryController::getOther(MemReqPtr& req){

    if(equalReadWritePri){

        // Send oldest request
    	if(starvationPreventionThreshold != -1){
    		assert(numReqsPastOldest <= starvationPreventionThreshold);
    		numReqsPastOldest = 0;
    	}

        list<MemReqPtr> mergedQueue = mergeQueues();
        assert(!mergedQueue.empty());
        MemReqPtr& tmp = mergedQueue.front();

        if (isActive(tmp)) {

            if(isBlocked() &&
                readQueue.size() <= readqueue_size &&
                writeQueue.size() <= writequeue_size){
                setUnBlocked();
            }

            lastIssuedReq = tmp;
            lastIsWrite = (tmp->cmd == Writeback);

            tmp->memCtrlIssuePosition = 0;
            req = tmp;
            return true;
        }

        assert(!closedPagePolicy);
        return closePageForRequest(req, tmp);

    }
    else{

    	if(starvationPreventionThreshold != -1){
    		fatal("starvation prevention threshold not implemented for RoW scheduling");
    	}

        // Strict read over write priority

        if(!readQueue.empty()){
            MemReqPtr& tmp = readQueue.front();
            if (isActive(tmp)) {

                if(isBlocked() && readQueue.size() <= readqueue_size && writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                }

                lastIssuedReq = tmp;
                lastIsWrite = false;

                req = tmp;
                return true;
            }

            assert(!closedPagePolicy);
            return closePageForRequest(req, tmp);
        }
        else{

            assert(!writeQueue.empty());
            MemReqPtr& tmp = writeQueue.front();
            if (isActive(tmp)) {

                if(isBlocked() && readQueue.size() <= readqueue_size && writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                }

                lastIssuedReq = tmp;
                lastIsWrite = true;

                req = tmp;
                return true;
            }

            assert(!closedPagePolicy);
            return closePageForRequest(req, tmp);
        }
    }

    return false;
}

bool
RDFCFSTimingMemoryController::closePageForRequest(MemReqPtr& choosenReq, MemReqPtr& oldestReq){

    MemReqPtr close = new MemReq();

    if(bankIsClosed(oldestReq)){
        // close the oldest activated page

        Tick min = TICK_T_MAX;
        Addr closeAddr = 0;
        map<int,ActivationEntry>::iterator minIterator = activePages.end();
        for(pageIterator = activePages.begin();pageIterator != activePages.end();pageIterator++){
            ActivationEntry entry = pageIterator->second;
            assert(entry.address != 0);
            if(entry.activatedAt < min){
                min = entry.activatedAt;
                closeAddr = getPageAddr(entry.address);
                minIterator = pageIterator;
            }
        }

        assert(minIterator != activePages.end());
        activePages.erase(minIterator);

        assert(closeAddr != 0);
        close->paddr = closeAddr;

        DPRINTF(MemoryController, "All pages are activated, closing oldest page, addr %d\n", closeAddr);
    }
    else{
        // the needed bank is currently active, close it
        assert(activePages.find(getMemoryBankID(oldestReq->paddr)) != activePages.end());
        activePages.erase(getMemoryBankID(oldestReq->paddr));
        close->paddr = oldestReq->paddr;
    }

    close->cmd = Close;
    close->flags &= ~SATISFIED;
    num_active_pages--;

    lastIssuedReq = close;
    choosenReq = close;
    return true;
}

list<MemReqPtr>
RDFCFSTimingMemoryController::mergeQueues(){
    list<MemReqPtr> retlist;

    list<MemReqPtr>::iterator readIter =  readQueue.begin();
    list<MemReqPtr>::iterator writeIter = writeQueue.begin();

    while(writeIter != writeQueue.end() || readIter != readQueue.end()){

        if(writeIter == writeQueue.end()){
            MemReqPtr& readReq = *readIter;
            retlist.push_back(readReq);
            readIter++;
        }
        else if(readIter == readQueue.end()){
            MemReqPtr& writeReq = *writeIter;
            retlist.push_back(writeReq);
            writeIter++;
        }
        else{
            if((*readIter)->inserted_into_memory_controller <= (*writeIter)->inserted_into_memory_controller){
                MemReqPtr& readReq = *readIter;
                retlist.push_back(readReq);
                readIter++;
            }
            else{
                MemReqPtr& writeReq = *writeIter;
                retlist.push_back(writeReq);
                writeIter++;
            }
        }
        assert(retlist.back()->cmd == Read || retlist.back()->cmd == Writeback);
    }

    // Check that the merge list is sorted
    list<MemReqPtr>::iterator mergedIter = retlist.begin();
    Tick prevTick = 0;
    for( ; mergedIter != retlist.end() ; mergedIter++){
        MemReqPtr& tmpreq = *mergedIter;
        assert(prevTick <= tmpreq->inserted_into_memory_controller);
        prevTick = tmpreq->inserted_into_memory_controller;
    }

    return retlist;
}

list<MemReqPtr>
RDFCFSTimingMemoryController::getPendingRequests(){
    list<MemReqPtr> retval;
    retval.splice(retval.end(), readQueue);
    retval.splice(retval.end(), writeQueue);
    return retval;
}

void
RDFCFSTimingMemoryController::setOpenPages(std::list<Addr> pages){
    fatal("setOpenPages not implemented");
}

void
RDFCFSTimingMemoryController::estimatePageResult(MemReqPtr& req){

    assert(req->cmd == Read || req->cmd == Writeback);

    checkPrivateOpenPage(req);

    if(isPageHitOnPrivateSystem(req)){
        req->privateResultEstimate = DRAM_RESULT_HIT;
    }
    else if(isPageConflictOnPrivateSystem(req)){
        req->privateResultEstimate = DRAM_RESULT_CONFLICT;
    }
    else{
        req->privateResultEstimate = DRAM_RESULT_MISS;
    }

    updatePrivateOpenPage(req);
}

void
RDFCFSTimingMemoryController::dumpBufferStatus(int CPUID){

    cout << "\nDumping buffer contents for CPU " << CPUID << " at " << curTick << "\n";

    for(int i=0;i<privateLatencyBuffer[CPUID].size();i++){
        PrivateLatencyBufferEntry* curEntry = privateLatencyBuffer[CPUID][i];
        cout << i << ": ";
        if(curEntry->scheduled){
            cout << "Scheduled " << curEntry->req->paddr << " "
                 << (curEntry->headAtEntry == NULL ? 0 : curEntry->headAtEntry->req->paddr) << " <- ";
        }
        else{
            cout << "Not scheduled " << curEntry->req->paddr << " "
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
RDFCFSTimingMemoryController::estimatePrivateLatency(MemReqPtr& req){

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
    		privorderparams.push_back(RequestTraceEntry(getMemoryBankID(curTraceCandidate->req->paddr)));
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
    if(oldestNeededHeadIndex > 0 && oldestNeededHeadIndex < privateLatencyBuffer[fromCPU].size()){
        deleteBufferRange(oldestNeededHeadIndex, fromCPU);
    }
}

void
RDFCFSTimingMemoryController::deleteBufferRange(int toIndex, int fromCPU){

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
RDFCFSTimingMemoryController::getArrivalIndex(PrivateLatencyBufferEntry* entry, int fromCPU){
    for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
        if(entry == privateLatencyBuffer[fromCPU][i]){
            return i;
        }
    }
    fatal("entry not found");
    return -1;
}

RDFCFSTimingMemoryController::PrivateLatencyBufferEntry*
RDFCFSTimingMemoryController::schedulePrivateRequest(int fromCPU){

	if(curTick == 10642216){
		dumpBufferStatus(fromCPU);
	}

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
RDFCFSTimingMemoryController::executePrivateRequest(PrivateLatencyBufferEntry* entry, int fromCPU, int headPos){
    estimatePageResult(entry->req);

    assert(!entry->scheduled);
    entry->scheduled = true;

    entry->req->busDelay = 0;
    Tick privateLatencyEstimate = 0;
    if(entry->req->privateResultEstimate == DRAM_RESULT_HIT){
        privateLatencyEstimate = 40;
    }
    else if(entry->req->privateResultEstimate == DRAM_RESULT_CONFLICT){
        if(entry->req->cmd == Read) privateLatencyEstimate = 191;
        else privateLatencyEstimate = 184;
    }
    else{
        if(entry->req->cmd == Read) privateLatencyEstimate = 120;
        else privateLatencyEstimate = 109;
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
RDFCFSTimingMemoryController::updateHeadPointer(PrivateLatencyBufferEntry* entry, int headPos, int fromCPU){
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

void
RDFCFSTimingMemoryController::computeInterference(MemReqPtr& req, Tick busOccupiedFor){

    //FIXME: how should additional L2 misses be handled
    estimatePrivateLatency(req);

#ifdef DO_ESTIMATION_TRACE

    vector<RequestTraceEntry> vals;
    vals.push_back(RequestTraceEntry(req->paddr));
    vals.push_back(RequestTraceEntry(getMemoryBankID(req->paddr)));

    if(req->privateResultEstimate == DRAM_RESULT_CONFLICT) vals.push_back(RequestTraceEntry("conflict"));
    else if(req->privateResultEstimate == DRAM_RESULT_HIT) vals.push_back(RequestTraceEntry("hit"));
    else vals.push_back(RequestTraceEntry("miss"));

    vals.push_back(RequestTraceEntry(req->inserted_into_memory_controller));
    vals.push_back(RequestTraceEntry(req->oldAddr == MemReq::inval_addr ? 0 : req->oldAddr ));
    vals.push_back(RequestTraceEntry(req->memCtrlIssuePosition));
    vals.push_back(RequestTraceEntry(req->entryReadCnt));
    vals.push_back(RequestTraceEntry(req->entryWriteCnt));
    vals.push_back(RequestTraceEntry(req->memCtrlSequenceNumber));
    vals.push_back(RequestTraceEntry(req->cmd.toString()));

    pageResultTraces[req->adaptiveMHASenderID].addTrace(vals);

#endif
}

void
RDFCFSTimingMemoryController::checkPrivateOpenPage(MemReqPtr& req){
    if(activatedPages.empty()) initializePrivateStorage();

    int actCnt = 0;
    Tick minAct = TICK_T_MAX;
    int minID = -1;
    int bank = getMemoryBankID(req->paddr);
    assert(req->adaptiveMHASenderID != -1);
    int cpuID = req->adaptiveMHASenderID;

    for(int i=0;i<activatedPages[cpuID].size();i++){
        if(activatedPages[cpuID][i] != 0){
            actCnt++;

            assert(activatedAt[cpuID][i] != 0);
            if(activatedAt[cpuID][i] < minAct){
                minAct = activatedAt[cpuID][i];
                minID = i;
            }
        }
    }

    if(actCnt == max_active_pages){
        if(activatedPages[cpuID][bank] == 0){
            // the memory controller has closed the oldest activated page, update storage
            activatedPages[cpuID][minID] = 0;
            activatedAt[cpuID][minID] = 0;
        }
    }
    else{
        assert(actCnt < max_active_pages);
    }
}

void
RDFCFSTimingMemoryController::updatePrivateOpenPage(MemReqPtr& req){

    assert(!closedPagePolicy);

    Addr curPage = getPage(req->paddr);
    int bank = getMemoryBankID(req->paddr);
    int cpuID = req->adaptiveMHASenderID;

    if(activatedPages[cpuID][bank] != curPage){
    	privateBankActSeqNum++;
    	assert(privateBankActSeqNum < TICK_T_MAX);
        activatedAt[cpuID][bank] = privateBankActSeqNum;
    }

    activatedPages[cpuID][bank] = curPage;
}

bool
RDFCFSTimingMemoryController::isPageHitOnPrivateSystem(MemReqPtr& req){

    if(activatedPages.empty()) initializePrivateStorage();

    assert(!closedPagePolicy);

    Addr curPage = getPage(req->paddr);
    int bank = getMemoryBankID(req->paddr);
    int cpuID = req->adaptiveMHASenderID;

    return curPage == activatedPages[cpuID][bank];
}

bool
RDFCFSTimingMemoryController::isPageConflictOnPrivateSystem(MemReqPtr& req){

    if(activatedPages.empty()) initializePrivateStorage();

    assert(!closedPagePolicy);

    Addr curPage = getPage(req->paddr);
    int bank = getMemoryBankID(req->paddr);
    int cpuID = req->adaptiveMHASenderID;

    return curPage != activatedPages[cpuID][bank] && activatedPages[cpuID][bank] != 0;
}

void RDFCFSTimingMemoryController::initializePrivateStorage(){
    activatedPages.resize(memCtrCPUCount, std::vector<Addr>(mem_interface->getMemoryBankCount(), 0));
    activatedAt.resize(memCtrCPUCount, std::vector<Tick>(mem_interface->getMemoryBankCount(), 0));
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)

    Param<int> readqueue_size;
    Param<int> writequeue_size;
    Param<int> reserved_slots;
    Param<bool> inf_write_bw;
    Param<string> page_policy;
    Param<string> priority_scheme;
    Param<int> rf_limit_all_cpus;
END_DECLARE_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)
    INIT_PARAM_DFLT(readqueue_size, "Max size of read queue", 64),
    INIT_PARAM_DFLT(writequeue_size, "Max size of write queue", 64),
    INIT_PARAM_DFLT(reserved_slots, "Number of activations reserved for reads", 2),
    INIT_PARAM_DFLT(inf_write_bw, "Infinite writeback bandwidth", false),
    INIT_PARAM_DFLT(page_policy, "Controller page policy", "ClosedPage"),
    INIT_PARAM_DFLT(priority_scheme, "Controller priority scheme", "FCFS"),
    INIT_PARAM_DFLT(rf_limit_all_cpus, "Private latency estimation ready first limit", 5)
END_INIT_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)


CREATE_SIM_OBJECT(RDFCFSTimingMemoryController)
{
    string page_policy_name = page_policy;
    RDFCFSTimingMemoryController::page_policy policy = RDFCFSTimingMemoryController::CLOSED_PAGE;
    if(page_policy_name == "ClosedPage"){
        policy = RDFCFSTimingMemoryController::CLOSED_PAGE;
    }
    else{
        policy = RDFCFSTimingMemoryController::OPEN_PAGE;
    }

    string priority_scheme_name = priority_scheme;
    RDFCFSTimingMemoryController::priority_scheme priority = RDFCFSTimingMemoryController::FCFS;
    if(priority_scheme_name == "FCFS"){
        priority = RDFCFSTimingMemoryController::FCFS;
    }
    else{
        priority = RDFCFSTimingMemoryController::RoW;
    }

    return new RDFCFSTimingMemoryController(getInstanceName(),
                                            readqueue_size,
                                            writequeue_size,
                                            reserved_slots,
                                            inf_write_bw,
                                            priority,
                                            policy,
                                            rf_limit_all_cpus);
}

REGISTER_SIM_OBJECT("RDFCFSTimingMemoryController", RDFCFSTimingMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS
