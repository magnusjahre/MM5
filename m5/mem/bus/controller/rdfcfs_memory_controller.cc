/**
 * @file
 * Describes the Memory Controller API.
 */

#include <sstream>

#include "mem/bus/controller/rdfcfs_memory_controller.hh"

using namespace std;

RDFCFSTimingMemoryController::RDFCFSTimingMemoryController(std::string _name,
                                                           int _readqueue_size,
                                                           int _writequeue_size,
                                                           int _reserved_slots)
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

}

/** Frees locally allocated memory. */
RDFCFSTimingMemoryController::~RDFCFSTimingMemoryController(){
}

int RDFCFSTimingMemoryController::insertRequest(MemReqPtr &req) {
    
    req->inserted_into_memory_controller = curTick;
    if (req->cmd == Read) {
        readQueue.push_back(req);
        if (readQueue.size() > readqueue_size) { // full queue + one in progress
            setBlocked();
        }
    }
    if (req->cmd == Writeback) {
        writeQueue.push_back(req);
        if (writeQueue.size() > writequeue_size) { // full queue + one in progress
            setBlocked();
        }
    }
    
    assert(req->cmd == Read || req->cmd == Writeback);
    
    return 0;
}

bool RDFCFSTimingMemoryController::hasMoreRequests() {
    
    if (readQueue.empty() && writeQueue.empty() && (num_active_pages == 0)) {
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
        if(activateFound) DPRINTF(MemoryController, "Found activate request, cmd %s addr %x\n", retval->cmd, retval->paddr);
    }

    if(!activateFound){
        closeFound = getClose(retval);
        if(closeFound) DPRINTF(MemoryController, "Found close request, cmd %s addr %x\n", retval->cmd, retval->paddr);
    }
    
    if(!activateFound && !closeFound){
        readyFound = getReady(retval);
        if(readyFound) DPRINTF(MemoryController, "Found ready request, cmd %s addr %x\n", retval->cmd, retval->paddr);
        assert(!bankIsClosed(retval));
    }
    
    if(!activateFound && !closeFound && !readyFound){
        bool otherFound = getOther(retval);
        if(otherFound) DPRINTF(MemoryController, "Found other request, cmd %s addr %x\n", retval->cmd, retval->paddr);
        assert(otherFound);
        assert(!bankIsClosed(retval));
    }
    
    // Remove request from queue (if read or write)
    if(lastIssuedReq){
        if(lastIsWrite){
            writeQueue.remove(lastIssuedReq);
        }
        else{
            readQueue.remove(lastIssuedReq);
        }
    }
    
    // estimate interference caused by this request
    if(retval->cmd == Read || retval->cmd == Writeback){
        estimateInterference(retval);
        bus->updatePerCPUAccessStats(retval->adaptiveMHASenderID,
                                     isPageHit(retval->paddr, getMemoryBankID(retval->paddr)));
    }
    
    DPRINTF(MemoryController, "Returning command %s, addr %x\n", retval->cmd, retval->paddr);
    
    return retval;
}

bool
RDFCFSTimingMemoryController::getActivate(MemReqPtr& req){
    // Go through all lists to see if we can activate anything
    for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
        MemReqPtr& tmp = *queueIterator;
        if (!isActive(tmp) && bankIsClosed(tmp)) {
            //Request is not active and bank is closed. Activate it
            
            currentActivationAddress(tmp->adaptiveMHASenderID, tmp->paddr, getMemoryBankID(tmp->paddr));
            
            activate->cmd = Activate;
            activate->paddr = tmp->paddr;
            activate->flags &= ~SATISFIED;
            activePages.push_back(getPage(tmp));
            num_active_pages++;
                
            req = activate;
            return true;
        }
    } 
    for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
        MemReqPtr& tmp = *queueIterator;
        if (!isActive(tmp) && bankIsClosed(tmp)) {
            
            currentActivationAddress(tmp->adaptiveMHASenderID, tmp->paddr, getMemoryBankID(tmp->paddr));
            
            //Request is not active and bank is closed. Activate it
            activate->cmd = Activate;
            activate->paddr = tmp->paddr;
            activate->flags &= ~SATISFIED;
            activePages.push_back(getPage(tmp));
            num_active_pages++;
                
            req = activate;
            return true;
        }
    }
    return false;
}

bool 
RDFCFSTimingMemoryController::getClose(MemReqPtr& req){
    // Check if we can close the first page (eg, there is no active requests to this page
    for (pageIterator = activePages.begin(); pageIterator != activePages.end(); pageIterator++) {
        Addr Active = *pageIterator;
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
            req = close;
            return true;
        }
    }
    return false;
}

bool 
RDFCFSTimingMemoryController::getReady(MemReqPtr& req){
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
    
    return false;
}

bool
RDFCFSTimingMemoryController::getOther(MemReqPtr& req){
    // No ready operation, issue any active operation 
    for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
        MemReqPtr& tmp = *queueIterator;
        if (isActive(tmp)) {

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
        if (isActive(tmp)) {

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
    return false;
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
    assert(activePages.empty());
    activePages.splice(activePages.begin(), pages);
    num_active_pages = activePages.size();
    assert(num_active_pages <= max_active_pages);
    DPRINTF(MemoryController, "Recieved active list, there are now %d active pages\n", num_active_pages);
}

void
RDFCFSTimingMemoryController::estimateInterference(MemReqPtr& req){
    
    int localCpuCnt = bus->adaptiveMHA->getCPUCount();
    vector<vector<Tick> > interference(localCpuCnt, vector<Tick>(localCpuCnt, 0));
    vector<vector<bool> > delayedIsRead(localCpuCnt, vector<bool>(localCpuCnt, false));
    int fromCPU = req->adaptiveMHASenderID;
    
    if(fromCPU == -1) return;
    
    // Interference due to bus capacity
    vector<bool> readBusInterference = hasReadyRequestWaiting(req, readQueue);
    assert(readBusInterference.size() == localCpuCnt);
    for(int i=0;i<readBusInterference.size();i++){
        if(readBusInterference[i]){
            interference[i][req->adaptiveMHASenderID] += 20;
            delayedIsRead[i][req->adaptiveMHASenderID] = true;
        }
    }
    
    // Interference due to bank conflicts
    vector<int> readBankInterference = computeBankWaitingPara(req, readQueue);
    assert(readBankInterference.size() == localCpuCnt);
    for(int i=0;i<readBankInterference.size();i++){
        if(readBankInterference[i] > 0){
            interference[i][req->adaptiveMHASenderID] += 60 / readBankInterference[i];
            delayedIsRead[i][req->adaptiveMHASenderID] = true;
        }
    }
    
    // Interference due to page hits becoming page misses
    if(!isPageHit(req->paddr, getMemoryBankID(req->paddr)) 
        && isPageHitOnPrivateSystem(req->paddr, getMemoryBankID(req->paddr), req->adaptiveMHASenderID)){
        //NOTE: might want to add these to measurements
        bus->incInterferenceMisses();
    }
    
    // Detect constructive interference
    if(isPageHit(req->paddr, getMemoryBankID(req->paddr)) 
       && !isPageHitOnPrivateSystem(req->paddr, getMemoryBankID(req->paddr), req->adaptiveMHASenderID)){
        //NOTE: might want to include these in the measuerements
        bus->incConstructiveInterference();
    }
    
    bus->adaptiveMHA->addInterferenceDelay(interference,
                                           req->paddr,
                                           req->cmd,
                                           req->adaptiveMHASenderID,
                                           MEMORY_INTERFERENCE,
                                           delayedIsRead);
}

std::vector<bool> 
RDFCFSTimingMemoryController::hasReadyRequestWaiting(MemReqPtr& req, std::list<MemReqPtr>& queue){
    
    int localCpuCnt = bus->adaptiveMHA->getCPUCount();
    list<MemReqPtr>::iterator tmpIterator;
    vector<bool> retval = vector<bool>(localCpuCnt, false);
    
    for (tmpIterator = queue.begin(); tmpIterator != queue.end(); tmpIterator++) {
        MemReqPtr& tmp = *tmpIterator;
        
        if (req->adaptiveMHASenderID != tmp->adaptiveMHASenderID && tmp->adaptiveMHASenderID != -1) {
            if(isReady(tmp)){
                // interference on bus
                retval[tmp->adaptiveMHASenderID] = true;
            }
        }
    }
    return retval;
}

vector<int>
RDFCFSTimingMemoryController::computeBankWaitingPara(MemReqPtr& req, std::list<MemReqPtr>& queue){
    
    const int BANKS = 8;
    int localCpuCnt = bus->adaptiveMHA->getCPUCount();
    list<MemReqPtr>::iterator tmpIterator;
    vector<vector<bool> > waitingForBanks(localCpuCnt, vector<bool>(BANKS, false));
    vector<bool> waitingInSameBank = vector<bool>(localCpuCnt, false);
    vector<int> retval = vector<int>(localCpuCnt, 0);
    
    stringstream banktrace;
    
    for(tmpIterator = queue.begin();tmpIterator != queue.end();tmpIterator++){
        
        MemReqPtr& tmp = *tmpIterator;
        
        if (tmp->adaptiveMHASenderID != -1) {
            if(getMemoryBankID(tmp->paddr) != getMemoryBankID(req->paddr)){
                waitingForBanks[tmp->adaptiveMHASenderID][getMemoryBankID(tmp->paddr)] = true;
            }
            else{
                waitingInSameBank[tmp->adaptiveMHASenderID] = true;
            }
        }
    }
    
    for(int i=0;i<localCpuCnt;i++){
        if(waitingInSameBank[i] && i != req->adaptiveMHASenderID){
            for(int j=0;j<BANKS;j++){
                if(waitingForBanks[i][j]){
                    retval[i]++;
                }
            }
        }
    }
    
    // divide by 2
    for(int i=0;i<localCpuCnt;i++) retval[i] = retval[i] >> 1;
    
    return retval;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)

    Param<int> readqueue_size;
    Param<int> writequeue_size;
    Param<int> reserved_slots;
END_DECLARE_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)

    INIT_PARAM_DFLT(readqueue_size, "Max size of read queue", 64),
    INIT_PARAM_DFLT(writequeue_size, "Max size of write queue", 64),
    INIT_PARAM_DFLT(reserved_slots, "Number of activations reserved for reads", 2)

END_INIT_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)


CREATE_SIM_OBJECT(RDFCFSTimingMemoryController)
{
    return new RDFCFSTimingMemoryController(getInstanceName(),
                                            readqueue_size,
                                            writequeue_size,
                                            reserved_slots);
}

REGISTER_SIM_OBJECT("RDFCFSTimingMemoryController", RDFCFSTimingMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS

