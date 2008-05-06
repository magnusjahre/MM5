/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/time_mult_memory_controller.hh"

using namespace std;

TimeMultiplexedMemoryController::TimeMultiplexedMemoryController(std::string _name, int _queueLength)
    : TimingMemoryController(_name) {
    
    queueLength = _queueLength;
    
    activePage = 0;
    pageActivated = false;
    prevActivate = false;
    
    pageCmd = new MemReq();
    pageCmd->cmd = Activate;
    pageCmd->paddr = 0;
    
}

TimeMultiplexedMemoryController::~TimeMultiplexedMemoryController(){
}

int
TimeMultiplexedMemoryController::insertRequest(MemReqPtr &req) {

    fatal("insert request not implemented");
    
//     req->inserted_into_memory_controller = curTick;
//     memoryRequestQueue.push_back(req);
// 
//     if (!isBlocked() && memoryRequestQueue.size() >= queueLength) {
//        setBlocked();
//     }

    return 0;
}

bool
TimeMultiplexedMemoryController::hasMoreRequests() {
    return !memoryRequestQueue.empty();
}

MemReqPtr&
TimeMultiplexedMemoryController::getRequest() {
    
    MemReqPtr& retval = pageCmd; // dummy initialization
    
    fatal("getRequest not impl");
    
//     if(pageActivated){
//         if(isActive(memoryRequestQueue.front())){
//             retval = memoryRequestQueue.front();
//             memoryRequestQueue.pop_front();
//         }
//         else{
//             pageCmd->cmd = Close;
//             pageCmd->paddr = getPageAddr(activePage);
//             pageCmd->flags &= ~SATISFIED;
//             retval = pageCmd;
//             pageActivated = false;
//             activePage = 0;
//             
//         }
//     }
//     else{
//         // update internals
//         assert(activePage == 0);
//         activePage = getPage(memoryRequestQueue.front());
//         pageActivated = true;
//         prevActivate = true;
//         
//         // issue an activate
//         pageCmd->cmd = Activate;
//         pageCmd->paddr = getPageAddr(activePage);
//         pageCmd->flags &= ~SATISFIED;
//         retval = pageCmd;
//     }
//     
//     if(isBlocked() && memoryRequestQueue.size() < queueLength){
//         setUnBlocked();
//     }
    
    return retval;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(TimeMultiplexedMemoryController)
    Param<int> queue_size;
END_DECLARE_SIM_OBJECT_PARAMS(TimeMultiplexedMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(TimeMultiplexedMemoryController)
    INIT_PARAM_DFLT(queue_size, "Max queue size", 64)
END_INIT_SIM_OBJECT_PARAMS(TimeMultiplexedMemoryController)


CREATE_SIM_OBJECT(TimeMultiplexedMemoryController)
{
    return new TimeMultiplexedMemoryController(getInstanceName(),
                                          queue_size);
}

REGISTER_SIM_OBJECT("TimeMultiplexedMemoryController", TimeMultiplexedMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS


