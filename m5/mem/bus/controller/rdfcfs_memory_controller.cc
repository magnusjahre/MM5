/**
 * @file
 * Describes the Memory Controller API.
 */

#include <sstream>

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
                                                           page_policy _page_policy)
    : TimingMemoryController(_name) {
    
    num_active_pages = 0;
    max_active_pages = 4;
    
    readqueue_size = _readqueue_size;
    writequeue_size = _writequeue_size;
    reserved_slots = _reserved_slots;

//     close = new MemReq();
//     close->cmd = Close;
//     activate = new MemReq();
//     activate->cmd = Activate;
//     
//     invalReq = new MemReq();
//     invalReq->cmd = InvalidCmd;

    lastIsWrite = false;
    
    infiniteWriteBW = _infinite_write_bw;
    
    currentDeliveredReqAt = 0;
    currentOccupyingCPUID = -1;
    
    lastDeliveredReqAt = 0;
    lastOccupyingCPUID = -1;
    
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

void 
RDFCFSTimingMemoryController::initializeTraceFiles(Bus* regbus){

    headPointers.resize(regbus->adaptiveMHA->getCPUCount(), NULL);
    tailPointers.resize(regbus->adaptiveMHA->getCPUCount(), NULL);
    readyFirstLimits.resize(regbus->adaptiveMHA->getCPUCount(), 5); //FIXME: parameterize
    privateLatencyBuffer.resize(regbus->adaptiveMHA->getCPUCount(), vector<PrivateLatencyBufferEntry*>());
    
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
        
        pageResultTraces[i].initalizeTrace(params);
    }
#endif
}

/** Frees locally allocated memory. */
RDFCFSTimingMemoryController::~RDFCFSTimingMemoryController(){
}

int RDFCFSTimingMemoryController::insertRequest(MemReqPtr &req) {
    
    req->inserted_into_memory_controller = curTick;
    
    DPRINTF(MemoryController, "Inserting new request, cmd %s addr %d bank %d\n", req->cmd, req->paddr, getMemoryBankID(req->paddr));
    
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
        DPRINTF(MemoryControllerInterference, "Recieved request from CPU %d, addr %d\n", 
                req->adaptiveMHASenderID,
                req->paddr);
        assert(req->adaptiveMHASenderID != -1);
        PrivateLatencyBufferEntry* newEntry = new PrivateLatencyBufferEntry(req);
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
            newEntry->headAtEntry = headPointers[req->adaptiveMHASenderID];
        }
        
        privateLatencyBuffer[req->adaptiveMHASenderID].push_back(newEntry);
    }

    assert(req->cmd == Read || req->cmd == Writeback);
    
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
        
        int position = 0;
        
        list<MemReqPtr> mergedQueue = mergeQueues();
        for (queueIterator = mergedQueue.begin(); queueIterator != mergedQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            
            if (isReady(tmp)) {
                
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
        if(curEntry->scheduled) cout << "Scheduled " << curEntry->req->paddr << " " << curEntry << " behind ";
        else cout << "Not scheduled " << curEntry->req->paddr << " " << curEntry << " " ;
        
        int pos = 0;
        PrivateLatencyBufferEntry* tmp = curEntry->scheduledBehind;
        while(tmp != NULL){
            cout << "(" << pos << ", " << tmp->req->paddr << ", " << tmp << ") <- ";
            tmp = tmp->scheduledBehind;
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
            DPRINTF(MemoryControllerInterference, "Request for addr %d allready scheduled\n", curLBE->req->paddr);
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
    PrivateLatencyBufferEntry* historyLBE = curLBE->headAtEntry;
    assert(historyLBE != NULL);
    if(historyLBE->scheduled){
        while(historyLBE != curLBE){
            assert(historyLBE->req);
            DPRINTF(MemoryControllerInterference, "Retrieving latency %d from addr %d, next buffer entry addr is %d\n",
                    historyLBE->req->busAloneServiceEstimate,
                    historyLBE->req->paddr,
                    historyLBE->scheduledBefore);
            queueLatency += historyLBE->req->busAloneServiceEstimate;
            historyLBE = historyLBE->scheduledBefore;
            assert(historyLBE != NULL);
        }
    }
    else{
        DPRINTF(MemoryControllerInterference, "Head pointer %d (%d) for request %d (%d), has not been scheduled, 0 queue latency\n",
                curLBE->headAtEntry,
                curLBE->headAtEntry->req->paddr,
                curLBE,
                curLBE->req->paddr);
    }
    
    req->busAloneWriteQueueEstimate = 0;
    req->busAloneReadQueueEstimate = queueLatency;
    
    DPRINTF(MemoryControllerInterference, "History traversal finished, estimated %d cycles of queue latency\n", queueLatency);
    
    // 3. Delete any requests that are no longer needed
    //    i.e. not pointed to by a head ptr, not used in path from any head ptr and has been scheduled
    DPRINTF(MemoryControllerInterference, "--- Step 3, delete old entries\n");
    list<PrivateLatencyBufferEntry*> deleteList;
    for(int i=0;i<privateLatencyBuffer[fromCPU].size();i++){
        if( privateLatencyBuffer[fromCPU][i]->canDelete()
            && privateLatencyBuffer[fromCPU][i] != tailPointers[fromCPU]
            && privateLatencyBuffer[fromCPU][i] != headPointers[fromCPU]){
            
            DPRINTF(MemoryControllerInterference, "Queue element %d is scheduled and not the current tail, addr %d, checking for deletion\n", i, (*privateLatencyBuffer[fromCPU][i]).req->paddr);
            
            bool pointedTo = false;
            bool onPath = false;
            
            for(int j=0;j<privateLatencyBuffer[fromCPU].size();j++){
                if(j != i && !privateLatencyBuffer[fromCPU][j]->canDelete()){
                    
                    if(privateLatencyBuffer[fromCPU][j]->headAtEntry == privateLatencyBuffer[fromCPU][i]){
                        DPRINTF(MemoryControllerInterference, 
                                "Queue element %d (%d) is pointed to by element %d (%d)\n",
                                i,
                                (*privateLatencyBuffer[fromCPU][i]).req->paddr,
                                j,
                                (*privateLatencyBuffer[fromCPU][j]).req->paddr);
                        pointedTo = true;
                    }
                    
                    if(!pointedTo){
                        
                        PrivateLatencyBufferEntry* tmp = privateLatencyBuffer[fromCPU][j]->headAtEntry;
                        
                        assert(tmp != NULL);
                        while(tmp != NULL){
                            
                            if(tmp == privateLatencyBuffer[fromCPU][i]){
                                DPRINTF(MemoryControllerInterference, 
                                        "Queue element %d (%d) is sheduled after addr %d which is the head of queue for element %d (%d)\n",
                                        i,
                                        privateLatencyBuffer[fromCPU][i]->req->paddr,
                                        privateLatencyBuffer[fromCPU][j]->headAtEntry,
                                        j,
                                        privateLatencyBuffer[fromCPU][j]->req->paddr);
                                
                                onPath = true;
                            }
                            tmp = tmp->scheduledBefore;
                        }
                    }
                }
            }
            
            if(!pointedTo && !onPath){
                DPRINTF(MemoryControllerInterference, "Adding addr %d to the delete list\n", privateLatencyBuffer[fromCPU][i]->req->paddr);
                deleteList.push_back(privateLatencyBuffer[fromCPU][i]);
            }
        }
    }
    
    while(!deleteList.empty()){
        vector<PrivateLatencyBufferEntry*>::iterator it = privateLatencyBuffer[fromCPU].begin();
        for( ;it != privateLatencyBuffer[fromCPU].end();it++){
            if(*it == deleteList.front()){
                
                DPRINTF(MemoryControllerInterference, "Deleting element %d\n", (*it)->req->paddr);
                
                if((*it)->scheduledBefore != NULL){
                    DPRINTF(MemoryControllerInterference,
                            "Removing scheduled before pointer of element %d\n",
                            (*it)->scheduledBefore->req->paddr);
                    (*it)->scheduledBefore->scheduledBehind = NULL;
                }
                
                if((*it)->scheduledBehind != NULL){
                    DPRINTF(MemoryControllerInterference,
                            "Removing scheduled behind pointer of element %d\n",
                            (*it)->scheduledBehind->req->paddr);
                    (*it)->scheduledBehind->scheduledBefore = NULL;
                }
                
                if(tailPointers[fromCPU] == *it){
                    DPRINTF(MemoryControllerInterference, "Element is on the tail of the queue, nulling tail pointer\n");
                    tailPointers[fromCPU] = NULL;
                }
                
                if(headPointers[fromCPU] == *it){
                    DPRINTF(MemoryControllerInterference, "Element the head of the queue, nulling head pointer\n");
                    headPointers[fromCPU] = NULL;
                }
                
                delete *it;
                privateLatencyBuffer[fromCPU].erase(it);
                break;
            }
        }
        deleteList.pop_front();
    }
}

RDFCFSTimingMemoryController::PrivateLatencyBufferEntry*
RDFCFSTimingMemoryController::schedulePrivateRequest(int fromCPU){
    
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
    
    DPRINTF(MemoryControllerInterference, "Estimated service latency for addr %d is %d\n", entry->req->paddr, privateLatencyEstimate);
    
    entry->req->busAloneServiceEstimate += privateLatencyEstimate;

    if(tailPointers[fromCPU] != NULL){
        tailPointers[fromCPU]->scheduledBefore = entry;
        entry->scheduledBehind = tailPointers[fromCPU];
    }
    DPRINTF(MemoryControllerInterference,
            "Updating tail pointer, previous was %d, changing to %d, new tail addr is %d, new tail is scheduled behind %d, old tail scheduled before %d\n", 
            tailPointers[fromCPU], entry, entry->req->paddr,
            (entry->scheduledBehind == NULL ? NULL : entry->scheduledBehind->req->paddr),
            (tailPointers[fromCPU] == NULL ? NULL : 
                    (tailPointers[fromCPU]->scheduledBefore == NULL ? NULL :
                    tailPointers[fromCPU]->scheduledBefore->req->paddr)));
    tailPointers[fromCPU] = entry;

    updateHeadPointer(entry, headPos, fromCPU);

    DPRINTF(MemoryControllerInterference, "Request %d (%d) beforepointer %d (%d), behindpointer %d (%d)\n",
	    entry,
	    entry->req->paddr,
	    entry->scheduledBefore,
	    (entry->scheduledBefore == NULL ? 0 : entry->scheduledBefore->req->paddr),
	    entry->scheduledBehind,
	    (entry->scheduledBehind == NULL ? 0 : entry->scheduledBehind->req->paddr) );
    
    if(entry->headAtEntry != entry){
        assert( !(entry->scheduledBehind == NULL && entry->scheduledBefore == NULL) );
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
    vals.push_back(req->entryReadCnt);
    vals.push_back(req->entryWriteCnt);
    
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
        activatedAt[cpuID][bank] = curTick;
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
END_DECLARE_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)

    INIT_PARAM_DFLT(readqueue_size, "Max size of read queue", 64),
    INIT_PARAM_DFLT(writequeue_size, "Max size of write queue", 64),
    INIT_PARAM_DFLT(reserved_slots, "Number of activations reserved for reads", 2),
    INIT_PARAM_DFLT(inf_write_bw, "Infinite writeback bandwidth", false),
    INIT_PARAM_DFLT(page_policy, "Controller page policy", "ClosedPage"),
    INIT_PARAM_DFLT(priority_scheme, "Controller priority scheme", "FCFS")

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
                                            policy);
}

REGISTER_SIM_OBJECT("RDFCFSTimingMemoryController", RDFCFSTimingMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS
