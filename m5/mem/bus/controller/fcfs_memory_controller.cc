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
    
    if(!takeOverActiveList.empty()){
        
        // taking over from other scheduler, close all active pages
        pageCmd->cmd = Close;
        pageCmd->paddr = getPageAddr(takeOverActiveList.front());
        takeOverActiveList.pop_front();
        pageCmd->flags &= ~SATISFIED;
        retval = pageCmd;
        pageActivated = false;
        activePage = 0;
        
        DPRINTF(MemoryController, "Closing active page due to takeover, cmd %s, addr %x\n", pageCmd->cmd, pageCmd->paddr);
    
    }
    else{
        
        // common case scheduling
        
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
            MemReqPtr req = memoryRequestQueue.front();
            activePage = getPage(req);
            pageActivated = true;
            prevActivate = true;
            
            // issue an activate
            currentActivationAddress(req->adaptiveMHASenderID, req->paddr, getMemoryBankID(req->paddr));
            pageCmd->cmd = Activate;
            pageCmd->paddr = getPageAddr(activePage);
            pageCmd->flags &= ~SATISFIED;
            retval = pageCmd;
        }
    }
    
    if(isBlocked() && memoryRequestQueue.size() < queueLength){
        setUnBlocked();
    }
    
    DPRINTF(MemoryController, "Returning memory request, cmd %s, addr %x\n", retval->cmd, retval->paddr);
    
    if(retval->cmd == Read || retval->cmd == Writeback){
        bus->updatePerCPUAccessStats(retval->adaptiveMHASenderID,
                                     isPageHit(retval->paddr, getMemoryBankID(retval->paddr)));
    }
    
    return retval;
}

void
FCFSTimingMemoryController::setOpenPages(std::list<Addr> pages){
    assert(takeOverActiveList.empty());
    takeOverActiveList.splice(takeOverActiveList.begin(), pages);
    DPRINTF(MemoryController, "Recieved take over list\n");
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


