/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/nfq_memory_controller.hh"

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
        setBlocked();
    }
    
    return 0;
}

Tick
NFQMemoryController::getMinStartTag(){
    Tick min = 10000000000;
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
        if(!requests.empty()) allEmpty = false;
    }
    
    if(!allEmpty) return true;
    
    for(int i=0;i<virtualFinishTimes.size();i++){
        virtualFinishTimes[i] = 0;
    }
    DPRINTF(NFQController, "No more requests, setting all finish times to 0");
    return false;
}

MemReqPtr&
NFQMemoryController::getRequest() {
    
    MemReqPtr& retval = pageCmd; // dummy initialization
    
    // 1. Prioritize ready pages
    
    // 2. Prioritize CAS commands
    
    // 3. Prioritize commands based on start tags
    
    return retval;
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


