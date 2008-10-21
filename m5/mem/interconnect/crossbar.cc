
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
                   Tick _detailedSimStartTick)
    : Interconnect(_name,
                   _width, 
                   _clock, 
                   _transDelay, 
                   _arbDelay,
                   _cpu_count,
                   _hier,
                   _adaptiveMHA){
            
    isFirstRequest = true;
    nextBusFreeTime = 0;
    doProfiling = false;
    
    detailedSimStartTick = _detailedSimStartTick;
    
    interferenceEvents = vector<vector<int> >(cpu_count, vector<int>(cpu_count,0));
    
    doFairArbitration = _useNFQArbitration;
    virtualFinishTimes = vector<Tick>(_cpu_count, 0);
}

void
Crossbar::doWarmUpArbitration(Tick cycle, Tick candidateTime){
    
    list<InterconnectRequest* >::iterator queueIterator = requestQueue.begin();
    
    while(queueIterator != requestQueue.end()){
        
        InterconnectRequest* req = *queueIterator;
            
        if(req->time <= candidateTime){
            allInterfaces[req->fromID]->grantData();
            delete req; 
            queueIterator = requestQueue.erase(queueIterator);
        }
        else{
            queueIterator++;
        }
    }
    
    assert(isSorted(&requestQueue));
    if(!requestQueue.empty()){
        if(requestQueue.front()->time <= candidateTime){
            scheduleArbitrationEvent(cycle+1);
        }
        else{
            scheduleArbitrationEvent(requestQueue.front()->time + arbitrationDelay);
        }
    }
    
}

void
Crossbar::arbitrate(Tick cycle){
    
    Tick candiateReqTime = cycle - arbitrationDelay;
    
    if(curTick < detailedSimStartTick){
        doWarmUpArbitration(cycle, candiateReqTime);
        return;
    }
    
    list<InterconnectRequest* > notGrantedReqs;
    vector<bool> occupiedEndNodes(cpu_count + slaveInterfaces.size(), false);
    
    bool busIsUsed = false;
    if(cycle <= nextBusFreeTime) busIsUsed = true;
    
    vector<int> grantedCPUs;
    vector<int> toBanks;
    vector<Addr> destinationAddrs;
    vector<MemCmd> currentCommands;
    
    if(!doFairArbitration){
        doStandardArbitration(candiateReqTime,
                              notGrantedReqs,
                              cycle,
                              busIsUsed,
                              occupiedEndNodes,
                              grantedCPUs,
                              toBanks,
                              destinationAddrs,
                              currentCommands);
    }
    else{
        // NFQ crossbar arbitration
        doNFQArbitration(candiateReqTime,
                         notGrantedReqs,
                         cycle,
                         busIsUsed,
                         occupiedEndNodes,
                         grantedCPUs,
                         toBanks,
                         destinationAddrs,
                         currentCommands);
    }
    
    //measure interference
    if(adaptiveMHA != NULL){
        
        vector<int> interferenceDelay(cpu_count, 0);
        
        for(int i=0;i<grantedCPUs.size();i++){
            
            vector<bool> isBlocking(cpu_count, false);
            vector<bool> isRead(cpu_count, false);
            
            if(!notGrantedReqs.empty()){
                list<InterconnectRequest* >::iterator notGrantedIterator;
                notGrantedIterator = notGrantedReqs.begin();
                
                while(notGrantedIterator != notGrantedReqs.end()){
                    InterconnectRequest* tmpReq = *notGrantedIterator;
                    
                    if(tmpReq->time <= candiateReqTime){
                        
                        MemCmd curCmd = allInterfaces[tmpReq->fromID]->getCurrentCommand();
                        assert(curCmd == Read || curCmd == Writeback);
                        if(curCmd == Read){
                            int tmpDestID = getDestinationId(tmpReq->fromID);
                        
                            int cpuID = -1;
                            (allInterfaces[tmpReq->fromID]->isMaster() 
                                    ? cpuID = interconnectIDToProcessorIDMap[tmpReq->fromID] 
                                    : cpuID = interconnectIDToProcessorIDMap[tmpDestID]);
                            assert(cpuID >= 0 && cpuID < cpu_count);
                            
                            int bankID = -1;
                            (allInterfaces[tmpReq->fromID]->isMaster() 
                                    ? bankID = interconnectIDToL2IDMap[tmpDestID] + cpu_count
                                    : bankID = interconnectIDToL2IDMap[tmpReq->fromID] + cpu_count);
                            
                            
                            if(bankID == toBanks[i] && cpuID != grantedCPUs[i]){
                                
                                interferenceDelay[cpuID] = 1;
                                
                                // only update values if we have not encountered a request from this processor before
                                if(!isBlocking[cpuID]){
                                    isBlocking[cpuID] = true;
                                    
                                    MemCmd delayedCmd = allInterfaces[tmpReq->fromID]->getCurrentCommand();
                                    if(delayedCmd == Read) isRead[cpuID] = true;
                                    else isRead[cpuID] = false;
                                }
        
                            }
                        }
                    }
                    notGrantedIterator++;
                }
                
                vector<vector<Tick> > queueWaitBuffer = vector<vector<Tick> >(cpu_count, vector<Tick>(cpu_count, 0));
                vector<vector<bool> > delayedIsRead(cpu_count, vector<bool>(cpu_count, false));
                
                for(int j=0;j<cpu_count;j++){
                    if(isBlocking[j]){
                        queueWaitBuffer[j][grantedCPUs[i]] = 1;
                        delayedIsRead[j][grantedCPUs[i]] = isRead[j];
                        interferenceEvents[j][grantedCPUs[i]]++;
                    }
                }
                
                adaptiveMHA->addInterferenceDelay(queueWaitBuffer,
                                                  destinationAddrs[i],
                                                  currentCommands[i],
                                                  grantedCPUs[i],
                                                  INTERCONNECT_INTERFERENCE,
                                                  delayedIsRead);
            }
        }
        
        // Check for delays due to blocking (assume this is interference)
        list<InterconnectRequest* >::iterator blockingNotGrantIterator;
        blockingNotGrantIterator = notGrantedReqs.begin();
                
        while(blockingNotGrantIterator != notGrantedReqs.end()){
            InterconnectRequest* tmpReq = *blockingNotGrantIterator;
            
            if(tmpReq->time <= candiateReqTime){
                
                MemCmd curCmd = allInterfaces[tmpReq->fromID]->getCurrentCommand();
                assert(curCmd == Read || curCmd == Writeback);
                
                if(allInterfaces[tmpReq->fromID]->isMaster() && 
                   curCmd == Read &&
                   blockedInterfaces[interconnectIDToL2IDMap[getDestinationId(tmpReq->fromID)]]){
                    interferenceDelay[interconnectIDToProcessorIDMap[tmpReq->fromID]] = 1;
                }
                
            
            }
            blockingNotGrantIterator++;
        }
        
        for(int i=0;i<cpu_count;i++){
            cpuInterferenceCycles[i] += interferenceDelay[i];
            if(interferenceDelay[i] > 0){
                adaptiveMHA->addAloneInterference(interferenceDelay[i], i, INTERCONNECT_INTERFERENCE);
            }
        }
    }
    
    // put not granted requests back into the request queue
    assert(requestQueue.empty());
    requestQueue.splice(requestQueue.begin(), notGrantedReqs);
#ifdef DEBUG_CROSSBAR
    assert(isSorted(&requestQueue));
#endif //DEBUG_CROSSBAR
    
    if(!requestQueue.empty()){
        if(requestQueue.front()->time <= candiateReqTime){
            scheduleArbitrationEvent(cycle+1);
        }
        else{
            scheduleArbitrationEvent(requestQueue.front()->time
                                    + arbitrationDelay);
        }
    }
}

void
Crossbar::doStandardArbitration(Tick candiateReqTime,
                                list<InterconnectRequest* > &notGrantedReqs,
                                Tick cycle,
                                bool& busIsUsed,
                                vector<bool>& occupiedEndNodes,
                                vector<int> &grantedCPUs,
                                vector<int> &toBanks,
                                vector<Addr> &destinationAddrs,
                                vector<MemCmd> &currentCommands){
    
    while(!requestQueue.empty()){
        InterconnectRequest* req = requestQueue.front();
        requestQueue.pop_front();
            
        if(req->time <= candiateReqTime){
                
            int toInterfaceID = getDestinationId(req->fromID);
            if(toInterfaceID == -1){ delete req; continue; }
                
                // check if destination is blocked
            if(!(allInterfaces[toInterfaceID]->isMaster()) 
                 && blockedInterfaces[interconnectIDToL2IDMap[toInterfaceID]]){
                // the destination cache is blocked, so we can not deliver to it
                notGrantedReqs.push_back(req);
                continue;
            }
            
            // check if the request can be granted access
            bool grantAccess = checkCrossbarState(req,toInterfaceID,&occupiedEndNodes,&busIsUsed,cycle);
                
            if(grantAccess){
                grantInterface(req,toInterfaceID,cycle,grantedCPUs,toBanks,destinationAddrs,currentCommands);
            }
            else{
                notGrantedReqs.push_back(req);
            }
        }
        else{
                // not ready
            notGrantedReqs.push_back(req);
        }
    }
}
        
