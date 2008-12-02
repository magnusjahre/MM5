/**
 * @file
 * Describes the Memory Controller API.
 */

#include <sstream>

#include "mem/bus/controller/rdfcfs_memory_controller.hh"

// #define DO_ESTIMATION_TRACE 1
#define ESTIMATION_CPU_ID 3

#ifdef DO_ESTIMATION_TRACE
#include <fstream>
#endif

using namespace std;

RDFCFSTimingMemoryController::RDFCFSTimingMemoryController(std::string _name,
                                                           int _readqueue_size,
                                                           int _writequeue_size,
                                                           int _reserved_slots, 
                                                           bool _infinite_write_bw)
    : TimingMemoryController(_name) {
    
    num_active_pages = 0;
    max_active_pages = 4;
    
    readqueue_size = _readqueue_size;
    writequeue_size = _writequeue_size;
    reserved_slots = _reserved_slots;

    close = new MemReq();
    close->cmd = Close;
    activate = new MemReq();
    activate->cmd = Activate;

    lastIsWrite = false;
    
    infiniteWriteBW = _infinite_write_bw;
    
    currentDeliveredReqAt = 0;
    currentOccupyingCPUID = -1;
    
    lastDeliveredReqAt = 0;
    lastOccupyingCPUID = -1;
    
    equalReadWritePri = true; // FIXME: parameterize
    
#ifdef DO_ESTIMATION_TRACE
    ofstream ofile("estimation_access_trace.txt");
    ofile << "";
    ofile.flush();
    ofile.close();
#endif

}

/** Frees locally allocated memory. */
RDFCFSTimingMemoryController::~RDFCFSTimingMemoryController(){
}

int RDFCFSTimingMemoryController::insertRequest(MemReqPtr &req) {
    
    req->inserted_into_memory_controller = curTick;
    if (req->cmd == Read) {
        readQueue.push_back(req);
        if (readQueue.size() > readqueue_size) { // full queue + one in progress
            setBlocked();
        }
    }
    if (req->cmd == Writeback && !infiniteWriteBW) {
        writeQueue.push_back(req);
        if (writeQueue.size() > writequeue_size) { // full queue + one in progress
            setBlocked();
        }
    }
    
    assert(req->cmd == Read || req->cmd == Writeback);
    
    return 0;
}

bool RDFCFSTimingMemoryController::hasMoreRequests() {
    
    if (readQueue.empty() && writeQueue.empty() && (num_active_pages == 0)) {
      return false;
    } else {
      return true;
    }
}

MemReqPtr& RDFCFSTimingMemoryController::getRequest() {

    MemReqPtr& retval = activate; //dummy initialization
    bool activateFound = false;
    bool closeFound = false;
    bool readyFound = false;
    
    if (num_active_pages < max_active_pages) { 
        activateFound = getActivate(retval);
        if(activateFound) DPRINTF(MemoryController, "Found activate request, cmd %s addr %x\n", retval->cmd, retval->paddr);
    }

    if(!activateFound){
        closeFound = getClose(retval);
        if(closeFound) DPRINTF(MemoryController, "Found close request, cmd %s addr %x\n", retval->cmd, retval->paddr);
    }
    
    if(!activateFound && !closeFound){
        readyFound = getReady(retval);
        if(readyFound) DPRINTF(MemoryController, "Found ready request, cmd %s addr %x\n", retval->cmd, retval->paddr);
        assert(!bankIsClosed(retval));
    }
    
    if(!activateFound && !closeFound && !readyFound){
        bool otherFound = getOther(retval);
        if(otherFound) DPRINTF(MemoryController, "Found other request, cmd %s addr %x\n", retval->cmd, retval->paddr);
        assert(otherFound);
        assert(!bankIsClosed(retval));
    }
    
    // Remove request from queue (if read or write)
    if(lastIssuedReq->cmd == Read || lastIssuedReq->cmd == Writeback){
        if(lastIsWrite){
            writeQueue.remove(lastIssuedReq);
        }
        else{
            readQueue.remove(lastIssuedReq);
        }
    }
    
    // estimate interference caused by this request
    if((retval->cmd == Read || retval->cmd == Writeback) && !isShadow){
        assert(!isShadow);
//         estimateInterference(retval); // NOTE: kept because the FAMHA code might be used again
        
//         if(retval->cmd == Read && bus->adaptiveMHA->getCPUCount() > 1){
//             estimatePrivateServiceLatency(retval);
//         }
//         else{
//             retval->busDelay = 0;
//         }
        
        
        bus->updatePerCPUAccessStats(retval->adaptiveMHASenderID,
                                     isPageHit(retval->paddr, getMemoryBankID(retval->paddr)));
        
        // store the ID of the CPU that used the bus prevoiusly and update current vals
        lastDeliveredReqAt = currentDeliveredReqAt;
        lastOccupyingCPUID = currentOccupyingCPUID;
        currentDeliveredReqAt = curTick;
        currentOccupyingCPUID = retval->adaptiveMHASenderID;
    }
    
    DPRINTF(MemoryController, "Returning command %s, addr %x\n", retval->cmd, retval->paddr);
    
    return retval;
}

bool
RDFCFSTimingMemoryController::getActivate(MemReqPtr& req){
    
    if(equalReadWritePri){
        list<MemReqPtr> mergedQueue = mergeQueues();
        for (queueIterator = mergedQueue.begin(); queueIterator != mergedQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (!isActive(tmp) && bankIsClosed(tmp)) {
                //Request is not active and bank is closed. Activate it
                currentActivationAddress(tmp->adaptiveMHASenderID, tmp->paddr, getMemoryBankID(tmp->paddr));
                
                activate->cmd = Activate;
                activate->paddr = tmp->paddr;
                activate->flags &= ~SATISFIED;
                activePages.push_back(getPage(tmp));
                num_active_pages++;
                
                lastIssuedReq = activate;
                
                req = activate;
                return true;
            }
        }
    }
    else{
        // Go through all lists to see if we can activate anything
        for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (!isActive(tmp) && bankIsClosed(tmp)) {
                //Request is not active and bank is closed. Activate it
                
                currentActivationAddress(tmp->adaptiveMHASenderID, tmp->paddr, getMemoryBankID(tmp->paddr));
                
                activate->cmd = Activate;
                activate->paddr = tmp->paddr;
                activate->flags &= ~SATISFIED;
                activePages.push_back(getPage(tmp));
                num_active_pages++;
                    
                lastIssuedReq = activate;
                
                req = activate;
                return true;
            }
        } 
        for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (!isActive(tmp) && bankIsClosed(tmp)) {
                
                currentActivationAddress(tmp->adaptiveMHASenderID, tmp->paddr, getMemoryBankID(tmp->paddr));
                
                //Request is not active and bank is closed. Activate it
                activate->cmd = Activate;
                activate->paddr = tmp->paddr;
                activate->flags &= ~SATISFIED;
                activePages.push_back(getPage(tmp));
                num_active_pages++;
                    
                lastIssuedReq = activate;
                
                req = activate;
                return true;
            }
        }
    }
    
    return false;
}

bool 
RDFCFSTimingMemoryController::getClose(MemReqPtr& req){
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

        if (canClose) {
            close->cmd = Close;
            close->paddr = getPageAddr(Active);
            close->flags &= ~SATISFIED;
            activePages.erase(pageIterator);
            num_active_pages--;
            
            lastIssuedReq = close;
            
            req = close;
            return true;
        }
    }
    return false;
}

bool 
RDFCFSTimingMemoryController::getReady(MemReqPtr& req){
    
    if(equalReadWritePri){
        list<MemReqPtr> mergedQueue = mergeQueues();
        for (queueIterator = mergedQueue.begin(); queueIterator != mergedQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (isReady(tmp)) {
                
                if(isBlocked() && 
                   readQueue.size() <= readqueue_size && 
                   writeQueue.size() <= writequeue_size){
                   setUnBlocked();
                }
        
                lastIssuedReq = tmp;
                lastIsWrite = (tmp->cmd == Writeback);
                
                req = tmp;
                
                return true;
            }
        }
    }
    else{
        // Go through the active pages and find a ready operation
        for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (isReady(tmp)) {
    
                if(isBlocked() && 
                    readQueue.size() <= readqueue_size && 
                    writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                }
        
                lastIssuedReq = tmp;
                lastIsWrite = false;
            
                req = tmp;
                return true;
            }
        }
        for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (isReady(tmp)) {
    
                if(isBlocked() &&
                    readQueue.size() <= readqueue_size && 
                    writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                    }
        
                    lastIssuedReq = tmp;
                    lastIsWrite = true;
                    
                    req = tmp;
                    return true;
            }
        }
    }
    
    return false;
}

bool
RDFCFSTimingMemoryController::getOther(MemReqPtr& req){
    
    if(equalReadWritePri){
        list<MemReqPtr> mergedQueue = mergeQueues();
        for (queueIterator = mergedQueue.begin(); queueIterator != mergedQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            
            if (isActive(tmp)) {
    
                if(isBlocked() && 
                   readQueue.size() <= readqueue_size && 
                   writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                }
            
                lastIssuedReq = tmp;
                lastIsWrite = (tmp->cmd == Writeback);
            
                req = tmp;
                return true;
            }
        }
    }
    else{
        // No ready operation, issue any active operation 
        for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (isActive(tmp)) {
    
                if(isBlocked() && 
                readQueue.size() <= readqueue_size && 
                writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                }
            
                lastIssuedReq = tmp;
                lastIsWrite = false;
                
                req = tmp;
                return true;
            }
        }
        for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (isActive(tmp)) {
    
                if(isBlocked() &&
                readQueue.size() <= readqueue_size && 
                writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                }
            
                lastIssuedReq = tmp;
                lastIsWrite = true;
                
                req = tmp;
                return true;
            }
        }
    }
    return false;
}

list<MemReqPtr>
RDFCFSTimingMemoryController::mergeQueues(){
    list<MemReqPtr> retlist;

    list<MemReqPtr>::iterator readIter =  readQueue.begin();
    list<MemReqPtr>::iterator writeIter = writeQueue.begin();
    
    while(writeIter != writeQueue.end() || readIter != readQueue.end()){
        
        if(writeIter == writeQueue.end()){
            MemReqPtr& readReq = *readIter;
            retlist.push_back(readReq);
            readIter++;
        }
        else if(readIter == readQueue.end()){
            MemReqPtr& writeReq = *writeIter;
            retlist.push_back(writeReq);
            writeIter++;
        }
        else{
            if((*readIter)->inserted_into_memory_controller <= (*writeIter)->inserted_into_memory_controller){
                MemReqPtr& readReq = *readIter;
                retlist.push_back(readReq);
                readIter++;
            }
            else{
                MemReqPtr& writeReq = *writeIter;
                retlist.push_back(writeReq);
                writeIter++;
            }
        }
        assert(retlist.back()->cmd == Read || retlist.back()->cmd == Writeback);
    }
    
    // Check that the merge list is sorted
    list<MemReqPtr>::iterator mergedIter = retlist.begin();
    Tick prevTick = 0;
    for( ; mergedIter != retlist.end() ; mergedIter++){
        MemReqPtr& tmpreq = *mergedIter;
        assert(prevTick <= tmpreq->inserted_into_memory_controller);
        prevTick = tmpreq->inserted_into_memory_controller;
    }
    
    return retlist;
}

list<MemReqPtr>
RDFCFSTimingMemoryController::getPendingRequests(){
    list<MemReqPtr> retval;
    retval.splice(retval.end(), readQueue);
    retval.splice(retval.end(), writeQueue);
    return retval;
}

void
RDFCFSTimingMemoryController::setOpenPages(std::list<Addr> pages){
    assert(activePages.empty());
    activePages.splice(activePages.begin(), pages);
    num_active_pages = activePages.size();
    assert(num_active_pages <= max_active_pages);
    DPRINTF(MemoryController, "Recieved active list, there are now %d active pages\n", num_active_pages);
}

// void
// RDFCFSTimingMemoryController::estimatePrivateServiceLatency(MemReqPtr& req){
//     
//     req->busDelay = 0;
//     int toBank = getMemoryBankID(req->paddr);
//     int fromCPU = req->adaptiveMHASenderID;
//     assert(fromCPU != -1);
//     
//     cout << curTick << ": Req from " << fromCPU << " for bank " << toBank << ", estimating service latency...\n";
//     
//     if(isPageHitOnPrivateSystem(req->paddr, toBank, fromCPU)){
//         cout << curTick << ": Hit!\n";
//         if(isReady(req)){
//             cout << curTick << ": Estimated page hit, hit in shared, no adjust\n";
//         }
//         else if(bankIsClosed(req)){
//             fatal("priv hit, shared is closed --> adjustment needed");
//         }
//         else{
//             assert(isActive(req));
//             fatal("priv hit, shared is active --> adjustment needed");
//         }
//     }
//     else if(isPageConflictOnPrivateSystem(req)){
//         cout << curTick << ": Conflict!\n";
//         if(isPageConflict(req)){
//             Tick actuallyActivatedAt = getBankActivatedAt(toBank);
//             cout << curTick << ": Conflict, bank activated at " << actuallyActivatedAt << "\n";
//             if(actuallyActivatedAt < (curTick + 40)){
//                 //page conflict, but latency was (at least partially) hidden
//                 Tick additionalSharedDelay = actuallyActivatedAt - (curTick + 40);
//                 req->busDelay = (-additionalSharedDelay);
//                 cout << curTick << ": page conflict in both, overlapped in shared, correcting with " << additionalSharedDelay << " ticks\n";
//             }
//             else if(actuallyActivatedAt > (curTick + 40)){
//                 fatal("stop here");
//             }
//             else{
//                 cout << curTick << ": page conflict in both, no overlap, no correction needed\n";
//             }
//         }
//         else{
//             fatal("private conflict, shared non-conflict, help!");
//         }
//     }
//     else{
//         assert(!bankIsClosed(req));
//         cout << curTick << ": Miss!\n";
//         if(isActive(req)){
//             // activation may be overlapped in the shared bus
//             // NOTE: assumes no overlap in the private bus
//             Tick commonActivateAt = curTick + 40;
//             if(getBankActivatedAt(toBank) < commonActivateAt){
//                 Tick overlap = commonActivateAt - getBankActivatedAt(toBank);
//                 if(overlap > 80) req->busDelay = 80;
//                 else req->busDelay = 80 - overlap;
//             }
//             else{
//                 assert(getBankActivatedAt(toBank) == commonActivateAt);
//             }
//         }
//         else{
//             assert(isReady(req));
//             fatal("priv miss, shared is ready --> adjustment needed");
//         }
//         cout << "returning a bus delay of " << req->busDelay << "\n";
//     }
// }

void
RDFCFSTimingMemoryController::computeInterference(MemReqPtr& req, Tick busOccupiedFor){
    
    //FIXME: how should additional L2 misses be handled
    
// #ifdef DO_ESTIMATION_TRACE
    bool isConflict = false;
    bool isHit = false;
// #endif
    
    req->busDelay = 0;
    Tick privateLatencyEstimate = 0;
    if(isPageHitOnPrivateSystem(req->paddr, getMemoryBankID(req->paddr), req->adaptiveMHASenderID)){
        privateLatencyEstimate = 40;
// #ifdef DO_ESTIMATION_TRACE
        isHit = true;
// #endif
    }
    else if(isPageConflictOnPrivateSystem(req)){
        privateLatencyEstimate = 151;
// #ifdef DO_ESTIMATION_TRACE
        isConflict = true;
// #endif
    }
    else{
        privateLatencyEstimate = 110;
    }
    
//     Tick latencyCorrection = 0;
//     if(privateLatencyEstimate != busOccupiedFor){
//         latencyCorrection = privateLatencyEstimate - busOccupiedFor;
//     }
    
#ifdef DO_ESTIMATION_TRACE
    if(req->adaptiveMHASenderID == ESTIMATION_CPU_ID){
        ofstream ofile("estimation_access_trace.txt", ofstream::app);
        ofile << curTick << ";" << req->paddr << ";" << getMemoryBankID(req->paddr) << ";";
        if(isConflict) ofile << "conflict";
        else if(isHit) ofile << "hit";
        else ofile << "miss";
        ofile << ";" << req->inserted_into_memory_controller << ";" << req->oldAddr << "\n";
        ofile.flush();
        ofile.close();
    }
#endif
    
    int fromCPU = req->adaptiveMHASenderID;
    list<MemReqPtr>::iterator readIter;
    assert(fromCPU != -1);
    
    readIter = readQueue.begin();
    for( ; readIter != readQueue.end(); readIter++){
        MemReqPtr waitingReq = *readIter;
        assert(waitingReq->adaptiveMHASenderID != -1);
        assert(waitingReq->cmd == Read);
        
        if(waitingReq->adaptiveMHASenderID == fromCPU){
            assert(req->cmd == Read || req->cmd == Writeback);
            if(req->cmd == Read){
                waitingReq->busAloneQueueEstimate += privateLatencyEstimate;
            }
            else{
                waitingReq->busAloneQueueEstimate += (privateLatencyEstimate * 1.25 );
                waitingReq->waitWritebackCnt++;
            }
        }
    }
    
    if(req->cmd == Read){
        req->busAloneServiceEstimate += privateLatencyEstimate;
    }
    
    
//     readIter = readQueue.begin();
//     for( ; readIter != readQueue.end(); readIter++){
//         MemReqPtr waitingReq = *readIter;
//         assert(waitingReq->adaptiveMHASenderID != -1);
//         assert(waitingReq->cmd == Read);
//         
//         if(waitingReq->adaptiveMHASenderID != fromCPU){
//             int extraLatency = 0;
//             
//             if(waitingReq->inserted_into_memory_controller <= lastDeliveredReqAt){
//                 // already in queue
//                 assert(busOccupiedFor > 0);
//                 extraLatency = busOccupiedFor;
//             }
//             else{
//                 // entered queue while this request was in service
//                 extraLatency = curTick - waitingReq->inserted_into_memory_controller;
//                 assert(extraLatency >= 0);
//             }
//             
//             waitingReq->busQueueInterference += extraLatency;
//             bus->addInterferenceCycles(waitingReq->adaptiveMHASenderID, extraLatency, BUS_INTERFERENCE);
//             waitingReq->interferenceBreakdown[MEM_BUS_TRANSFER_LAT] += extraLatency;
//         }
//         else{
//             waitingReq->busDelay += latencyCorrection;
//             bus->addInterferenceCycles(fromCPU, latencyCorrection, BUS_INTERFERENCE);
//             waitingReq->interferenceBreakdown[MEM_BUS_TRANSFER_LAT] += latencyCorrection;
//         }
//     }
//     
//     if(req->inserted_into_memory_controller > lastDeliveredReqAt
//        && lastOccupyingCPUID != fromCPU
//        && req->cmd == Read){
//         int extraLatency = curTick - req->inserted_into_memory_controller;
//         assert(extraLatency >= 0);
//         req->busQueueInterference += extraLatency;
//         bus->addInterferenceCycles(fromCPU, extraLatency, BUS_INTERFERENCE);
//         req->interferenceBreakdown[MEM_BUS_TRANSFER_LAT] += extraLatency;
//     }
//     
//     if(req->cmd == Read){
//         req->busDelay += latencyCorrection;
//         bus->addInterferenceCycles(fromCPU, latencyCorrection, BUS_INTERFERENCE);
//         req->interferenceBreakdown[MEM_BUS_TRANSFER_LAT] += latencyCorrection;
//     }
}



#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)

    Param<int> readqueue_size;
    Param<int> writequeue_size;
    Param<int> reserved_slots;
    Param<bool> inf_write_bw;
END_DECLARE_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)

    INIT_PARAM_DFLT(readqueue_size, "Max size of read queue", 64),
    INIT_PARAM_DFLT(writequeue_size, "Max size of write queue", 64),
    INIT_PARAM_DFLT(reserved_slots, "Number of activations reserved for reads", 2),
    INIT_PARAM_DFLT(inf_write_bw, "Infinite writeback bandwidth", false)

END_INIT_SIM_OBJECT_PARAMS(RDFCFSTimingMemoryController)


CREATE_SIM_OBJECT(RDFCFSTimingMemoryController)
{
    return new RDFCFSTimingMemoryController(getInstanceName(),
                                            readqueue_size,
                                            writequeue_size,
                                            reserved_slots,
                                            inf_write_bw);
}

REGISTER_SIM_OBJECT("RDFCFSTimingMemoryController", RDFCFSTimingMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS

        
// OLD CODE: kept for compatibilty reasons
// 
// void
// RDFCFSTimingMemoryController::estimateInterference(MemReqPtr& req){
        //     
//     int localCpuCnt = bus->adaptiveMHA->getCPUCount();
//     vector<vector<Tick> > interference(localCpuCnt, vector<Tick>(localCpuCnt, 0));
//     vector<vector<bool> > delayedIsRead(localCpuCnt, vector<bool>(localCpuCnt, false));
//     int fromCPU = req->adaptiveMHASenderID;
        //     
//     if(fromCPU == -1) return;
        //     
//     // Interference due to bus capacity
//     vector<bool> readBusInterference = hasReadyRequestWaiting(req, readQueue);
//     assert(readBusInterference.size() == localCpuCnt);
//     for(int i=0;i<readBusInterference.size();i++){
//         if(readBusInterference[i]){
//             interference[i][req->adaptiveMHASenderID] += 40;
//             delayedIsRead[i][req->adaptiveMHASenderID] = true;
//             bus->addInterference(i, req->adaptiveMHASenderID, BUS_INTERFERENCE);
//         }
//     }
        //     
//     // Interference due to bank conflicts
//     vector<int> readBankInterference = computeBankWaitingPara(req, readQueue);
//     assert(readBankInterference.size() == localCpuCnt);
//     for(int i=0;i<readBankInterference.size();i++){
//         if(readBankInterference[i] > 0){
//             interference[i][req->adaptiveMHASenderID] += 120 / readBankInterference[i];
//             delayedIsRead[i][req->adaptiveMHASenderID] = true;
//             bus->addInterference(i, req->adaptiveMHASenderID, CONFLICT_INTERFERENCE);
//         }
//     }
        //     
//     // Interference due to page hits becoming page misses
//     if(!isPageHit(req->paddr, getMemoryBankID(req->paddr)) 
//         && isPageHitOnPrivateSystem(req->paddr, getMemoryBankID(req->paddr), req->adaptiveMHASenderID)){
//         //NOTE: might want to add these to measurements
//         bus->incInterferenceMisses();
//         bus->addInterference(req->adaptiveMHASenderID, 
//                              getLastActivatedBy(getMemoryBankID(req->paddr)), 
//                              HIT_TO_MISS_INTERFERENCE);
//     }
        //     
//     // Detect constructive interference
//     if(isPageHit(req->paddr, getMemoryBankID(req->paddr)) 
//        && !isPageHitOnPrivateSystem(req->paddr, getMemoryBankID(req->paddr), req->adaptiveMHASenderID)){
//         //NOTE: might want to include these in the measuerements
//         bus->incConstructiveInterference();
//     }
        //     
//     bus->adaptiveMHA->addInterferenceDelay(interference,
//                                            req->paddr,
//                                            req->cmd,
//                                            req->adaptiveMHASenderID,
//                                            MEMORY_INTERFERENCE,
//                                            delayedIsRead);
// }
        // 
// std::vector<bool> 
// RDFCFSTimingMemoryController::hasReadyRequestWaiting(MemReqPtr& req, std::list<MemReqPtr>& queue){
        //     
//     int localCpuCnt = bus->adaptiveMHA->getCPUCount();
//     list<MemReqPtr>::iterator tmpIterator;
//     vector<bool> retval = vector<bool>(localCpuCnt, false);
        //     
//     for (tmpIterator = queue.begin(); tmpIterator != queue.end(); tmpIterator++) {
//         MemReqPtr& tmp = *tmpIterator;
        //         
//         if (req->adaptiveMHASenderID != tmp->adaptiveMHASenderID && tmp->adaptiveMHASenderID != -1) {
//             if(isReady(tmp)){
//                 // interference on bus
//                 retval[tmp->adaptiveMHASenderID] = true;
//             }
//         }
//     }
//     return retval;
// }
        // 
// vector<int>
// RDFCFSTimingMemoryController::computeBankWaitingPara(MemReqPtr& req, std::list<MemReqPtr>& queue){
        //     
//     const int BANKS = 8;
//     int localCpuCnt = bus->adaptiveMHA->getCPUCount();
//     list<MemReqPtr>::iterator tmpIterator;
//     vector<vector<bool> > waitingForBanks(localCpuCnt, vector<bool>(BANKS, false));
//     vector<bool> waitingInSameBank = vector<bool>(localCpuCnt, false);
//     vector<int> retval = vector<int>(localCpuCnt, 0);
        //     
//     stringstream banktrace;
        //     
//     for(tmpIterator = queue.begin();tmpIterator != queue.end();tmpIterator++){
        //         
//         MemReqPtr& tmp = *tmpIterator;
        //         
//         if (tmp->adaptiveMHASenderID != -1) {
//             if(getMemoryBankID(tmp->paddr) != getMemoryBankID(req->paddr)){
//                 waitingForBanks[tmp->adaptiveMHASenderID][getMemoryBankID(tmp->paddr)] = true;
//             }
//             else{
//                 waitingInSameBank[tmp->adaptiveMHASenderID] = true;
//             }
//         }
//     }
        //     
//     for(int i=0;i<localCpuCnt;i++){
//         if(waitingInSameBank[i] && i != req->adaptiveMHASenderID){
//             for(int j=0;j<BANKS;j++){
//                 if(waitingForBanks[i][j]){
//                     retval[i]++;
//                 }
//             }
//         }
//     }
        //     
//     // divide by 2
//     for(int i=0;i<localCpuCnt;i++) retval[i] = retval[i] >> 1;
        //     
//     return retval;
// }

// void
// RDFCFSTimingMemoryController::addInterference(MemReqPtr &req, Tick lat){
//     //NOTE: this method is called at the tick the memory transaction starts
//     // therefore, all reqs in the queue have actually delayed by this req
        //     
//     std::list<MemReqPtr>::iterator tmpIterator = readQueue.begin();
//     while(tmpIterator != readQueue.end()){
//         MemReqPtr tmpReq = *tmpIterator;
//         if(tmpReq->adaptiveMHASenderID != req->adaptiveMHASenderID){
//             tmpReq->busDelay += lat;
//         }
//         tmpIterator++;
//     }
// }
