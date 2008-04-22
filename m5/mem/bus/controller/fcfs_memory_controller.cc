/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/fcfs_memory_controller.hh"

using namespace std;

FCFSTimingMemoryController::FCFSTimingMemoryController(std::string _name, int _queueLength)
    : TimingMemoryController(_name) {
    cout << "sweet FCFS mem-ctrl " << _name << " with length " << _queueLength << "\n";
    //FIXME: blocking code must be improved
    fatal("FCFS blocking does not work");
}

/** Frees locally allocated memory. */
FCFSTimingMemoryController::~FCFSTimingMemoryController(){
}

int FCFSTimingMemoryController::insertRequest(MemReqPtr &req) {
    MemReqPtr activate = new MemReq();
    activate->paddr = req->paddr;
    activate->cmd = Activate;
    MemReqPtr close = new MemReq();
    close->paddr = req->paddr;
    close->cmd = Close;
    if (req->cmd == Prewrite) {
        req->cmd = Writeback;
    }

    if (!memoryRequestQueue.empty()) {
        MemReqPtr& lastrequest = memoryRequestQueue.back();
        assert(lastrequest->cmd == Close);
        if (req->paddr >> 10 == lastrequest->paddr >> 10) {
            //Same page
            memoryRequestQueue.pop_back();
            memoryRequestQueue.push_back(req);
            memoryRequestQueue.push_back(close);
        } else {
            memoryRequestQueue.push_back(activate);
            memoryRequestQueue.push_back(req);
            memoryRequestQueue.push_back(close);
        }
    } else {
        memoryRequestQueue.push_back(activate);
        memoryRequestQueue.push_back(req);
        memoryRequestQueue.push_back(close);
    }

    if (memoryRequestQueue.size() > 192) {
       setBlocked();
    }

    return 0;
}

bool FCFSTimingMemoryController::hasMoreRequests() {
    return !memoryRequestQueue.empty();
}

MemReqPtr& FCFSTimingMemoryController::getRequest() {
    assert(!memoryRequestQueue.empty());
    MemReqPtr& tmp = memoryRequestQueue.front();
    memoryRequestQueue.pop_front();
    if (isBlocked() && memoryRequestQueue.size() < 60) {
       setUnBlocked();
    }
    return tmp;
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