void 
Crossbar::doNFQArbitration(Tick candiateReqTime,
                           list<InterconnectRequest* > &notGrantedReqs,
                           Tick cycle,
                           bool& busIsUsed,
                           vector<bool>& occupiedEndNodes,
                           vector<int> &grantedCPUs,
                           vector<int> &toBanks,
                           vector<Addr> &destinationAddrs,
                           vector<MemCmd> &currentCommands){
    
    vector<int> numSlaveReqs(slaveInterfaces.size(), 0);
    
    list<InterconnectRequest* >::iterator tmpIterator;
    tmpIterator = requestQueue.begin();
    while(tmpIterator != requestQueue.end()){
            
        InterconnectRequest* req = *tmpIterator;
            
        if(req->time <= candiateReqTime){
                
            int toInterfaceID = getDestinationId(req->fromID);
            if(toInterfaceID == -1){
                delete req; 
                tmpIterator = requestQueue.erase(tmpIterator);
                continue;
            }
            
            int proc = -1;
            if(allInterfaces[toInterfaceID]->isMaster()){
                assert(!allInterfaces[req->fromID]->isMaster());
                int bank = interconnectIDToL2IDMap[req->fromID];
                
                int tmpToInterface = allInterfaces[req->fromID]->getRequestDestination(numSlaveReqs[bank]);
                numSlaveReqs[bank]++;
                
                proc = interconnectIDToProcessorIDMap[tmpToInterface];
            }
            else{
                proc = interconnectIDToProcessorIDMap[req->fromID];
            }
                
            if(req->virtualStartTime == -1){
                req->proc = proc;
                    
                Tick minTag = getMinStartTag();
                Tick curFinTag = virtualFinishTimes[proc];

                req->virtualStartTime = (minTag > curFinTag ? minTag : curFinTag);
                virtualFinishTimes[proc] = req->virtualStartTime + cpu_count;
            }
            
        }
        tmpIterator++;
    }
    
    // Make sure all requests have a vitual start time set
    list<InterconnectRequest* >::iterator testIterator;
    testIterator = requestQueue.begin();
    while(testIterator != requestQueue.end()){
        if((*testIterator)->time <= candiateReqTime) assert((*testIterator)->virtualStartTime >= 0);
        testIterator++;
    }
    
    while(!requestQueue.empty()){
        list<InterconnectRequest* >::iterator tmpIterator;
        tmpIterator = requestQueue.begin();
        Tick minVirtStartTime = getMinStartTag();
        
        while(tmpIterator != requestQueue.end()){
            
            bool incremented = false;
            InterconnectRequest* req = *tmpIterator;
            
            
            if(req->time <= candiateReqTime){

                if(req->virtualStartTime == minVirtStartTime){
                    
                    int toInterfaceID = getDestinationId(req->fromID);
                    if(toInterfaceID == -1){
                        delete req; 
                        tmpIterator = requestQueue.erase(tmpIterator);
                        continue;
                    }
                    
                    // check if destination is blocked
                    if(!(allInterfaces[toInterfaceID]->isMaster()) 
                         && blockedInterfaces[interconnectIDToL2IDMap[toInterfaceID]]){
                        tmpIterator = requestQueue.erase(tmpIterator);
                        notGrantedReqs.push_back(req);
                        continue;
                    }
                    
                    int proc = allInterfaces[toInterfaceID]->isMaster() ? 
                            interconnectIDToProcessorIDMap[toInterfaceID] : interconnectIDToProcessorIDMap[req->fromID];
                    
                    int position = 0;
                    int destination = toInterfaceID;
                    if(req->proc != proc){
                        assert(!allInterfaces[req->fromID]->isMaster());
                        for(int i=0;i<requestQueue.size();i++){
                            int tmpDestination = allInterfaces[req->fromID]->getRequestDestination(i);
                            if(interconnectIDToProcessorIDMap[tmpDestination] == req->proc){
                                position = i;
                                destination = tmpDestination;
                                break;
                            }
                        }
                    }
                    
                    bool grantAccess = checkCrossbarState(req,
                                                          (position == 0 ? toInterfaceID : destination),
                                                          &occupiedEndNodes,
                                                          &busIsUsed,
                                                          cycle);
                
                    if(grantAccess){
                        
                        grantInterface(req,
                                       (position == 0 ? toInterfaceID : destination),
                                       cycle,
                                       grantedCPUs,
                                       toBanks,
                                       destinationAddrs,
                                       currentCommands,
                                       position);
                    }
                    else{
                        // resource conflict, access not granted
                        notGrantedReqs.push_back(req);
                    }
                
                    // remove request from queue
                    tmpIterator = requestQueue.erase(tmpIterator);
                    incremented = true;
                }
            }
            else{
                // not ready
                notGrantedReqs.push_back(req);
                tmpIterator = requestQueue.erase(tmpIterator);
                incremented = true;
            }
            
            if(!incremented) tmpIterator++;
        }
    }
    
    // check if no requests are queued at this time
    // NOTE: there might be future requests in the queue
    bool waitingRequests = false;
    list<InterconnectRequest* >::iterator waitIterator;
    waitIterator = notGrantedReqs.begin();
    while(waitIterator != notGrantedReqs.end()){
        if((*waitIterator)->time <= candiateReqTime) waitingRequests = true;
        waitIterator++;
    }
    
    if(!waitingRequests){
        //no outstanding requests, clear v-time registers
        for(int i=0;i<cpu_count;i++) virtualFinishTimes[i] = 0;
    }
    
    notGrantedReqs.sort(reqLess());
}

void
Crossbar::grantInterface(InterconnectRequest* req,
                         int toInterfaceID,
                         Tick cycle,
                         vector<int> &grantedCPUs,
                         vector<int> &toBanks,
                         vector<Addr> &destinationAddrs,
                         vector<MemCmd> &currentCommands,
                         int position){
    // update statistics
    arbitratedRequests++;
    totalArbQueueCycles += (cycle - req->time) - arbitrationDelay;
    totalArbitrationCycles += arbitrationDelay;

    // grant access
    int grantedCPU =  (allInterfaces[req->fromID]->isMaster() ? interconnectIDToProcessorIDMap[req->fromID] 
                        : interconnectIDToProcessorIDMap[toInterfaceID]);
    grantedCPUs.push_back(grantedCPU);

    int toBank = (allInterfaces[req->fromID]->isMaster() ? interconnectIDToL2IDMap[toInterfaceID] + cpu_count
                : interconnectIDToL2IDMap[req->fromID] + cpu_count);
    toBanks.push_back(toBank);

    destinationAddrs.push_back(getDestinationAddr(req->fromID));
    currentCommands.push_back(getCurrentCommand(req->fromID));

    if(position == 0){
        allInterfaces[req->fromID]->grantData();
    }
    else{
        assert(!allInterfaces[req->fromID]->isMaster());
        allInterfaces[req->fromID]->grantData(position);
    }
    delete req;
}

bool
Crossbar::checkCrossbarState(InterconnectRequest* req,
                             int toInterfaceID,
                             std::vector<bool>* state,
                             bool* busIsUsed,
                             Tick cycle){

    // handle bus arbitration
    if(allInterfaces[toInterfaceID]->isMaster()){
        // sent to a master
        if(allInterfaces[req->fromID]->isMaster()){
            // Master to Master transfer, use bus
            if(*busIsUsed) return false;
            *busIsUsed = true;
            nextBusFreeTime = cycle + transferDelay;
            return true;
        }
    }
    
    int occToID =   (allInterfaces[toInterfaceID]->isMaster() ?
                     interconnectIDToProcessorIDMap[toInterfaceID]
                   : interconnectIDToL2IDMap[toInterfaceID] + cpu_count);
    int occFromID = (allInterfaces[req->fromID]->isMaster() ?
                     interconnectIDToProcessorIDMap[req->fromID]
                   : interconnectIDToL2IDMap[req->fromID] + cpu_count);
    
    // check if the request can be granted
    // if it can, update the state and grant access
    if(!(*state)[occFromID] && !(*state)[occToID]){
        (*state)[occToID] = true;
        (*state)[occFromID] = true;
        return true;
    }
    return false;
}


void
Crossbar::send(MemReqPtr& req, Tick time, int fromID){
    
    assert((req->size / width) <= 1);
    
    int toID = -1;
    bool busIsUsed = false;
    if(allInterfaces[fromID]->isMaster() && req->toInterfaceID != -1){
        busIsUsed = true;
        toID = req->toInterfaceID;
    }
    else if(allInterfaces[fromID]->isMaster()){
        toID = getTarget(req->paddr);
    }
    else{
        toID = req->fromInterfaceID;
    }
    
    //update profile stats
    if(doProfiling){
        if(busIsUsed){
            // the coherence bus is not pipelined
            channelUseCycles[cpu_count + slaveInterfaces.size()] += transferDelay;
        }
        else{
            // regular pipelined crossbar channels used
            // one pipeline slot is allocated in both sender
            // and recievers channel
            allInterfaces[fromID]->isMaster() ?
                    channelUseCycles[interconnectIDToProcessorIDMap[fromID]] += 1 :
                    channelUseCycles[slaveInterfaces.size() +
                                     interconnectIDToL2IDMap[fromID]] += 1;
        }
    }
    
    grantQueue.push_back(new InterconnectDelivery(time, fromID, toID, req));
    scheduleDeliveryQueueEvent(time + transferDelay);
}


