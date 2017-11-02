/**
 * @file
 * Describes the Memory Controller API.
 */

#include <sstream>

#include "mem/bus/controller/rdfcfs_memory_controller.hh"

#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)

//#define DO_ESTIMATION_TRACE

//#define DO_OCCUPANCY_TRACE

using namespace std;

RDFCFSTimingMemoryController::RDFCFSTimingMemoryController(std::string _name,
                                                           int _readqueue_size,
                                                           int _writequeue_size,
                                                           int _reserved_slots,
                                                           bool _infinite_write_bw,
                                                           priority_scheme _priority_scheme,
                                                           page_policy _page_policy,
														   int _starvationThreshold)
    : TimingMemoryController(_name) {

    num_active_pages = 0;

    readqueue_size = _readqueue_size;
    writequeue_size = _writequeue_size;
    reserved_slots = _reserved_slots;

    lastIsWrite = false;

    infiniteWriteBW = _infinite_write_bw;

    currentDeliveredReqAt = 0;
    currentOccupyingCPUID = -1;

    lastDeliveredReqAt = 0;
    lastOccupyingCPUID = -1;

    starvationPreventionThreshold = _starvationThreshold;
    numReqsPastOldest = 0;

    highPriCPUID = -1;

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
}

RDFCFSTimingMemoryController::~RDFCFSTimingMemoryController(){
}

void
RDFCFSTimingMemoryController::initializeTraceFiles(Bus* regbus){

	requestSequenceNumbers.resize(regbus->adaptiveMHA->getCPUCount(),0);

#ifdef DO_OCCUPANCY_TRACE
	queueOccupancyTrace = RequestTrace(name(), "QueueOccupancyTrace");

	vector<string> occParams;
	occParams.resize(regbus->adaptiveMHA->getCPUCount() * 3, "");
	for(int i=0;i<regbus->adaptiveMHA->getCPUCount();i++){
		stringstream filename;
		filename << "CPU" << i << " Reads";
		occParams[i] = filename.str();
	}

	for(int i=regbus->adaptiveMHA->getCPUCount();i<occParams.size();i++){
		stringstream filename;
		if(i<regbus->adaptiveMHA->getCPUCount()*2){
			filename << "CPU" << i - regbus->adaptiveMHA->getCPUCount()  << " Priv Writes";
		}
		else{
			filename << "CPU" << i - regbus->adaptiveMHA->getCPUCount()*2  << " Shared Writes";
		}
		occParams[i] = filename.str();
	}

	queueOccupancyTrace.initalizeTrace(occParams);

#endif



#ifdef DO_ESTIMATION_TRACE

    pageResultTraces.resize(regbus->adaptiveMHA->getCPUCount(), RequestTrace());

    for(int i=0;i<regbus->adaptiveMHA->getCPUCount();i++){
        stringstream filename;
        filename << "_estimation_access_trace_" << i;
        pageResultTraces[i] = RequestTrace(name(), filename.str().c_str());

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

    if(regbus->adaptiveMHA->getCPUCount() == 1){

    	aloneAccessOrderTraces = RequestTrace(name(), "_private_access_order");
    	vector<string> aloneparams;
    	aloneparams.push_back("Address");
    	aloneparams.push_back("Bank");
    	aloneparams.push_back("Command");
    	aloneparams.push_back("WB gen by");
    	aloneAccessOrderTraces.initalizeTrace(aloneparams);
    }
#endif
}

void
RDFCFSTimingMemoryController::incrementWaitRequestCnt(int increment){
	int reqCnt = 0;
	for(queueIterator = readQueue.begin();queueIterator != readQueue.end(); queueIterator++){
		MemReqPtr tmp = *queueIterator;
		tmp->numMembusWaitReqs += increment;
		reqCnt++;
	}

	for(queueIterator = writeQueue.begin();queueIterator != writeQueue.end(); queueIterator++){
		MemReqPtr tmp = *queueIterator;
		tmp->numMembusWaitReqs += increment;
		reqCnt++;
	}

	DPRINTF(MemoryController, "Increasing the request wait counter for %d requests by %d\n", reqCnt, increment);
}

int RDFCFSTimingMemoryController::insertRequest(MemReqPtr &req) {

	if(controllerInterference != NULL && !controllerInterference->isInitialized()){
		controllerInterference->initialize(memCtrCPUCount);
	}

    req->inserted_into_memory_controller = curTick;

    if(req->adaptiveMHASenderID != -1){
    	req->memCtrlSequenceNumber = requestSequenceNumbers[req->adaptiveMHASenderID];
    	requestSequenceNumbers[req->adaptiveMHASenderID]++;

    }

    DPRINTF(MemoryController, "Inserting new request, cmd %s addr %d bank %d, page %d, CPUID %d\n",
    		req->cmd,
    		req->paddr,
    		getMemoryBankID(req->paddr),
    		getPage(req),
			(req->cmd == Writeback ? req->nfqWBID : req->adaptiveMHASenderID));

    vector<int> waitingReads;
    vector<int> waitingWrites;
    waitingReads.resize(bus->adaptiveMHA->getCPUCount(), 0);
    waitingWrites.resize(bus->adaptiveMHA->getCPUCount()*2, 0);

    if(bus->adaptiveMHA->getCPUCount() > 1){
        int privReadCnt = 0;
        for(queueIterator = readQueue.begin();queueIterator != readQueue.end(); queueIterator++){
            MemReqPtr tmp = *queueIterator;
            if(tmp->adaptiveMHASenderID == req->adaptiveMHASenderID && req->adaptiveMHASenderID != -1) privReadCnt++;
            if(tmp->adaptiveMHASenderID != -1) waitingReads[tmp->adaptiveMHASenderID]++;
        }

        int privWriteCnt = 0;
        for(queueIterator = writeQueue.begin();queueIterator != writeQueue.end(); queueIterator++){
            MemReqPtr tmp = *queueIterator;
            if(tmp->adaptiveMHASenderID == req->adaptiveMHASenderID && req->adaptiveMHASenderID != -1) privWriteCnt++;
            if(tmp->adaptiveMHASenderID != -1){
            	if(tmp->adaptiveMHASenderID != -1){
            		waitingWrites[tmp->adaptiveMHASenderID]++;
            	}
            	else{
            		waitingWrites[tmp->adaptiveMHASenderID + bus->adaptiveMHA->getCPUCount()]++;
            	}
            }
        }

        req->entryReadCnt = privReadCnt;
        req->entryWriteCnt = privWriteCnt;
    }
    else{
        req->entryReadCnt = readQueue.size();
        req->entryWriteCnt = writeQueue.size();

        assert(waitingReads.size() == 1 && waitingWrites.size() == 2);
        waitingReads[0] = readQueue.size();

        for(queueIterator = writeQueue.begin();queueIterator != writeQueue.end(); queueIterator++){
        	if((*queueIterator)->adaptiveMHASenderID != -1 ){
        		waitingWrites[0]++;
        	}
        	else{
        		waitingWrites[1]++;
        	}
        }
    }

#ifdef DO_OCCUPANCY_TRACE
    vector<RequestTraceEntry> tracevals;
    for(int i=0;i<waitingReads.size();i++) tracevals.push_back(RequestTraceEntry(waitingReads[i]));
    for(int i=0;i<waitingWrites.size();i++) tracevals.push_back(RequestTraceEntry(waitingWrites[i]));
    queueOccupancyTrace.addTrace(tracevals);
#endif

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

    if(memCtrCPUCount > 1 && controllerInterference != NULL){
        controllerInterference->insertRequest(req);
    }


#ifdef DO_ESTIMATION_TRACE
	if(memCtrCPUCount == 1){
		assert(aloneAccessOrderTraces.isInitialized());
    	vector<RequestTraceEntry> aloneparams;
    	aloneparams.push_back(RequestTraceEntry(req->paddr));
    	aloneparams.push_back(RequestTraceEntry(getMemoryBankID(req->paddr)));
    	aloneparams.push_back(RequestTraceEntry(req->cmd.toString()));
    	aloneparams.push_back(RequestTraceEntry(req->memCtrlWbGenBy == MemReq::inval_addr ? 0 : req->memCtrlWbGenBy));
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

	checkMaxActivePages();

    MemReqPtr retval = new MemReq();
    retval->cmd = InvalidCmd;
    retval->paddr = MemReq::inval_addr;

    bool activateFound = false;
    bool closeFound = false;
    bool readyFound = false;

    DPRINTF(MemoryController, "Scheduling, %d banks active\n", num_active_pages);

    if (num_active_pages < max_active_pages) {
        activateFound = getActivate(retval);
        if(activateFound) DPRINTF(MemoryController, "Found activate request, cmd %s addr %d bank %d page %d\n", retval->cmd, retval->paddr, getMemoryBankID(retval->paddr), getPage(retval));
    }

    if(!activateFound){
        if(closedPagePolicy){
            closeFound = getClose(retval);
            if(closeFound) DPRINTF(MemoryController, "Found close request, cmd %s addr %d bank %d page %d\n", retval->cmd, retval->paddr, getMemoryBankID(retval->paddr), getPage(retval));
        }
    }

    if(!activateFound && !closeFound){

        readyFound = getReady(retval);
        if(readyFound){
            DPRINTF(MemoryController, "Found ready request, cmd %s addr %d bank %d page %d cpu %d (%d), position %d\n",
            		retval->cmd,
					retval->paddr,
					getMemoryBankID(retval->paddr),
					getPage(retval),
					retval->adaptiveMHASenderID,
					retval->nfqWBID,
					retval->memCtrlIssuePosition);
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
            DPRINTF(MemoryController, "Found other request, cmd %s addr %d bank %d page %d\n", retval->cmd, retval->paddr, getMemoryBankID(retval->paddr), getPage(retval));
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

        if(retval->adaptiveMHASenderID == highPriCPUID && currentOccupyingCPUID != highPriCPUID && retval->cmd == Read){
        	assert(curTick >= retval->inserted_into_memory_controller);
        	bus->interferenceManager->asrEpocMeasurements.addValue(highPriCPUID,
        			ASREpochMeasurements::EPOCH_QUEUEING_CYCLES,
					curTick - retval->inserted_into_memory_controller);
        }

        // store the ID of the CPU that used the bus prevoiusly and update current vals
        lastDeliveredReqAt = currentDeliveredReqAt;
        lastOccupyingCPUID = currentOccupyingCPUID;
        currentDeliveredReqAt = curTick;
        currentOccupyingCPUID = (retval->cmd == Read ? retval->adaptiveMHASenderID : retval->nfqWBID);
    }

    return retval;
}

bool
RDFCFSTimingMemoryController::getActivate(MemReqPtr& req){

    if(equalReadWritePri){
        list<MemReqPtr> mergedQueue = mergeQueues();

        for (queueIterator = mergedQueue.begin(); queueIterator != mergedQueue.end(); queueIterator++) {
            MemReqPtr tmp = *queueIterator;

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
            MemReqPtr tmp = *queueIterator;
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
            MemReqPtr tmp = *queueIterator;
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
        Addr active = pageIterator->second.address;
        DPRINTF(MemoryController, "Checking if page %d can be closed\n", active);
        bool canClose = true;
        for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
            MemReqPtr tmp = *queueIterator;
            if (getPage(tmp) == active) {
            	DPRINTF(MemoryController, "Read for addr %d needs active page %d, cannot close\n", tmp->paddr, active);
                canClose = false;
            }
        }
        for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
            MemReqPtr tmp = *queueIterator;

            if (getPage(tmp) == active) {
            	DPRINTF(MemoryController, "Write for addr %d needs active page %d, cannot close\n", tmp->paddr, active);
                canClose = false;
            }
        }

        if (canClose) {

            MemReqPtr close = new MemReq();

            close->cmd = Close;
            close->paddr = getPageAddr(active);
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
			if(numReqsPastOldest == starvationPreventionThreshold){
				DPRINTF(MemoryController, "Number of bypassing requests is %d, starvation threshold %d, forcing oldest request\n",
						numReqsPastOldest,
						starvationPreventionThreshold);
				return false;
			}

			DPRINTF(MemoryController, "Number of bypassing requests is %d, starvation threshold %d, allowing search for ready request\n",
									numReqsPastOldest,
									starvationPreventionThreshold);

			assert(numReqsPastOldest < starvationPreventionThreshold);
    	}

        int position = 0;

        list<MemReqPtr> mergedQueue = mergeQueues();

        for (queueIterator = mergedQueue.begin(); queueIterator != mergedQueue.end(); queueIterator++) {
            MemReqPtr tmp = *queueIterator;

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
            MemReqPtr tmp = *queueIterator;
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
            MemReqPtr tmp = *queueIterator;

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
        MemReqPtr tmp = mergedQueue.front();

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
            MemReqPtr tmp = readQueue.front();

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
            MemReqPtr tmp = writeQueue.front();

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

int
RDFCFSTimingMemoryController::countRequests(int cpuID, std::list<MemReqPtr>* queue){
	int cnt = 0;
	list<MemReqPtr>::iterator iter =  queue->begin();
	while(iter != queue->end()){
		MemReqPtr req = *iter;
		assert(req->cmd == Read || req->cmd == Writeback);
		if(req->cmd == Read){
			assert(req->adaptiveMHASenderID != -1);
			if(req->adaptiveMHASenderID == cpuID) cnt++;
		}
		else if(req->cmd == Writeback){
			assert(req->nfqWBID != -1);
			if(req->nfqWBID == cpuID) cnt++;
		}
		iter++;
	}
	return cnt;
}

list<MemReqPtr>
RDFCFSTimingMemoryController::mergeQueues(){
    list<MemReqPtr> retlist;

    list<MemReqPtr>::iterator readIter =  readQueue.begin();
    list<MemReqPtr>::iterator writeIter = writeQueue.begin();

    while(writeIter != writeQueue.end() || readIter != readQueue.end()){

        if(writeIter == writeQueue.end()){
            MemReqPtr readReq = *readIter;
            retlist.push_back(readReq);
            readIter++;
        }
        else if(readIter == readQueue.end()){
            MemReqPtr writeReq = *writeIter;
            retlist.push_back(writeReq);
            writeIter++;
        }
        else{
            if((*readIter)->inserted_into_memory_controller <= (*writeIter)->inserted_into_memory_controller){
                MemReqPtr readReq = *readIter;
                retlist.push_back(readReq);
                readIter++;
            }
            else{
                MemReqPtr writeReq = *writeIter;
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
        MemReqPtr tmpreq = *mergedIter;
        assert(prevTick <= tmpreq->inserted_into_memory_controller);
        prevTick = tmpreq->inserted_into_memory_controller;
    }

    if(highPriCPUID != -1){
    	int highPriReadCnt = countRequests(highPriCPUID, &readQueue);
    	int highPriWriteCnt = countRequests(highPriCPUID, &writeQueue);
    	DPRINTF(MemoryController, "High priority CPU %d has %d pending reads and %d pending writes\n",
    			highPriCPUID,
				highPriReadCnt,
				highPriWriteCnt);

    	if(highPriReadCnt + highPriWriteCnt == 0){
    		DPRINTF(MemoryController, "High priority CPU %d has no pending requests, returning unmodified merged queue\n", highPriCPUID);
    		return retlist;
    	}
    	return filterQueue(retlist);
    }
    return retlist;
}

std::list<MemReqPtr>
RDFCFSTimingMemoryController::filterQueue(std::list<MemReqPtr> queue){

	list<MemReqPtr> filteredList;
	list<MemReqPtr>::iterator filterIter = queue.begin();

	for( ; filterIter != queue.end() ; filterIter++){
		MemReqPtr candReq = *filterIter;
		bool add = false;
		if(candReq->cmd == Read){
			assert(candReq->adaptiveMHASenderID != -1);
			if(candReq->adaptiveMHASenderID == highPriCPUID){
				add = true;
			}
		}
		if(candReq->cmd == Writeback){
			assert(candReq->nfqWBID != -1);
			if(candReq->nfqWBID == highPriCPUID){
				add = true;
			}
		}

		if(add){
			DPRINTF(MemoryController, "Adding high priority CPU %d request addr %d, cmd %s to filtered list (queued at %d)\n",
					highPriCPUID,
					candReq->paddr,
					candReq->cmd,
					candReq->inserted_into_memory_controller);
			filteredList.push_back(candReq);
		}
		else{
			DPRINTF(MemoryController, "Skipping request from CPU %d, addr %d, cmd %s (queued at %d)\n",
					(candReq->cmd == Read ? candReq->adaptiveMHASenderID : candReq->nfqWBID),
					candReq->paddr,
					candReq->cmd,
					candReq->inserted_into_memory_controller);
		}
	}
	return filteredList;
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
RDFCFSTimingMemoryController::computeInterference(MemReqPtr& req, Tick busOccupiedFor){

	if(controllerInterference != NULL){
		controllerInterference->estimatePrivateLatency(req, busOccupiedFor);
	}

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
RDFCFSTimingMemoryController::setASRHighPriCPUID(int cpuID){
	DPRINTF(ASRPolicy, "Setting CPU %d as the high priority thread, was %d\n", cpuID, highPriCPUID);
	highPriCPUID = cpuID;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)
    Param<int> readqueue_size;
    Param<int> writequeue_size;
    Param<int> reserved_slots;
    Param<bool> inf_write_bw;
    Param<string> page_policy;
    Param<string> priority_scheme;
    Param<int> starvation_threshold;
END_DECLARE_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)
    INIT_PARAM_DFLT(readqueue_size, "Max size of read queue", 64),
    INIT_PARAM_DFLT(writequeue_size, "Max size of write queue", 64),
    INIT_PARAM_DFLT(reserved_slots, "Number of activations reserved for reads", 2),
    INIT_PARAM_DFLT(inf_write_bw, "Infinite writeback bandwidth", false),
    INIT_PARAM_DFLT(page_policy, "Controller page policy", "ClosedPage"),
    INIT_PARAM_DFLT(priority_scheme, "Controller priority scheme", "FCFS"),
	INIT_PARAM_DFLT(starvation_threshold, "Maximum number of non-oldest requests to issue in a row", -1)
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
											starvation_threshold);
}

REGISTER_SIM_OBJECT("RDFCFSTimingMemoryController", RDFCFSTimingMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS
