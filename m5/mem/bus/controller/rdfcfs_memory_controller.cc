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

    close = new MemReq();
    close->cmd = Close;
    activate = new MemReq();
    activate->cmd = Activate;

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
        
        pageResultTraces[i].initalizeTrace(params);
    }
#endif
}

/** Frees locally allocated memory. */
RDFCFSTimingMemoryController::~RDFCFSTimingMemoryController(){
}

int RDFCFSTimingMemoryController::insertRequest(MemReqPtr &req) {
    
    req->inserted_into_memory_controller = curTick;
    
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

MemReqPtr& RDFCFSTimingMemoryController::getRequest() {

    MemReqPtr& retval = activate; //dummy initialization
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
        if(readyFound) DPRINTF(MemoryController, "Found ready request, cmd %s addr %d bank %d\n", retval->cmd, retval->paddr, getMemoryBankID(retval->paddr));
        assert(!bankIsClosed(retval));
    }
    
    if(!activateFound && !closeFound && !readyFound){
        
        bool otherFound = getOther(retval);
        if(otherFound) DPRINTF(MemoryController, "Found other request, cmd %s addr %d bank %d\n", retval->cmd, retval->paddr, getMemoryBankID(retval->paddr));
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
RDFCFSTimingMemoryController::computeInterference(MemReqPtr& req, Tick busOccupiedFor){
    
    //FIXME: how should additional L2 misses be handled
    
#ifdef DO_ESTIMATION_TRACE
    bool isConflict = false;
    bool isHit = false;
#endif
    
    assert(req->cmd == Read || req->cmd == Writeback);
    
    checkPrivateOpenPage(req);
    
    req->busDelay = 0;
    Tick privateLatencyEstimate = 0;
    if(isPageHitOnPrivateSystem(req)){
        privateLatencyEstimate = 40;
#ifdef DO_ESTIMATION_TRACE
        if(req->memCtrlIssuePosition < 1) isHit = true;
        else isConflict = true;
#endif
    }
    else if(isPageConflictOnPrivateSystem(req)){
        if(req->cmd == Read) privateLatencyEstimate = 152;
        else privateLatencyEstimate = 135;
#ifdef DO_ESTIMATION_TRACE
        isConflict = true;
#endif
    }
    else{
        if(req->cmd == Read) privateLatencyEstimate = 108;
        else privateLatencyEstimate = 79;
    }
    
    updatePrivateOpenPage(req);
    
#ifdef DO_ESTIMATION_TRACE
    
    vector<RequestTraceEntry> vals;
    vals.push_back(RequestTraceEntry(req->paddr));
    vals.push_back(RequestTraceEntry(getMemoryBankID(req->paddr)));
    
    if(isConflict) vals.push_back(RequestTraceEntry("conflict"));
    else if(isHit) vals.push_back(RequestTraceEntry("hit"));
    else vals.push_back(RequestTraceEntry("miss"));
    
    vals.push_back(RequestTraceEntry(req->inserted_into_memory_controller));
    vals.push_back(RequestTraceEntry(req->oldAddr == MemReq::inval_addr ? 0 : req->oldAddr ));
    vals.push_back(RequestTraceEntry(req->memCtrlIssuePosition));
    
    pageResultTraces[req->adaptiveMHASenderID].addTrace(vals);

#endif
    
    int fromCPU = req->adaptiveMHASenderID;
    list<MemReqPtr>::iterator readIter;
    assert(fromCPU != -1);
    
    readIter = readQueue.begin();
    for( ; readIter != readQueue.end(); readIter++){
        MemReqPtr waitingReq = *readIter;
        assert(waitingReq->adaptiveMHASenderID != -1);
        assert(waitingReq->cmd == Read);
        
        if(waitingReq->adaptiveMHASenderID == fromCPU){
            assert(req->cmd == Read || req->cmd == Writeback);
            if(req->cmd == Read){
                waitingReq->busAloneReadQueueEstimate += privateLatencyEstimate;
            }
            else{
                waitingReq->busAloneWriteQueueEstimate += privateLatencyEstimate;
                waitingReq->waitWritebackCnt++;
            }
        }
    }
    
    if(req->cmd == Read){
        req->busAloneServiceEstimate += privateLatencyEstimate;
    }
}

void
RDFCFSTimingMemoryController::checkPrivateOpenPage(MemReqPtr& req){
    if(activatedPages.empty()) initializePrivateStorage();
    
    int actCnt = 0;
    Tick minAct = TICK_T_MAX;
    int minID = -1;
    int bank = getMemoryBankID(req->paddr);
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
    
    assert(!closedPagePolicy);
    
    Addr curPage = getPage(req->paddr);
    int bank = getMemoryBankID(req->paddr);
    int cpuID = req->adaptiveMHASenderID;
    
    return curPage == activatedPages[cpuID][bank];
}

bool 
RDFCFSTimingMemoryController::isPageConflictOnPrivateSystem(MemReqPtr& req){
    
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
