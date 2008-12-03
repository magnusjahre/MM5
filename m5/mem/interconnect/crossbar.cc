
#include "sim/builder.hh"
#include "crossbar.hh"
        
using namespace std;

Crossbar::Crossbar(const std::string &_name,
                   int _width, 
                   int _clock,
                   int _transDelay,
                   int _arbDelay,
                   int _cpu_count,
                   HierParams *_hier,
                   AdaptiveMHA* _adaptiveMHA,
                   bool _useNFQArbitration,
                   Tick _detailedSimStartTick,
                   int _shared_cache_wb_buffers,
                   int _shared_cache_mshrs)
    : Interconnect(_name,
                   _width, 
                   _clock, 
                   _transDelay, 
                   _arbDelay,
                   _cpu_count,
                   _hier,
                   _adaptiveMHA){
    
    detailedSimStartTick = _detailedSimStartTick;
    crossbarTransferDelay = _transDelay + _arbDelay;
    crossbarRequests = vector<list<pair<MemReqPtr, int> > >(_cpu_count, list<pair<MemReqPtr, int> >());
    
    
    perEndPointQueueSize = 64; //16; // FIXME: parameterize
    requestOccupancyTicks = 2; // FIXME: parameterize
    requestL2BankCount = 4;
    crossbarResponses = vector<list<pair<MemReqPtr, int> > >(requestL2BankCount, list<pair<MemReqPtr, int> >());
    
    slaveDeliveryBuffer = vector<list<DeliveryBufferEntry> >(requestL2BankCount, list<DeliveryBufferEntry>());
    
    blockedLocalQueues = vector<bool>(_cpu_count, false);
    requestsInProgress = vector<int>(requestL2BankCount, 0);
    
    notRetrievedRequests = vector<int>((_cpu_count * 2) + requestL2BankCount, 0);
    
    crossbarArbEvent = new CrossbarArbitrationEvent(this);
    
    shared_cache_wb_buffers = _shared_cache_wb_buffers;
    shared_cache_mshrs = _shared_cache_mshrs;
    
    if(shared_cache_mshrs == -1 || shared_cache_wb_buffers == -1){
        fatal("The crossbar needs to know the number of MSHRs and WB buffers in the shared cache to esimate interference");
    }
    
    if(requestL2BankCount + _cpu_count > 32){
        fatal("The current crossbar implementation supports maximum 32 endpoints");
    }
}

void
Crossbar::request(Tick cycle, int fromID){
    requests++;
    CrossbarRetrieveReqEvent* event = new CrossbarRetrieveReqEvent(this, fromID);
    event->schedule(cycle);
}

void 
Crossbar::retriveRequest(int fromInterface){
    
    DPRINTF(Crossbar, "Request recieved from interface %d, cpu %d\n", fromInterface, interconnectIDToProcessorIDMap[fromInterface]);
    
    if(!allInterfaces[fromInterface]->isMaster()){
        allInterfaces[fromInterface]->grantData();
        return;
    }
    
    if(!blockedLocalQueues[interconnectIDToProcessorIDMap[fromInterface]]){
        allInterfaces[fromInterface]->grantData();
    }
    else{
        notRetrievedRequests[fromInterface]++;
    }
}

void
Crossbar::send(MemReqPtr& req, Tick time, int fromID){
    
    req->inserted_into_crossbar = curTick;
    
    // FIXME: what about entries from the slave side?
    if(allInterfaces[fromID]->isMaster()){
        assert(curTick >= req->finishedInCacheAt);
        int waitTime = curTick - req->finishedInCacheAt; 
        entryDelay += waitTime;
        
        assert(req->cmd == Read || req->cmd == Writeback);
        if(req->cmd == Read && waitTime > 0){
            assert(req->adaptiveMHASenderID != -1);
                
            int extraDelay = (int) ((double) waitTime * 0.75);
            cpuEntryInterferenceCycles[req->adaptiveMHASenderID] += extraDelay;
            adaptiveMHA->addAloneInterference(extraDelay, req->adaptiveMHASenderID, INTERCONNECT_INTERFERENCE);
            req->interferenceBreakdown[INTERCONNECT_ENTRY_LAT] += extraDelay;
            
            entryReadDelay += waitTime;
            perCpuTotalDelay[req->adaptiveMHASenderID] += waitTime;
        }

        req->latencyBreakdown[INTERCONNECT_ENTRY_LAT] += waitTime;
    }
    entryRequests++;
    if(req->cmd == Read) entryReadRequests++;
    
    assert(req->adaptiveMHASenderID >= 0 && req->adaptiveMHASenderID < cpu_count);
    int resources = 0;
    
    if(allInterfaces[fromID]->isMaster()){
        int destinationMask = -1;
        for(int i=0;i<slaveInterfaces.size();i++){
            if(slaveInterfaces[i]->inRange(req->paddr)){
                destinationMask = 1 << (cpu_count + i);
                req->toInterfaceID = L2IDMapToInterconnectID[i];
                break;
            }
        }
        assert(destinationMask != -1);
        resources |= destinationMask;
        resources |= (1 << req->adaptiveMHASenderID);
        
        assert(req->adaptiveMHASenderID == interconnectIDToProcessorIDMap[fromID]);
        
        DPRINTF(Crossbar, "Inserting request from master %d, cpu %d, addr %x\n", fromID, interconnectIDToProcessorIDMap[fromID], req->paddr);
        
        if(!crossbarRequests[req->adaptiveMHASenderID].empty()){
            assert(crossbarRequests[req->adaptiveMHASenderID].back().first->inserted_into_crossbar <= req->inserted_into_crossbar);
        }
        crossbarRequests[req->adaptiveMHASenderID].push_back(pair<MemReqPtr, int>(req, resources));
        
        if(crossbarRequests[req->adaptiveMHASenderID].size() >= perEndPointQueueSize){
            setBlockedLocal(req->adaptiveMHASenderID);
        }
    }
    else{
        int bankID = interconnectIDToL2IDMap[fromID];
        resources |= 1 << req->adaptiveMHASenderID;
        resources |= 1 << (bankID + cpu_count);
        
        DPRINTF(Crossbar, "Inserting request from slave %d, cpu %d, addr %x\n", fromID, interconnectIDToProcessorIDMap[fromID], req->paddr);
        
        if(!crossbarResponses[bankID].empty()){
            assert(crossbarResponses[bankID].back().first->inserted_into_crossbar <= req->inserted_into_crossbar);
        }
        crossbarResponses[bankID].push_back(pair<MemReqPtr, int>(req, resources));
    }
    
    if(!crossbarArbEvent->scheduled()){
        crossbarArbEvent->schedule(curTick);
    }
}

void
Crossbar::arbitrate(Tick time){
    
    if(curTick < detailedSimStartTick){
        
        // infinite bw in warm-up, deliver all pending requests
        for(int i=0;i<crossbarResponses.size();i++){
            while(!crossbarResponses[i].empty()){
                CrossbarDeliverEvent* delivery = new CrossbarDeliverEvent(this, crossbarResponses[i].front().first, false);
                delivery->schedule(curTick + crossbarTransferDelay);
                crossbarResponses[i].pop_front();
            }
        }
        
        for(int i=0;i<crossbarRequests.size();i++){
            while(!crossbarRequests[i].empty()){
                
                CrossbarDeliverEvent* delivery = new CrossbarDeliverEvent(this, crossbarRequests[i].front().first, true);
                delivery->schedule(curTick + crossbarTransferDelay);
                requestsInProgress[interconnectIDToL2IDMap[crossbarRequests[i].front().first->toInterfaceID]]++;
                crossbarRequests[i].pop_front();
            }
        }
        return;
    }
    
    // initialize crossbar state with information about the blocked interfaces
    int masterToSlaveCrossbarState = addBlockedInterfaces();
    int slaveToMasterCrossbarState = 0;
    
    DPRINTF(Crossbar, "Arbitating, initial master to slave cb state is %x\n", masterToSlaveCrossbarState);
    
    vector<int> masterOrder = findServiceOrder(&crossbarRequests);
    assert(masterOrder.size() == cpu_count);
    for(int i=0;i<masterOrder.size();i++){
        attemptDelivery(&crossbarRequests[masterOrder[i]], &masterToSlaveCrossbarState, true);
    }
    
    vector<int> slaveOrder = findServiceOrder(&crossbarResponses);
    for(int i=0;i<slaveOrder.size();i++){
        attemptDelivery(&crossbarResponses[slaveOrder[i]], &slaveToMasterCrossbarState, false);
    }
    
    bool moreReqs = false;
    for(int i=0;i<crossbarResponses.size();i++) if(!crossbarResponses[i].empty()) moreReqs = true;
    for(int i=0;i<crossbarRequests.size();i++) if(!crossbarRequests[i].empty()) moreReqs = true;
    
    if(moreReqs){
        crossbarArbEvent->schedule(curTick + requestOccupancyTicks);
    }
    
    // we might have space in the local queues now, attempt to retrieve additional requests
    retriveAdditionalRequests();
}

vector<int>
Crossbar::findServiceOrder(std::vector<std::list<std::pair<MemReqPtr, int> > >* currentQueue){
    vector<int> order(cpu_count, -1);
    vector<bool> marked(cpu_count, false);
    stringstream debugtrace;
    
    for(int i=0;i<currentQueue->size();i++){
        
        Tick min = 1000000000000000ull;
        Tick minIndex = -1;
            
        for(int j=0;j<currentQueue->size();j++){
            if(!marked[j] && !(*currentQueue)[j].empty()){
                MemReqPtr req = (*currentQueue)[j].front().first;
                if(req->inserted_into_crossbar < min){
                    minIndex = j;
                    min = req->inserted_into_crossbar;
                }
            }
        }
        
        if(minIndex == -1){
            for(int j=0;j<currentQueue->size();j++){
                if(!marked[j] && (*currentQueue)[j].empty()){
                    minIndex = j;
                    min = -1;
                    break;
                }
            }
        }
        
        assert(minIndex != -1);
        order[i] = minIndex;
        marked[minIndex] = true;
        debugtrace << "(" << minIndex << ", " << min << ") ";
    }
    
    DPRINTF(Crossbar, "Service order: %s\n", debugtrace.str());
    return order;
}

int
Crossbar::addBlockedInterfaces(){
    int state = 0;
    
    stringstream debugtrace;
    
    // the buffer on the slave side only contains enough spaces to empty the crossbar pipeline when the slave blocks
    // make sure we do not issue more requests than we can handle
    debugtrace << "Pipe full: ";
    for(int i=0;i<requestsInProgress.size();i++){
        if(requestsInProgress[i] >= (crossbarTransferDelay / requestOccupancyTicks)){
            debugtrace << i <<":1 ";
            state |= 1 << (i + cpu_count);
        }
        else{
            debugtrace << i << ":0 ";
        }
    }
    
    DPRINTF(Crossbar, "Arbitating, current blocked state: %s\n", debugtrace.str());
    
    return state;
}

void 
Crossbar::retriveAdditionalRequests(){
    
    bool allZero = true;
    for(int i=0;i<allInterfaces.size();i++){
        if(!allInterfaces[i]->isMaster()){
            assert(notRetrievedRequests[i] == 0);
        }
        if(notRetrievedRequests[i] > 0) allZero = false;
    }
    
    if(!allZero){
        
        for(int i=0;i<cpu_count;i++){
            assert(processorIDToInterconnectIDs[i].size() == 2);
            int firstInterfaceID = processorIDToInterconnectIDs[i].front();
            int secondInterfaceID = processorIDToInterconnectIDs[i].back();

            while((notRetrievedRequests[firstInterfaceID] > 0 || notRetrievedRequests[secondInterfaceID] > 0) 
                   && !blockedLocalQueues[i]){
                
                MemReqPtr firstReq = allInterfaces[firstInterfaceID]->getPendingRequest();
                MemReqPtr secondReq = allInterfaces[secondInterfaceID]->getPendingRequest();
                int grantedID = -1;
                
                if(notRetrievedRequests[firstInterfaceID] > 0 && notRetrievedRequests[secondInterfaceID] == 0){
                    allInterfaces[firstInterfaceID]->grantData();
                    grantedID = firstInterfaceID;
                }
                else if(notRetrievedRequests[firstInterfaceID] == 0 && notRetrievedRequests[secondInterfaceID] > 0){
                    allInterfaces[secondInterfaceID]->grantData();
                    grantedID = secondInterfaceID;
                }
                else{
                    assert(notRetrievedRequests[firstInterfaceID] > 0);
                    assert(notRetrievedRequests[secondInterfaceID] > 0);
                    
                    Tick firstTime = firstReq ? firstReq->finishedInCacheAt : 0;
                    Tick secondTime = secondReq ? secondReq->finishedInCacheAt : 0;
                    
                    if(firstTime <= secondTime){
                        allInterfaces[firstInterfaceID]->grantData();
                        grantedID = firstInterfaceID;
                    }
                    else{
                        allInterfaces[secondInterfaceID]->grantData();
                        grantedID = secondInterfaceID;
                    }
                }
                
                notRetrievedRequests[grantedID]--;
                DPRINTF(Crossbar, "Accepting new request from interface %d, proc %d, first waiting %d, second waiting %d, queued %d\n", grantedID, i, notRetrievedRequests[firstInterfaceID], notRetrievedRequests[secondInterfaceID], crossbarRequests[i].size());
            }
        }
    }
}

bool
Crossbar::attemptDelivery(list<pair<MemReqPtr, int> >* currentQueue, int* crossbarState, bool toSlave){

    if(!currentQueue->empty()){
        if((currentQueue->front().second & *crossbarState) == 0){
            CrossbarDeliverEvent* delivery = new CrossbarDeliverEvent(this, currentQueue->front().first, toSlave);
            delivery->schedule(curTick + crossbarTransferDelay);
            
            MemReqPtr req = currentQueue->front().first;
            
            totalArbQueueCycles += curTick - req->inserted_into_crossbar;
            arbitratedRequests++;
            
            int fromCPU = req->adaptiveMHASenderID;
            
            if(toSlave){
                int toSlaveID = interconnectIDToL2IDMap[req->toInterfaceID];
                requestsInProgress[toSlaveID]++;
            }
            
            DPRINTF(Crossbar, "Granting access to proc %d, addr %x, cb state %x, deliver at %d\n", fromCPU, req->paddr, *crossbarState, curTick + crossbarTransferDelay);
            
            if(blockedLocalQueues[fromCPU] &&
                crossbarRequests[fromCPU].size() < perEndPointQueueSize){
                clearBlockedLocal(fromCPU);
            }
            
            *crossbarState |= currentQueue->front().second; 
            currentQueue->pop_front();
            
            // check interference if from slave 
            if(!toSlave && !currentQueue->empty() && cpu_count > 1){
                list<pair<MemReqPtr, int> >::iterator it = currentQueue->begin();
                for( ; it != currentQueue->end(); it++){
                    int toID = it->first->adaptiveMHASenderID;
                    assert(it->first->cmd == Read);
                    if(toID != req->adaptiveMHASenderID){
                        cpuTransferInterferenceCycles[toID] += requestOccupancyTicks;
                        adaptiveMHA->addAloneInterference(requestOccupancyTicks, toID, INTERCONNECT_INTERFERENCE);
                        it->first->interferenceBreakdown[INTERCONNECT_TRANSFER_LAT] += requestOccupancyTicks;
                    }
                }
            }
            
            return true;
        }
        else{
            if(toSlave){
                // Interference: empty pipe stage, all waiting requests are delayed
//                 int senderCPUID = currentQueue->front().first->adaptiveMHASenderID;
                
                bool destinationBlocked = (addBlockedInterfaces() & currentQueue->front().second) != 0;
                
                list<pair<MemReqPtr, int> >::iterator it = currentQueue->begin();
                for( ; it != currentQueue->end(); it++){
                    MemCmd cmd = it->first->cmd;
                    assert(cmd == Read || cmd == Writeback);
                    
                    if(!destinationBlocked){
                        if(cpu_count > 1){
                            it->first->interferenceBreakdown[INTERCONNECT_TRANSFER_LAT] += requestOccupancyTicks;
                            
                            if(cmd == Read){
                                cpuTransferInterferenceCycles[it->first->adaptiveMHASenderID] += requestOccupancyTicks;
                                adaptiveMHA->addAloneInterference(requestOccupancyTicks, it->first->adaptiveMHASenderID, INTERCONNECT_INTERFERENCE);
                            }
                        }
                        
                    }
                    else{
                        if(cpu_count > 1){
                            
                            if(!blockingDueToPrivateAccesses(it->first->adaptiveMHASenderID)){
                                it->first->interferenceBreakdown[INTERCONNECT_DELIVERY_LAT] += requestOccupancyTicks;
                                
                                if(cmd == Read){
                                    cpuDeliveryInterferenceCycles[it->first->adaptiveMHASenderID] += requestOccupancyTicks;
                                    adaptiveMHA->addAloneInterference(requestOccupancyTicks, it->first->adaptiveMHASenderID, INTERCONNECT_INTERFERENCE);
                                }
                            }
                        }
                        it->first->latencyBreakdown[INTERCONNECT_TRANSFER_LAT] -= requestOccupancyTicks;
                        it->first->latencyBreakdown[INTERCONNECT_DELIVERY_LAT] += requestOccupancyTicks;
                    }
                }


            }
            else{
                
                if(currentQueue->size() > 1 && cpu_count > 1){
                    // since this is an output conflict, the first request is from the same CPU as the one it is in conflict with
                    // however, other queued requests might be to different processors
                    
                    int firstCPUID = currentQueue->front().first->adaptiveMHASenderID;
                    list<pair<MemReqPtr, int> >::iterator it = currentQueue->begin();
                    
                    it++; //skip first
                    for( ; it != currentQueue->end(); it++){
                        int toID = it->first->adaptiveMHASenderID;
                        assert(it->first->cmd == Read);
                        if(toID != firstCPUID){
                            cpuTransferInterferenceCycles[toID] += requestOccupancyTicks;
                            adaptiveMHA->addAloneInterference(requestOccupancyTicks, toID, INTERCONNECT_INTERFERENCE);
                            it->first->interferenceBreakdown[INTERCONNECT_TRANSFER_LAT] += requestOccupancyTicks;
                        }
                    }
                }
            }
            


        }
    }
    return false;
}

void
Crossbar::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){
    
    DPRINTF(Crossbar, "Delivering to %d from %d, proc %d, addr %x\n", toID, fromID, req->adaptiveMHASenderID, req->paddr);
    
    totalTransferCycles += crossbarTransferDelay;
    sentRequests++;
    
    req->latencyBreakdown[INTERCONNECT_TRANSFER_LAT] += curTick - req->inserted_into_crossbar;
    
    if(allInterfaces[toID]->isMaster()){
        
        if(req->cmd == Read){
            assert(req->adaptiveMHASenderID != -1);
            // the request was counted earlier, don't count it again
            assert(curTick - req->inserted_into_crossbar >= transferDelay);
            perCpuTotalDelay[req->adaptiveMHASenderID] += curTick - req->inserted_into_crossbar;
        }
        
        allInterfaces[toID]->deliver(req);
        deliverBufferRequests++;
    }
    else{
        int toSlaveID = interconnectIDToL2IDMap[toID];
        if(blockedInterfaces[toSlaveID]){

            DeliveryBufferEntry entry(req, curTick, blockingDueToPrivateAccesses(req->adaptiveMHASenderID));
            slaveDeliveryBuffer[toSlaveID].push_back(entry);
            
            DPRINTF(Crossbar, "Delivery queued, %d requests in buffer for slave %d\n", slaveDeliveryBuffer[toSlaveID].size(), toSlaveID);
            assert(slaveDeliveryBuffer[toSlaveID].size() <= crossbarTransferDelay / requestOccupancyTicks);
        }
        else{
            
            if(req->cmd == Read){
                assert(req->adaptiveMHASenderID != -1);
                assert(curTick - req->inserted_into_crossbar >= transferDelay);
                perCpuTotalDelay[req->adaptiveMHASenderID] += curTick - req->inserted_into_crossbar;
                perCpuRequests[req->adaptiveMHASenderID]++;
            }
            
            assert(slaveDeliveryBuffer[toSlaveID].empty());
            allInterfaces[toID]->access(req);
            requestsInProgress[toSlaveID]--;
            deliverBufferRequests++;
        }
    }
}

void
Crossbar::clearBlocked(int fromInterface){
    Interconnect::clearBlocked(fromInterface);
    
    assert(!allInterfaces[fromInterface]->isMaster());
    int unblockedSlaveID = interconnectIDToL2IDMap[fromInterface];
    
    while(!slaveDeliveryBuffer[unblockedSlaveID].empty()){
        DPRINTF(Crossbar, "Issuing queued request, %d reqs left for slave %d\n",requestsInProgress[unblockedSlaveID]-1, unblockedSlaveID);
        
        DeliveryBufferEntry entry = slaveDeliveryBuffer[unblockedSlaveID].front();
        MemReqPtr req = entry.req;
        Tick queuedAt = entry.enteredAt;
        
        req->latencyBreakdown[INTERCONNECT_DELIVERY_LAT] += curTick - queuedAt;
        
        deliverBufferDelay += curTick - queuedAt;
        deliverBufferRequests++;
        
        assert(req->cmd == Read || req->cmd == Writeback);
        if(!entry.blameForBlocking && req->cmd == Read && cpu_count > 1){
            Tick extraDelay = curTick - queuedAt;
            cpuDeliveryInterferenceCycles[req->adaptiveMHASenderID] += extraDelay;
            adaptiveMHA->addAloneInterference(extraDelay, req->adaptiveMHASenderID, INTERCONNECT_INTERFERENCE);
            req->interferenceBreakdown[INTERCONNECT_DELIVERY_LAT] += extraDelay;
        }
        
        assert(req->adaptiveMHASenderID != -1);
        if(req->cmd == Read){
            perCpuTotalDelay[req->adaptiveMHASenderID] += curTick - req->inserted_into_crossbar;
            perCpuRequests[req->adaptiveMHASenderID]++;
        }
        
        MemAccessResult res = allInterfaces[fromInterface]->access(req);
        slaveDeliveryBuffer[unblockedSlaveID].pop_front();
        
        requestsInProgress[unblockedSlaveID]--;

        // the interface is blocked again, stop sending requests
        if(res == BA_BLOCKED) break;
    }
}

void 
Crossbar::setBlockedLocal(int fromCPUId){
    DPRINTF(Blocking, "Blocking the Interconnect due to full local queue for CPU %d\n", fromCPUId);
    assert(!blockedLocalQueues[fromCPUId]);
    blockedLocalQueues[fromCPUId] = true;
}

void 
Crossbar::clearBlockedLocal(int fromCPUId){
    DPRINTF(Blocking, "Unblocking the Interconnect, local queue space available for CPU%d\n", fromCPUId);
    assert(blockedLocalQueues[fromCPUId]);
    blockedLocalQueues[fromCPUId] = false;
}

bool 
Crossbar::blockingDueToPrivateAccesses(int blockedCPU){
    int reads = 0;
    int writes = 0;
    
    assert(blockedCPU >= 0 && blockedCPU < crossbarRequests.size());
    
    list<pair<MemReqPtr, int> >::iterator queueIt;
    queueIt = crossbarRequests[blockedCPU].begin();
    for( ; queueIt != crossbarRequests[blockedCPU].end() ; queueIt++){
        MemReqPtr req = queueIt->first;
        assert(req->cmd == Read || req->cmd == Writeback);
        if(req->cmd == Read) reads++;
        else writes++;
    }
    
    double mshrThreshold = shared_cache_mshrs * 2.35;
    double wbThreshold = shared_cache_wb_buffers * 2.35;
    
    return (reads >= mshrThreshold || writes >= wbThreshold);
}

vector<int>
Crossbar::getChannelSample(){
    fatal("ni");
}

void
Crossbar::writeChannelDecriptor(std::ofstream &stream){
    fatal("ni");
}

std::vector<std::vector<int> > 
Crossbar::retrieveInterferenceStats(){
    vector<std::vector<int> > retval(cpu_count, vector<int>(cpu_count, 0));
    return retval;
}

void 
Crossbar::resetInterferenceStats(){
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Crossbar)
    Param<int> width;
    Param<int> clock;
    Param<int> transferDelay;
    Param<int> arbitrationDelay;
    Param<int> cpu_count;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
    Param<bool> use_NFQ_arbitration;
    Param<Tick> detailed_sim_start_tick;
    Param<int> shared_cache_writeback_buffers;
    Param<int> shared_cache_mshrs;
END_DECLARE_SIM_OBJECT_PARAMS(Crossbar)

BEGIN_INIT_SIM_OBJECT_PARAMS(Crossbar)
    INIT_PARAM(width, "the width of the crossbar transmission channels"),
    INIT_PARAM(clock, "crossbar clock"),
    INIT_PARAM(transferDelay, "crossbar transfer delay in CPU cycles"),
    INIT_PARAM(arbitrationDelay, "crossbar arbitration delay in CPU cycles"),
    INIT_PARAM(cpu_count, "the number of CPUs in the system"),
    INIT_PARAM_DFLT(hier,
                    "Hierarchy global variables",
                    &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "AdaptiveMHA object", NULL),
    INIT_PARAM_DFLT(use_NFQ_arbitration, "If true, Network Fair Queuing arbitration is used", false),
    INIT_PARAM(detailed_sim_start_tick, "The tick detailed simulation starts"),
    INIT_PARAM_DFLT(shared_cache_writeback_buffers, "Number of writeback buffers in the shared cache", -1),
    INIT_PARAM_DFLT(shared_cache_mshrs, "Number of MSHRs in the shared cache", -1)
END_INIT_SIM_OBJECT_PARAMS(Crossbar)

CREATE_SIM_OBJECT(Crossbar)
{
    return new Crossbar(getInstanceName(),
                        width,
                        clock,
                        transferDelay,
                        arbitrationDelay,
                        cpu_count,
                        hier,
                        adaptive_mha,
                        use_NFQ_arbitration,
                        detailed_sim_start_tick,
                        shared_cache_writeback_buffers,
                        shared_cache_mshrs);
}

REGISTER_SIM_OBJECT("Crossbar", Crossbar)

#endif //DOXYGEN_SHOULD_SKIP_THIS
