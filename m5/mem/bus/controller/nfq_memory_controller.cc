/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/nfq_memory_controller.hh"

#define LARGE_TICK 10000000000
#define MAX_ACTIVE_PAGES 4

using namespace std;

NFQMemoryController::NFQMemoryController(std::string _name, 
                                         int _rdQueueLength,
                                         int _wrQueueLength,
                                         int _spt,
                                         int _numCPUs,
                                         int _processorPriority,
                                         int _writePriority)
    : TimingMemoryController(_name) {
    
    readQueueLength = _rdQueueLength;
    writeQueueLenght = _wrQueueLength;
    starvationPreventionThreshold = _spt;
    
    nfqNumCPUs = _numCPUs;
    processorPriority = _processorPriority;
    writebackPriority = _writePriority;
    
    if(processorPriority < writebackPriority){
        processorInc = writebackPriority / processorPriority;
        writebackInc = 1;
    }
    else{
        processorInc = 1;
        writebackInc = processorPriority / writebackPriority;
    }
    
    virtualFinishTimes.resize(nfqNumCPUs+1, 0);
    requests.resize(nfqNumCPUs+1);
    
    pageCmd = new MemReq();
    pageCmd->cmd = Activate;
    pageCmd->paddr = 0;
    
    queuedReads = 0;
    queuedWrites = 0;
    starvationCounter = 0;
}

NFQMemoryController::~NFQMemoryController(){
}

int
NFQMemoryController::insertRequest(MemReqPtr &req) {

    assert(req->cmd == Read || req->cmd == Writeback);
    req->cmd == Read ? queuedReads++ : queuedWrites++;
    
    Tick minTag = getMinStartTag();
    Tick curFinTag = -1;
    if(req->adaptiveMHASenderID >= 0){
        curFinTag = virtualFinishTimes[req->adaptiveMHASenderID];
    }
    else{
        curFinTag = virtualFinishTimes[nfqNumCPUs];
    }
    
    // assign start time and update virtual clock
    req->virtualStartTime = (minTag > curFinTag ? minTag : curFinTag);
    if(req->adaptiveMHASenderID >= 0){
        virtualFinishTimes[req->adaptiveMHASenderID] += processorInc;
    }
    else{
        assert(req->cmd == Writeback);
        virtualFinishTimes[nfqNumCPUs] += writebackInc;
    }
    
    DPRINTF(NFQController, 
            "Inserting request from cpu %d, addr %x, start time is %d, minimum time is %d\n",
            req->adaptiveMHASenderID, 
            req->paddr, 
            req->virtualStartTime, 
            minTag);
    
    if(req->adaptiveMHASenderID >= 0){
        assert(req->adaptiveMHASenderID < nfqNumCPUs);
        requests[req->adaptiveMHASenderID].push_back(req);
        
    }
    else{
        requests[nfqNumCPUs].push_back(req);
    }
    
    if((queuedReads > readQueueLength || queuedWrites > writeQueueLenght) && !isBlocked() ){
        DPRINTF(NFQController, "Blocking, #reads %d, #writes %d\n", queuedReads, queuedWrites);
        setBlocked();
    }
    
    return 0;
}

Tick
NFQMemoryController::getMinStartTag(){
    Tick min = LARGE_TICK;
    bool allEmpty = true;
    for(int i=0;i<requests.size();i++){
        for(int j=0;j<requests[i].size();j++){
            if(requests[i][j]->virtualStartTime < min){
                min = requests[i][j]->virtualStartTime;
            }
            allEmpty = false;
        }
    }
    if(allEmpty) return 0;
    return min;
}

bool
NFQMemoryController::hasMoreRequests() {
    
    bool allEmpty = true;
    
    for(int i=0;i<requests.size();i++){
        if(!requests[i].empty()) allEmpty = false;
    }
    
    if(!allEmpty){
        return true;
    }
    
    for(int i=0;i<virtualFinishTimes.size();i++){
        virtualFinishTimes[i] = 0;
    }
    
    DPRINTF(NFQController, "No more requests, setting all finish times to 0\n");
    return false;
}

MemReqPtr&
NFQMemoryController::getRequest() {
    
    MemReqPtr& retval = pageCmd; // dummy initialization
    
    bool foundReady = false;
    bool foundColumn = false;
    
    if(starvationCounter < starvationPreventionThreshold){
        // 1. Prioritize ready pages
        foundReady = findColumnRequest(retval, &NFQMemoryController::isActive);
        
        // 2. Prioritize CAS commands
        if(!foundReady){
            foundColumn = findColumnRequest(retval, &NFQMemoryController::pageActivated);
        }
    }
    else{
        DPRINTF(NFQController,
                "Bypassing ready check, starvation counter above threshold (cnt=%d, thres=%d)\n",
               starvationCounter,
               starvationPreventionThreshold);
    }
    
    bool foundRow = false;
    if(!foundReady && !foundColumn){
        // 3. Prioritize commands based on start tags
        foundRow = findRowRequest(retval);
        assert(foundRow);
        if(retval->cmd == Read || retval->cmd == Writeback){
            DPRINTF(NFQController, "Resetting starvation counter after bypass\n");
            starvationCounter = 0;
        }
    }
    else{
        //a request may have passed an older request
        Tick oldest = getMinStartTag();
        if(oldest < retval->virtualStartTime){
            starvationCounter++;
            DPRINTF(NFQController, "Incrementing starvation counter, value is now %d\n", starvationCounter);
        }
        else{
            assert(retval->virtualStartTime <= oldest);
            starvationCounter = 0;
            DPRINTF(NFQController, 
                    "Resetting starvation counter (req at %d, lowest %d)\n",
                    retval->virtualStartTime,
                    oldest);
        }
    }
    
    return retval;
}

bool
NFQMemoryController::findColumnRequest(MemReqPtr& req, 
                                       bool (NFQMemoryController::*compare)(MemReqPtr&)){
    
    Tick minval = LARGE_TICK;
    int minrow = -1;
    int mincol = -1;
    
    for(int i=0;i<requests.size();i++){
        for(int j=0;j<requests[i].size();j++){
            if( (this->*compare)(requests[i][j]) ){
                if(requests[i][j]->virtualStartTime < minval){
                    minval = requests[i][j]->virtualStartTime;
                    minrow = i;
                    mincol = j;
                }
            }
        }
    }
    
    if(minrow != -1 && mincol != -1){
        req = prepareColumnRequest(requests[minrow][mincol]);
        requests[minrow].erase(requests[minrow].begin()+mincol);
        return true;
    }
    
    return false;
}

bool 
NFQMemoryController::pageActivated(MemReqPtr& req){
    assert(activePages.size() < MAX_ACTIVE_PAGES);
    
    for(int i=0;i<activePages.size();i++){
        if(activePages[i] == getPage(req)){
            return true;
        }
    }
    return false;
}

bool
NFQMemoryController::findRowRequest(MemReqPtr& req){
    
    Tick minval = LARGE_TICK;
    int minrow = -1;
    int mincol = -1;
    
    for(int i=0;i<requests.size();i++){
        for(int j=0;j<requests[i].size();j++){
            if(requests[i][j]->virtualStartTime < minval){
                minval = requests[i][j]->virtualStartTime;
                minrow = i;
                mincol = j;
            }
        }
    }
    
    assert(minrow >= 0 && mincol >= 0);
    MemReqPtr oldestReq = requests[minrow][mincol];
    if(pageActivated(oldestReq)){
        // issuing oldest column command because starvation prevention threshold has been reached
        assert(starvationCounter >= starvationPreventionThreshold);
        req = prepareColumnRequest(oldestReq);
        requests[minrow].erase(requests[minrow].begin()+mincol);
        return true;
    }
    
    // check for pages that can be closed
    for(int i=0;i<activePages.size();i++){
        bool canClose = true;
        for(int j=0;j<requests.size();j++){
            for(int k=0;k<requests[j].size();k++){
                if(activePages[i] == getPageAddr(requests[j][k])){
                    canClose = false;
                }
            }
        }
        if(canClose){
            req = createCloseReq(activePages[i]);
            activePages.erase(activePages.begin()+i);
            
            DPRINTF(NFQController, 
                    "Closing page addr %x because it has no pending requests\n",
                    req->paddr);
            
            return true;
        }
    }
    
    if(activePages.size() >= MAX_ACTIVE_PAGES){
        // a page must be closed before we can issue the request
        // close the page that has been active for the longest time
        req = createCloseReq(activePages[0]);
        activePages.erase(activePages.begin());
        DPRINTF(NFQController, 
                "Closing page addr %x to issue oldest request\n",
                req->paddr);
        
    }
    else{
        req = createActivateReq(oldestReq);
        activePages.push_back(getPage(oldestReq));
        DPRINTF(NFQController, 
                "Activating page addr %x, %d pages are currently active\n",
                req->paddr,
                activePages.size());
    }
    
    return true;
}

MemReqPtr&
NFQMemoryController::createCloseReq(Addr pageAddr){
    pageCmd->cmd = Close;
    pageCmd->paddr = getPageAddr(pageAddr);
    pageCmd->flags &= ~SATISFIED;
    return pageCmd;
}
        
MemReqPtr&
NFQMemoryController::createActivateReq(MemReqPtr& req){
    Addr pageAddr = getPage(req);
    pageCmd->cmd = Activate;
    pageCmd->paddr = getPageAddr(pageAddr);
    pageCmd->flags &= ~SATISFIED;
    return pageCmd;
}

MemReqPtr&
NFQMemoryController::prepareColumnRequest(MemReqPtr& req){
    assert(req->cmd == Read || req->cmd == Writeback);
    req->cmd == Writeback ? queuedWrites-- : queuedReads--;
        
    if((queuedReads <= readQueueLength || queuedWrites <= writeQueueLenght) && isBlocked() ){
        DPRINTF(NFQController, "Unblocking, #reads %d, #writes %d\n", queuedReads, queuedWrites);
        setUnBlocked();
    }
        
    DPRINTF(NFQController, 
            "Returning column request, start time %d, addr %x, cpu %d\n",
            req->virtualStartTime,
            req->paddr,
            req->adaptiveMHASenderID);
    
    return req;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(NFQMemoryController)
    Param<int> rd_queue_size;
    Param<int> wr_queue_size;
    Param<int> starvation_prevention_thres;
    Param<int> num_cpus;
    Param<int> processor_priority;
    Param<int> writeback_priority;
END_DECLARE_SIM_OBJECT_PARAMS(NFQMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(NFQMemoryController)
    INIT_PARAM_DFLT(rd_queue_size, "Max read queue size", 64),
    INIT_PARAM_DFLT(wr_queue_size, "Max write queue size", 64),
    INIT_PARAM_DFLT(starvation_prevention_thres, "Starvation Prevention Threshold", 1),
    INIT_PARAM_DFLT(num_cpus, "Number of CPUs", -1),
    INIT_PARAM_DFLT(processor_priority, "The priority given to writeback requests", -1),
    INIT_PARAM_DFLT(writeback_priority, "The priority given to writeback requests", -1)
END_INIT_SIM_OBJECT_PARAMS(NFQMemoryController)


CREATE_SIM_OBJECT(NFQMemoryController)
{
    return new NFQMemoryController(getInstanceName(),
                                   rd_queue_size,
                                   wr_queue_size,
                                   starvation_prevention_thres,
                                   num_cpus,
                                   processor_priority,
                                   writeback_priority);
}

REGISTER_SIM_OBJECT("NFQMemoryController", NFQMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS


