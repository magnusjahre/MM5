/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/fcfspri_memory_controller.hh"

using namespace std;

FCFSPRITimingMemoryController::FCFSPRITimingMemoryController() { 
  cout << "FCFSPRI memory controller created" << endl;
  startedprewrite = false;
}

/** Frees locally allocated memory. */
FCFSPRITimingMemoryController::~FCFSPRITimingMemoryController(){
}

int FCFSPRITimingMemoryController::insertRequest(MemReqPtr &req) {
    MemReqPtr activate = new MemReq();
    activate->paddr = req->paddr;
    activate->cmd = Activate;
    MemReqPtr close = new MemReq();
    close->paddr = req->paddr;
    close->cmd = Close;
    if (req->cmd == Prewrite) {
      req->cmd = Writeback;
      if (!prewritebackQueue.empty()) {
          MemReqPtr& lastrequest = prewritebackQueue.back();
          assert(lastrequest->cmd == Close);
          if (req->paddr >> 10 == lastrequest->paddr >> 10) {
              //Same page
              prewritebackQueue.pop_back();
              prewritebackQueue.push_back(req);
              prewritebackQueue.push_back(close);
          } else {
              prewritebackQueue.push_back(activate);
              prewritebackQueue.push_back(req);
              prewritebackQueue.push_back(close);
          }
      } else {
          prewritebackQueue.push_back(activate);
          prewritebackQueue.push_back(req);
          prewritebackQueue.push_back(close);
      }
      if (prewritebackQueue.size() > 192) {
        setPrewriteBlocked();
      }
    } else {
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
    }



    return 0;
}

bool FCFSPRITimingMemoryController::hasMoreRequests() {
    if (memoryRequestQueue.empty() && prewritebackQueue.empty()) {
      return false;
    } else {
      return true;
    }
}

MemReqPtr& FCFSPRITimingMemoryController::getRequest() {
    if ((!memoryRequestQueue.empty()) && (!startedprewrite)) {
      MemReqPtr& tmp = memoryRequestQueue.front();
      memoryRequestQueue.pop_front();
      //cout << curTick << " issued from queue " << tmp->cmd << " : " << tmp->paddr << endl;
      if (isBlocked() && memoryRequestQueue.size() < 192) {
         setUnBlocked();
      }
      return tmp;
    } else {
      startedprewrite = true;
      MemReqPtr& tmp = prewritebackQueue.front();
      prewritebackQueue.pop_front();
      //cout << curTick << " issued from prewritequeue " << tmp->cmd << " : " << tmp->paddr << endl;
      if (isPrewriteBlocked() && prewritebackQueue.size() < 192) {
         setPrewriteUnBlocked();
      }
      if (tmp->cmd == Close) {
        startedprewrite = false;
      }
      return tmp;
    }
}


