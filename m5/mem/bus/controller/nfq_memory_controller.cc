/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/nfq_memory_controller.hh"

using namespace std;

NFQMemoryController::NFQMemoryController(std::string _name, 
                                         int _rdQueueLength,
                                         int _wrQueueLength,
                                         int _spt)
    : TimingMemoryController(_name) {
    
    readQueueLength = _rdQueueLength;
    writeQueueLenght = _wrQueueLength;
    starvationPreventionThreshold = _spt;
    
    pageCmd = new MemReq();
    pageCmd->cmd = Activate;
    pageCmd->paddr = 0;
    
}

NFQMemoryController::~NFQMemoryController(){
}

int
NFQMemoryController::insertRequest(MemReqPtr &req) {

    fatal("insert request not implemented");

    return 0;
}

bool
NFQMemoryController::hasMoreRequests() {
    return !memoryRequestQueue.empty();
}

MemReqPtr&
NFQMemoryController::getRequest() {
    
    MemReqPtr& retval = pageCmd; // dummy initialization
    
    fatal("getRequest not impl");
    
    return retval;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(NFQMemoryController)
    Param<int> rd_queue_size;
    Param<int> wr_queue_size;
    Param<int> starvation_prevention_thres;
END_DECLARE_SIM_OBJECT_PARAMS(NFQMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(NFQMemoryController)
    INIT_PARAM_DFLT(rd_queue_size, "Max read queue size", 64),
    INIT_PARAM_DFLT(wr_queue_size, "Max write queue size", 64),
    INIT_PARAM_DFLT(starvation_prevention_thres, "Starvation Prevention Threshold", 1)
END_INIT_SIM_OBJECT_PARAMS(NFQMemoryController)


CREATE_SIM_OBJECT(NFQMemoryController)
{
    return new NFQMemoryController(getInstanceName(),
                                   rd_queue_size,
                                   wr_queue_size,
                                   starvation_prevention_thres);
}

REGISTER_SIM_OBJECT("NFQMemoryController", NFQMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS


