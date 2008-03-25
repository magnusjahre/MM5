/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/fcfsrw_memory_controller.hh"

using namespace std;

FCFSRWTimingMemoryController::FCFSRWTimingMemoryController() { 
  cout << "FCFS (Read over writes) memory controller created" << endl;
  openpage = 0;
  pageclosed = true;
  close = new MemReq();
  close->cmd = Close;
  activate = new MemReq();
  activate->cmd = Activate;
}

/** Frees locally allocated memory. */
FCFSRWTimingMemoryController::~FCFSRWTimingMemoryController(){
}

int FCFSRWTimingMemoryController::insertRequest(MemReqPtr &req) {
    if (req->cmd == Prewrite) {
        req->cmd = Writeback;
    }

    if (req->cmd == Read) {
      memoryReadQueue.push_back(req);
    } else {
      memoryWriteQueue.push_back(req);
    }
    
    if (memoryReadQueue.size() > 60) {
       cout << curTick << " :  memoryReadQueue.size()= " << memoryReadQueue.size() << endl;
    }
    
    return 0;
}

bool FCFSRWTimingMemoryController::hasMoreRequests() {
    // Check if both ques are empty and the page is closed.
    if (memoryReadQueue.empty() && memoryWriteQueue.empty()) {
      return false;
    } else {
      return true;
    }
}

MemReqPtr& FCFSRWTimingMemoryController::getRequest() {
 
    if (!memoryReadQueue.empty()) {
      current = memoryReadQueue.front();
    } else {
      current = memoryWriteQueue.front();
    }

    assert(current);

    // Page is closed
    if (pageclosed) {
      activate->paddr = current->paddr;
      activate->flags &= ~SATISFIED;
      openpage = current->paddr >> 10;
      pageclosed = false;
      return activate;
    }

    if (openpage != (current->paddr >> 10)) {
      // Close page first
      close->paddr  = openpage << 10;
      close->flags &= ~SATISFIED;
      pageclosed = true;
      return close;
    }

    // Hit an open page, just return the right one
    if (memoryReadQueue.empty()) {
      memoryWriteQueue.pop_front();
    } else {
      memoryReadQueue.pop_front();
    }
    return current;
    
}


