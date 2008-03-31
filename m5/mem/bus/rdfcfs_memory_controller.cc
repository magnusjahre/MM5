/**
 * @file
 * Describes the Memory Controller API.
 */

#include <sstream>

#include "mem/bus/rdfcfs_memory_controller.hh"

using namespace std;

RDFCFSTimingMemoryController::RDFCFSTimingMemoryController() { 
  num_active_pages = 0;
  max_active_pages = 4;

  close = new MemReq();
  close->cmd = Close;
  activate = new MemReq();
  activate->cmd = Activate;

  total_read_wait = 0;
  total_write_wait = 0;
  total_prewrite_wait = 0;

  reads = 0;
  writes = 0;
  prewrites = 0;

  invoked = false;

  avg_read_size = 0;
  avg_write_size = 0;
  avg_prewrite_size = 0;

  last_invoke = 0;

}

/** Frees locally allocated memory. */
RDFCFSTimingMemoryController::~RDFCFSTimingMemoryController(){
}

int RDFCFSTimingMemoryController::insertRequest(MemReqPtr &req) {
  // Stats
  avg_read_size += readQueue.size() * (curTick - last_invoke);
  avg_write_size += writeQueue.size() * (curTick - last_invoke);
  avg_prewrite_size += prewritebackQueue.size() * (curTick - last_invoke);

  last_invoke = curTick;

  req->inserted_into_memory_controller = curTick;
  if (req->cmd == Read) {
    readQueue.push_back(req);
    if (readQueue.size() >= readqueue_size) {
      setBlocked();
    }
  }
  if (req->cmd == Write) {
    writeQueue.push_back(req);
    if (writeQueue.size() >= writequeue_size) {
      setBlocked();
    }
  }
  if (req->cmd == Prewrite) {
    req->cmd = Writeback;
    prewritebackQueue.push_back(req);
    if (prewritebackQueue.size() >= prewritequeue_size) {
      setPrewriteBlocked();
    }
  }
  // Early activation of reads
  if ((req->cmd == Read) && (num_active_pages < max_active_pages)) {
    if (!isActive(req) && bankIsClosed(req)) {
      // Activate that page
      activate->paddr = req->paddr;
      activate->flags &= ~SATISFIED;
      activePages.push_back(getPage(req));
      num_active_pages++;
      assert(mem_interface->calculateLatency(activate) == 0);
    }
  }
  
  return 0;
}

bool RDFCFSTimingMemoryController::hasMoreRequests() {
    if (readQueue.empty() && writeQueue.empty() && prewritebackQueue.empty() && (num_active_pages == 0)) {
      return false;
    } else {
      return true;
    }
}

