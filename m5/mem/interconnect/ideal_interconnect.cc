
#include "sim/builder.hh"
#include "ideal_interconnect.hh"
        
using namespace std;

void
IdealInterconnect::send(MemReqPtr& req, Tick time, int fromID){
    
    assert(!blocked);
    assert((req->size / width) <= 1);
    
    bool isFromMaster = allInterfaces[fromID]->isMaster();
    
    if(req->toInterfaceID != -1){
        /* cache-to-cache transfer */
        assert(req->toProcessorID != -1);
        assert(req->toInterfaceID == getInterconnectID(req->toProcessorID));
        grantQueue.push_back(new InterconnectDelivery(time, 
                                                      fromID, 
                                                      req->toInterfaceID,
                                                      req));
    }
    else if(isFromMaster){
        /* reciever is a slave interface */
        int recvCount = 0;
        int recvID = -1;
        
        for(int i=0;i<allInterfaces.size();++i){
            if(allInterfaces[i]->isMaster()) continue;
            
            if(allInterfaces[i]->inRange(req->paddr)){
                recvCount++;
                recvID = i;
            }
        }
        
        /* check for errors */
        if(recvCount > 1){
            fatal("More than one supplier for address in IdealInterconnect");
        }
        if(recvCount != 1){
            fatal("No supplier for address in IdealInterconnect");
        }
        assert(recvID >= 0);
        
        grantQueue.push_back(new InterconnectDelivery(time, 
                                                      fromID, 
                                                      recvID,
                                                      req));
    }
    else{
        /* reciever is a master interface*/
        grantQueue.push_back(new InterconnectDelivery(time,
                                                      fromID,
                                                      req->fromInterfaceID,
                                                      req));
    }
    
#ifdef DEBUG_IDEAL_INTERCONNECT
    assert(isSorted(&grantQueue));
#endif //DEBUG_IDEAL_INTERCONNECT
    
    scheduleDeliveryQueueEvent(time + transferDelay);
}

void
IdealInterconnect::arbitrate(Tick cycle){
    
    assert(!blocked);
    
    list<InterconnectRequest*> notGrantedRequests;
    Tick candidateReqTime = cycle - arbitrationDelay;
    
    while(!requestQueue.empty()){
        InterconnectRequest* tmpReq = requestQueue.front();
        requestQueue.pop_front();
        
        if(tmpReq->time <= candidateReqTime){
            
            int toInterfaceID = getDestinationId(tmpReq->fromID);
            if(toInterfaceID == -1){ delete tmpReq; continue; }
            
            if(!allInterfaces[toInterfaceID]->isMaster() &&
                blockedInterfaces[interconnectIDToL2IDMap[toInterfaceID]]){
                // destination is blocked, we can not deliver
                notGrantedRequests.push_back(tmpReq);
                continue;
            }
            
            /* update statistics */
            arbitratedRequests++;
            totalArbQueueCycles += (cycle - tmpReq->time) - arbitrationDelay;
            totalArbitrationCycles += arbitrationDelay;
            
            allInterfaces[tmpReq->fromID]->grantData();
            delete tmpReq;
        }
        else{
            notGrantedRequests.push_back(tmpReq);
        }
    }
    
    assert(requestQueue.empty());
    requestQueue.splice(requestQueue.begin(), notGrantedRequests);
    
    if(!requestQueue.empty()){
        if(requestQueue.front()->time <= candidateReqTime){
            scheduleArbitrationEvent(cycle + 1);
        }
        else{
            scheduleArbitrationEvent(requestQueue.front()->time + arbitrationDelay);
        }
    }
}

void
IdealInterconnect::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){
    
    assert(!blocked);
    
    assert(!req);
    assert(toID == -1);
    assert(fromID == -1);
    
    list<InterconnectDelivery* > notDeliveredRequests;
    Tick legalGrantTime = cycle - transferDelay;
    
    while(!grantQueue.empty()){
        InterconnectDelivery* tempGrant = grantQueue.front();
        grantQueue.pop_front();
        
        if(tempGrant->grantTime > legalGrantTime){
            notDeliveredRequests.push_back(tempGrant);
            continue;
        }
        
        // check if destination is blocked
        if(!(allInterfaces[tempGrant->toID]->isMaster()) 
             && blockedInterfaces[interconnectIDToL2IDMap[tempGrant->toID]]){
            // the destination cache is blocked, so we can not deliver to it
            notDeliveredRequests.push_back(tempGrant);
            continue;
        }
        
        // update statistics
        sentRequests++;
        int queueCycles = (cycle - tempGrant->grantTime) - transferDelay;
        int curCpuId = tempGrant->req->xc->cpu->params->cpu_id;
        
        totalTransQueueCycles += queueCycles;
        totalTransferCycles += transferDelay;
        perCpuTotalTransQueueCycles[curCpuId] += queueCycles;
        perCpuTotalTransferCycles[curCpuId] += transferDelay;
        
        if(allInterfaces[tempGrant->toID]->isMaster()){
            allInterfaces[tempGrant->toID]->deliver(tempGrant->req);
        }
        else{
            allInterfaces[tempGrant->toID]->access(tempGrant->req);
        }
        
        delete tempGrant;
    }

    assert(grantQueue.empty());
    grantQueue.splice(grantQueue.begin(), notDeliveredRequests);
    
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

#ifdef DEBUG_IDEAL_INTERCONNECT

void
IdealInterconnect::printRequestQueue(){
    cout << "ReqQueue: ";
    for(list<InterconnectRequest*>::iterator i = requestQueue.begin();
        i != requestQueue.end();
        i++){
            cout << "(" << (*i)->time << ", " << (*i)->fromID << ") ";
    }
    cout << "\n";
}

void
IdealInterconnect::printGrantQueue(){
    cout << "GrantQueue: ";
    for(list<InterconnectDelivery*>::iterator i = grantQueue.begin();
        i != grantQueue.end();
        i++){
            cout << "(" 
                    << (*i)->grantTime 
                    << ", " 
                    << (*i)->fromID 
                    << ", " 
                    << (*i)->toID 
                    << ") ";
    }
    cout << "\n";
}

#endif //DEBUG_IDEAL_INTERCONNECT

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(IdealInterconnect)
    Param<int> width;
    Param<int> clock;
    Param<int> transferDelay;
    Param<int> arbitrationDelay;
    Param<int> cpu_count;
    SimObjectParam<HierParams *> hier;
END_DECLARE_SIM_OBJECT_PARAMS(IdealInterconnect)

BEGIN_INIT_SIM_OBJECT_PARAMS(IdealInterconnect)
    INIT_PARAM(width, "ideal interconnect width, set this to the cache line "
                      "width"),
    INIT_PARAM(clock, "ideal interconnect clock"),
    INIT_PARAM(transferDelay, "ideal interconnect transfer delay in CPU "
                              "cycles"),
    INIT_PARAM(arbitrationDelay, "ideal interconnect arbitration delay in CPU "
                                 "cycles"),
    INIT_PARAM(cpu_count, "the number of CPUs in the system"),
    INIT_PARAM_DFLT(hier,
                    "Hierarchy global variables",
                    &defaultHierParams)
END_INIT_SIM_OBJECT_PARAMS(IdealInterconnect)

CREATE_SIM_OBJECT(IdealInterconnect)
{
    return new IdealInterconnect(getInstanceName(),
                                 width,
                                 clock,
                                 transferDelay,
                                 arbitrationDelay,
                                 cpu_count,
                                 hier);
}

REGISTER_SIM_OBJECT("IdealInterconnect", IdealInterconnect)

#endif //DOXYGEN_SHOULD_SKIP_THIS