void
Crossbar::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){
    
    assert(!req);
    assert(toID == -1);
    assert(fromID == -1);
    
#ifdef DEBUG_CROSSBAR
    assert(isSorted(&grantQueue));
#endif //DEBUG_CROSSBAR
    
    list<InterconnectDelivery* > notDeliveredReqs;
    
    Tick legalGrantTime = cycle - transferDelay;

    /* attempt to deliver as many requests as possible */
    while(!grantQueue.empty()){
        InterconnectDelivery* delivery = grantQueue.front();
        grantQueue.pop_front();
        
        if(delivery->grantTime > legalGrantTime){
            notDeliveredReqs.push_back(delivery);
            continue;
        }
        
        if(!allInterfaces[delivery->toID]->isMaster() &&
            blockedInterfaces[interconnectIDToL2IDMap[delivery->toID]]){
            // destination is blocked, we can not deliver
            notDeliveredReqs.push_back(delivery);
            continue;
        }
        
        /* update statistics */
        sentRequests++;
        int curCpuId = delivery->req->adaptiveMHASenderID; //delivery->req->xc->cpu->params->cpu_id;
        int queueCycles = (cycle - delivery->grantTime) - transferDelay;
        
        totalTransQueueCycles += queueCycles;
        totalTransferCycles += transferDelay;
        perCpuTotalTransQueueCycles[curCpuId] += queueCycles;
        perCpuTotalTransferCycles[curCpuId] += transferDelay;
        
        
        int retval = BA_NO_RESULT;
        if(allInterfaces[delivery->toID]->isMaster()){
            allInterfaces[delivery->toID]->deliver(delivery->req);
        }
        else{
            retval = allInterfaces[delivery->toID]->access(delivery->req);
        }
        delete delivery;
    }
    
    assert(grantQueue.empty());
    grantQueue.splice(grantQueue.begin(), notDeliveredReqs);
    
    if(!grantQueue.empty()){
        if(grantQueue.front()->grantTime > legalGrantTime){
            scheduleDeliveryQueueEvent(grantQueue.front()->grantTime 
                                       + transferDelay);
        }
        else{
            scheduleDeliveryQueueEvent(cycle + 1); 
        }
    }
}

vector<int>
Crossbar::getChannelSample(){
    
    if(!doProfiling) doProfiling = true;
    
    std::vector<int> retval(channelUseCycles);
    
    for(int i=0;i<channelUseCycles.size();i++){
        channelUseCycles[i] = 0;
    }
    
    return retval;
}

void
Crossbar::writeChannelDecriptor(std::ofstream &stream){
    
    for(int i=0;i<cpu_count;i++){
        stream << "Channel " << i << ": CPU " << i << "\n";
    }
    
    for(int i=0;i<slaveInterfaces.size();i++){
        stream << "Channel " << cpu_count+i << ": " 
               << slaveInterfaces[i]->getCacheName() << "\n";
    }
    
    stream << "Channel " << cpu_count+slaveInterfaces.size() << ": Coherence bus\n";
}

std::vector<std::vector<int> > 
Crossbar::retrieveInterferenceStats(){
    return interferenceEvents;
}

void 
Crossbar::resetInterferenceStats(){
    for(int i=0;i<interferenceEvents.size();i++){
        for(int j=0;j<interferenceEvents[0].size();j++){
            interferenceEvents[i][j] = 0;
        }
    }
}

Tick
Crossbar::getMinStartTag(){
    Tick min = 10000000000ull;
    bool allEmpty = true;
    
    list<InterconnectRequest* >::iterator tmpIterator;
    tmpIterator = requestQueue.begin();
    while(tmpIterator != requestQueue.end()){
        InterconnectRequest* req = *tmpIterator;
        if(req->virtualStartTime > -1 && req->virtualStartTime < min){
            min = req->virtualStartTime;
            allEmpty = false;
        }
        tmpIterator++;
    }
    
    if(allEmpty) return 0;
    return min;
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
    INIT_PARAM(detailed_sim_start_tick, "The tick detailed simulation starts")
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
                        detailed_sim_start_tick);
}

REGISTER_SIM_OBJECT("Crossbar", Crossbar)

#endif //DOXYGEN_SHOULD_SKIP_THIS
