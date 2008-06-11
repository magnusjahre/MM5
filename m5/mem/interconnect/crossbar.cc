
#include "sim/builder.hh"
#include "crossbar.hh"
        
using namespace std;

void
Crossbar::arbitrate(Tick cycle){
    
    list<InterconnectRequest* > notGrantedReqs;
    vector<bool> occupiedEndNodes(cpu_count + slaveInterfaces.size(), false);
    
    bool busIsUsed = false;
    if(cycle <= nextBusFreeTime) busIsUsed = true;
    
    Tick candiateReqTime = cycle - arbitrationDelay;
    
    vector<int> grantedCPUs;
    vector<int> toBanks;
    vector<Addr> destinationAddrs;
    vector<MemCmd> currentCommands;
    
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
            bool grantAccess = checkCrossbarState(req,
                                                  toInterfaceID,
                                                  &occupiedEndNodes,
                                                  &busIsUsed,
                                                  cycle);
            
            if(grantAccess){
                // update statistics
                arbitratedRequests++;
                totalArbQueueCycles += (cycle - req->time) - arbitrationDelay;
                totalArbitrationCycles += arbitrationDelay;
                
                // grant access
                int grantedCPU =  (allInterfaces[req->fromID]->isMaster() ?
                                   interconnectIDToProcessorIDMap[req->fromID]
                                 : interconnectIDToProcessorIDMap[toInterfaceID]);
                grantedCPUs.push_back(grantedCPU);
                
                int toBank = (allInterfaces[req->fromID]->isMaster() ?
                              interconnectIDToL2IDMap[toInterfaceID] + cpu_count
                            : interconnectIDToL2IDMap[req->fromID] + cpu_count);
                toBanks.push_back(toBank);
                
                destinationAddrs.push_back(getDestinationAddr(req->fromID));
                currentCommands.push_back(getCurrentCommand(req->fromID));
                
//                 cout << curTick << ": Granting access to CPU" << grantedCPU << ", addr " << hex << getDestinationAddr(req->fromID) << dec << "\n";
                allInterfaces[req->fromID]->grantData();
                delete req;
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
    
    //measure interference
    if(adaptiveMHA != NULL){
        for(int i=0;i<grantedCPUs.size();i++){
            
            std::vector<bool> isBlocking(cpu_count, false);
            
            if(!notGrantedReqs.empty()){
                list<InterconnectRequest* >::iterator notGrantedIterator;
                notGrantedIterator = notGrantedReqs.begin();
                
                while(notGrantedIterator != notGrantedReqs.end()){
                    InterconnectRequest* tmpReq = *notGrantedIterator;
                    
                    int cpuID = -1;
                    (allInterfaces[tmpReq->fromID]->isMaster() 
                            ? cpuID = interconnectIDToProcessorIDMap[tmpReq->fromID] 
                            : cpuID = interconnectIDToProcessorIDMap[getDestinationId(tmpReq->fromID)]);
                    assert(cpuID >= 0 && cpuID < cpu_count);
                    
                    int bankID = -1;
                    (allInterfaces[tmpReq->fromID]->isMaster() 
                            ? bankID = interconnectIDToL2IDMap[getDestinationId(tmpReq->fromID)] + cpu_count
                            : bankID = interconnectIDToL2IDMap[tmpReq->fromID] + cpu_count);
                    
                    if(bankID == toBanks[i] && cpuID != grantedCPUs[i]) isBlocking[cpuID] = true;
                    
                    
                    notGrantedIterator++;
                }
                
                vector<vector<Tick> > queueWaitBuffer = vector<vector<Tick> >(cpu_count, vector<Tick>(cpu_count, 0));
                
                for(int j=0;j<cpu_count;j++){
                    if(isBlocking[j]){
                        queueWaitBuffer[j][grantedCPUs[i]] = transferDelay + arbitrationDelay;
                    }
                }
                
                adaptiveMHA->addInterferenceDelay(queueWaitBuffer,
                                                  destinationAddrs[i],
                                                  currentCommands[i],
                                                  grantedCPUs[i],
                                                  INTERCONNECT_INTERFERENCE);
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Crossbar)
    Param<int> width;
    Param<int> clock;
    Param<int> transferDelay;
    Param<int> arbitrationDelay;
    Param<int> cpu_count;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
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
    INIT_PARAM_DFLT(adaptive_mha, "AdaptiveMHA object", NULL)
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
                        adaptive_mha);
}

REGISTER_SIM_OBJECT("Crossbar", Crossbar)

#endif //DOXYGEN_SHOULD_SKIP_THIS