MemReqPtr& RDFCFSTimingMemoryController::getRequest() {
  //workQueue.clear();
  //workQueue.splice(workQueue.end(), readQueue);
  //workQueue.splice(workQueue.end(), writeQueue);
  //workQueue.splice(workQueue.end(), prewritebackQueue);
    
  avg_read_size += readQueue.size() * (curTick - last_invoke);
  avg_write_size += writeQueue.size() * (curTick - last_invoke);
  avg_prewrite_size += prewritebackQueue.size() * (curTick - last_invoke);
  last_invoke = curTick;

  if (num_active_pages < max_active_pages) { 
    // Go through all lists to see if we can activate anything
    for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
      MemReqPtr& tmp = *queueIterator;
      if (!isActive(tmp) && bankIsClosed(tmp)) {
        //Request is not active and bank is closed. Activate it
        activate->paddr = tmp->paddr;
        activate->flags &= ~SATISFIED;
        activePages.push_back(getPage(tmp));
        num_active_pages++;
        return (activate);
      }
    }        
    for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
      MemReqPtr& tmp = *queueIterator;
      if (!isActive(tmp) && bankIsClosed(tmp)) {
        //Request is not active and bank is closed. Activate it
        activate->paddr = tmp->paddr;
        activate->flags &= ~SATISFIED;
        activePages.push_back(getPage(tmp));
        num_active_pages++;
        return (activate);
      }
    }        
  }
  if (num_active_pages < reserved_slots) { // Very careful activation of prewritebacks.
    for (queueIterator = prewritebackQueue.begin(); queueIterator != prewritebackQueue.end(); queueIterator++) {
      MemReqPtr& tmp = *queueIterator;
      if (!isActive(tmp) && bankIsClosed(tmp)) {
        //Request is not active and bank is closed. Activate it
        activate->paddr = tmp->paddr;
        activate->flags &= ~SATISFIED;
        activePages.push_back(getPage(tmp));
        num_active_pages++;
        return (activate);
      }
    }        
  }
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
    for (queueIterator = prewritebackQueue.begin(); queueIterator != prewritebackQueue.end(); queueIterator++) {
      MemReqPtr& tmp = *queueIterator;
      if (getPage(tmp) == Active) {
        canClose = false;
      }
    }

    if (canClose) {
      close->paddr = Active << 10;
      close->flags &= ~SATISFIED;
      activePages.erase(pageIterator);
      num_active_pages--;
      return close;
    }
  }
  
  // Go through the active pages and find a ready operation
  for (pageIterator = activePages.begin(); pageIterator != activePages.end() ; pageIterator++) {
    for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
      MemReqPtr& tmp = *queueIterator;
      if (isReady(tmp)) {
        // Remove it
        reads++;
        total_read_wait += curTick - tmp->inserted_into_memory_controller;
        if(isBlocked() && readQueue.size() == readqueue_size) setUnBlocked();
        readQueue.erase(queueIterator);
        return tmp;
      }
    }
    for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
      MemReqPtr& tmp = *queueIterator;
      if (isReady(tmp)) {
        // Remove it
        writes ++;
        total_write_wait += curTick - tmp->inserted_into_memory_controller;
        if(isBlocked() && writeQueue.size() == writequeue_size) setUnBlocked();
        writeQueue.erase(queueIterator);
        return tmp;
      }
    }
    for (queueIterator = prewritebackQueue.begin(); queueIterator != prewritebackQueue.end(); queueIterator++) {
      MemReqPtr& tmp = *queueIterator;
      if (isReady(tmp)) {
        // Remove it
        prewrites++;
        total_prewrite_wait += curTick - tmp->inserted_into_memory_controller;
        if(isPrewriteBlocked() && prewritebackQueue.size() == prewritequeue_size) setPrewriteUnBlocked();
        prewritebackQueue.erase(queueIterator);
        return tmp;
      }
    }
  }
  // No ready operation, issue any active operation 
  for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
    MemReqPtr& tmp = *queueIterator;
    if (isActive(tmp)) {
      reads++;
      total_read_wait += curTick - tmp->inserted_into_memory_controller;
      if(isBlocked() && readQueue.size() == readqueue_size) setUnBlocked();
      readQueue.erase(queueIterator);
      return tmp;
    }
  }
  for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
    MemReqPtr& tmp = *queueIterator;
    if (isActive(tmp)) {
      writes ++;
      total_write_wait += curTick - tmp->inserted_into_memory_controller;
      if(isBlocked() && writeQueue.size() == writequeue_size) setUnBlocked();
      writeQueue.erase(queueIterator);
      return tmp;
    }
  }
  for (queueIterator = prewritebackQueue.begin(); queueIterator != prewritebackQueue.end(); queueIterator++) {
    MemReqPtr& tmp = *queueIterator;
    if (isActive(tmp)) {
      prewrites++;
      total_prewrite_wait += curTick - tmp->inserted_into_memory_controller;
      if(isPrewriteBlocked() && prewritebackQueue.size() == prewritequeue_size) setPrewriteUnBlocked();
      prewritebackQueue.erase(queueIterator);
      return tmp;
    }
  }

  fatal("This should never happen!");
  return close;
}

std::string RDFCFSTimingMemoryController::dumpstats()
{
  // Drop first stat. Fastfwd will corrupt this one

  if (!invoked) {
    invoked = true;
    return ("");
  }

  
  int avg, avg_read, avg_writes, avg_prewrites;
  
  if (reads+writes+prewrites >0) {
    avg = (total_read_wait+total_write_wait+total_prewrite_wait)/(reads+writes+prewrites);
  } else {
    avg = 0;
  }
  
  if (reads >0) {
    avg_read = total_read_wait/reads;
  } else {
    avg_read = 0;
  }

  if (writes >0) {
    avg_writes = total_write_wait/writes;
  } else {
    avg_writes = 0;
  }

  if (prewrites > 0 ) {
    avg_prewrites = total_prewrite_wait / prewrites;
  } else {
    avg_prewrites = 0;
  }
  
  

  Tick time = curTick - last_invoke_internal;

  std::stringstream out;
  
  out << avg << " " << avg_read << " " << avg_writes << " " << avg_prewrites;
  out << " " << avg_read_size / time << " " << avg_write_size/time << " " << avg_prewrite_size / time;
  out << " " << reads << " " << writes << " " << prewrites;
  // Reset for next time
  reads = 0;
  writes = 0;
  prewrites = 0;
  total_read_wait = 0;
  total_write_wait = 0;
  total_prewrite_wait = 0;
  avg_read_size = 0;
  avg_write_size = 0;
  avg_prewrite_size = 0;
  last_invoke_internal = curTick;
  return out.str();
}
