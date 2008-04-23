/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/fcfs_memory_controller.hh"

using namespace std;

FCFSTimingMemoryController::FCFSTimingMemoryController(std::string _name, int _queueLength)
    : TimingMemoryController(_name) {
    
    queueLength = _queueLength;
    
    activePage = 0;
    pageActivated = false;
    prevActivate = false;
    
    pageCmd = new MemReq();
    pageCmd->cmd = Activate;
    pageCmd->paddr = 0;
}

/** Frees locally allocated memory. */
FCFSTimingMemoryController::~FCFSTimingMemoryController(){
}

int FCFSTimingMemoryController::insertRequest(MemReqPtr &req) {

    req->inserted_into_memory_controller = curTick;
    memoryRequestQueue.push_back(req);

    if (!isBlocked() && memoryRequestQueue.size() >= queueLength) {
       setBlocked();
    }

    return 0;
}

bool FCFSTimingMemoryController::hasMoreRequests() {
    return !memoryRequestQueue.empty();
}

MemReqPtr& FCFSTimingMemoryController::getRequest() {
    
    MemReqPtr& retval = pageCmd; // dummy initialization
    
    if(pageActivated){
        if(isActive(memoryRequestQueue.front())){
            retval = memoryRequestQueue.front();
            memoryRequestQueue.pop_front();
        }
        else{
            pageCmd->cmd = Close;
            pageCmd->paddr = getPageAddr(activePage);
            pageCmd->flags &= ~SATISFIED;
            retval = pageCmd;
            pageActivated = false;
            activePage = 0;
            
        }
    }
    else{
        // update internals
        assert(activePage == 0);
        activePage = getPage(memoryRequestQueue.front());
        pageActivated = true;
        prevActivate = true;
        
        // issue an activate
        pageCmd->cmd = Activate;
        pageCmd->paddr = getPageAddr(activePage);
        pageCmd->flags &= ~SATISFIED;
        retval = pageCmd;
    }
    
    if(isBlocked() && memoryRequestQueue.size() < queueLength){
        setUnBlocked();
    }
    
    return retval;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(FCFSTimingMemoryController)
    Param<int> queue_size;
END_DECLARE_SIM_OBJECT_PARAMS(FCFSTimingMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(FCFSTimingMemoryController)
    INIT_PARAM_DFLT(queue_size, "Max queue size", 64)
END_INIT_SIM_OBJECT_PARAMS(FCFSTimingMemoryController)


CREATE_SIM_OBJECT(FCFSTimingMemoryController)
{
    return new FCFSTimingMemoryController(getInstanceName(),
                                          queue_size);
}

REGISTER_SIM_OBJECT("FCFSTimingMemoryController", FCFSTimingMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS


