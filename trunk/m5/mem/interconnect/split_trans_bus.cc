
#include "sim/builder.hh"
#include "split_trans_bus.hh"

using namespace std;

void
SplitTransBus::request(Tick time, int fromID){
    
    requests++;
    
    assert(fromID >= 0);
    
    // keep linked list of requests sorted at all times
    // first request takes priority over later requests at same cycle
    InterconnectRequest* newReq = new InterconnectRequest(time, fromID);
    if(pipelined){
        if(allInterfaces[fromID]->isMaster()){
            addToList(&requestQueue, newReq);
        }
        else{
            addToList(slaveRequestQueue, newReq);
        }
    }
    else{
        addToList(&requestQueue, newReq);
    }

    
#ifdef DEBUG_SPLIT_TRANS_BUS
    checkIfSorted(&requestQueue);
    if(pipelined) checkIfSorted(slaveRequestQueue);
#endif //DEBUG_SPLIT_TRANS_BUS
    
    if(!blocked){
        
        if(!pipelined){
            if(arbitrationEvents.empty()){
                scheduleArbitrationEvent(time + arbitrationDelay);
            }
            else{
                Tick nextArbCycle = TICK_T_MAX;
                int hitIndex = -1;
                for(int i=0;i<arbitrationEvents.size();i++){
                    if(arbitrationEvents[i]->when() < nextArbCycle){
                        nextArbCycle = arbitrationEvents[i]->when();
                        hitIndex = i;
                    }
                }
                assert(nextArbCycle < TICK_T_MAX);
                
                if(nextArbCycle > (time + arbitrationDelay)){
                    /* the arbitration events are out of synch */
                    for(int i=0;i<arbitrationEvents.size();i++){
                        if(arbitrationEvents[i]->scheduled()){
                            arbitrationEvents[i]->deschedule();
                        }
                        delete arbitrationEvents[i];
                    }
                    arbitrationEvents.clear();
                    
                    scheduleArbitrationEvent(time + arbitrationDelay);
                }
    
            }
        }
        else{
            scheduleArbitrationEvent(time + arbitrationDelay);
        }
    }
}

void
SplitTransBus::addToList(std::list<InterconnectRequest*>* inList,
                         InterconnectRequest* icReq){
    
    list<InterconnectRequest*>::iterator findPos;
    for(findPos=inList->begin();
        findPos!=inList->end();
        findPos++){
            InterconnectRequest* tempReq = *findPos;
            if(icReq->time < tempReq->time) break;
        }

    inList->insert(findPos, icReq);
}

void
SplitTransBus::scheduleArbitrationEvent(Tick possibleArbCycle){
    
    assert(!blocked);
    
    bool addArbCycle = true;
    for(int i=0;i<arbitrationEvents.size();++i){
        if(arbitrationEvents[i]->when() == possibleArbCycle){
            addArbCycle = false;
        }
    }
    
    if(addArbCycle){
        InterconnectArbitrationEvent* event = 
                new InterconnectArbitrationEvent(this);
        event->schedule(possibleArbCycle);
        arbitrationEvents.push_back(event);
    }
}

void
SplitTransBus::arbitrate(Tick cycle){
    
    assert(!blocked);
    if(pipelined) assert(!requestQueue.empty() || !slaveRequestQueue->empty());
    else assert(!requestQueue.empty());
    
    if(pipelined){
        grantInterface(STB_SLAVE, cycle);
        grantInterface(STB_MASTER, cycle);
    }
    else{
        grantInterface(STB_NOT_PIPELINED, cycle);
    }
    
    
    if(!requestQueue.empty()){
        Tick nextReqTime = requestQueue.front()->time;
    
        if(pipelined){
            if(nextReqTime <= (cycle - arbitrationDelay)){
                scheduleArbitrationEvent(cycle + 1);
            }
            else{
                scheduleArbitrationEvent(nextReqTime + arbitrationDelay);
            }
        }
        else{
            if(nextReqTime <= cycle){
                scheduleArbitrationEvent(cycle + arbitrationDelay);
            }
            else{
                scheduleArbitrationEvent(nextReqTime + arbitrationDelay);
            }
        }
    }
    
    if(pipelined){
        if(!slaveRequestQueue->empty()){
            Tick nextReqTime = slaveRequestQueue->front()->time;
        
            if(nextReqTime <= (cycle - arbitrationDelay)){
                scheduleArbitrationEvent(cycle + 1);
            }
            else{
                scheduleArbitrationEvent(nextReqTime + arbitrationDelay);
            }
        }
    }
}

void
SplitTransBus::grantInterface(grant_type gt, Tick cycle){
    
    Tick goodReqTime = cycle - arbitrationDelay;
    InterconnectRequest* grantReq;
    
    /* remove the request from the correct queue */
    switch(gt){
        case STB_NOT_PIPELINED:
            grantReq = requestQueue.front();
            requestQueue.pop_front();
            break;
        
        case STB_MASTER:
            if(requestQueue.empty()) return;
            grantReq = requestQueue.front();
            
            /* check if requests are available */
            if(grantReq->time > goodReqTime) return;
            requestQueue.pop_front();
            break;
            
        case STB_SLAVE:
            if(slaveRequestQueue->empty()) return;
            grantReq = slaveRequestQueue->front();
            
            /* check if requests are available */
            if(grantReq->time > goodReqTime) return;
            slaveRequestQueue->pop_front();
            break;
            
        default:
            fatal("Unknown grant_type encountered");
            
    }
    
    /* grant access */
    allInterfaces[grantReq->fromID]->grantData();
    
    /* update statistics */
    arbitratedRequests++;
    totalArbQueueCycles += ((cycle - grantReq->time) - arbitrationDelay);
    totalArbitrationCycles += arbitrationDelay;
    
    delete grantReq;
}


void 
SplitTransBus::send(MemReqPtr& req, Tick time, int fromID){
    
    assert(!blocked);
    assert((req->size / width) <= 1);
    
    bool isFromMaster = false;
    if(allInterfaces[fromID]->isMaster()) isFromMaster = true;
    
    if(req->toInterfaceID != -1){
        // L1 to L1 request
        deliverQueue.push_back(
                new InterconnectDelivery(time, 
                                         fromID,
                                         req->toInterfaceID,
                                         req));
    }
    else if(isFromMaster){
        // Try all slaves and check if they can supply the needed data
        int successCount = 0;
        int toID = -1;
        for(int i=0;i<allInterfaces.size();++i){

            if(allInterfaces[i]->isMaster()) continue;
            
            if(allInterfaces[i]->inRange(req->paddr)){
                successCount++;
                toID = i;
            }
        }
        
        if(successCount == 0){
            fatal("No supplier for data on SplitTransBus");
        }
        if(successCount > 1){
            fatal("More than one supplier for data on SplitTransBus");
        }
        
        /* deliver to L2 cache */
        deliverQueue.push_back(
                new InterconnectDelivery(time, fromID, toID, req));
    }
    else{
        /* deliver to L1 cache */
        deliverQueue.push_back(
                new InterconnectDelivery(time,
                                         fromID,
                                         req->fromInterfaceID,
                                         req));
    }
    
#ifdef DEBUG_SPLIT_TRANS_BUS
    /* check that the queue is sorted */
    InterconnectDelivery* prev = NULL;
    bool first = true;
    for(list<InterconnectDelivery*>::iterator i=deliverQueue.begin();
        i!=deliverQueue.end();
        i++){
            if(first){
                first = false;
                prev = *i;
                continue;
            }
            
            assert(prev->grantTime <= (*i)->grantTime);
            prev = *i;
        }
#endif //DEBUG_SPLIT_TRANS_BUS
    
    if(doProfile) useCycleSample += transferDelay;
        
    scheduleDeliverEvent(time + transferDelay);
}

void
SplitTransBus::scheduleDeliverEvent(Tick possibleArbCycle){
    
    bool addEvent = true;
    for(int i=0;i < deliverEvents.size();i++){
        if(deliverEvents[i]->when() == possibleArbCycle){
            addEvent = false;
        }
    }
    
    if(addEvent){
        
        InterconnectDeliverQueueEvent* event = 
                new InterconnectDeliverQueueEvent(this);
        event->schedule(possibleArbCycle);
        deliverEvents.push_back(event);
    }
}


void
SplitTransBus::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){

    assert(!blocked);
    assert(!deliverQueue.empty());
    
    InterconnectDelivery* delivery = deliverQueue.front();
    deliverQueue.pop_front();
    
    /* update statistics */
    sentRequests++;
    int queueTime = (cycle - delivery->grantTime) - transferDelay;
    totalTransQueueCycles += queueTime;
    totalTransferCycles += transferDelay;
    
    int curCpuId = delivery->req->xc->cpu->params->cpu_id;
    perCpuTotalTransQueueCycles[curCpuId] += queueTime;
    perCpuTotalTransferCycles[curCpuId] += transferDelay;
    
    int retval = BA_NO_RESULT;
    assert(delivery->toID > -1);
    if(allInterfaces[delivery->toID]->isMaster()){
        allInterfaces[delivery->toID]->deliver(delivery->req);
    }
    else{
        retval = allInterfaces[delivery->toID]->access(delivery->req);
    }
    
    delete delivery;
    
    if(retval != BA_BLOCKED){
        /* see if we need to schedule another delivery */
        if(!deliverQueue.empty()){
            InterconnectDelivery* nextDelivery = deliverQueue.front();
            if(nextDelivery->grantTime <= (cycle - transferDelay)){
                if(pipelined) scheduleDeliverEvent(cycle + 1); 
                else scheduleDeliverEvent(cycle + transferDelay);
            }
            else{
                scheduleDeliverEvent(nextDelivery->grantTime + transferDelay);
            }
        }
    }
}

void
SplitTransBus::setBlocked(int fromInterface){
    
    if(blocked) warn("SplitTransBus blocking on a second cause");
    
    blocked = true;
    numSetBlocked++;
    waitingFor = fromInterface;
    
    /* remove all scheduled arbitration events */
    for(int i=0;i<arbitrationEvents.size();++i){
        if (arbitrationEvents[i]->scheduled()) {
            arbitrationEvents[i]->deschedule();
        }
        delete arbitrationEvents[i];
    }
    arbitrationEvents.clear();
    
    /* remove all deliver events */
    for(int i=0;i<deliverEvents.size();i++){
        if(deliverEvents[i]->scheduled()){
            deliverEvents[i]->deschedule();
        }
        delete deliverEvents[i];
    }
    deliverEvents.clear();
    
    blockedAt = curTick;
}

void
SplitTransBus::clearBlocked(int fromInterface){
    
    assert(blocked);
    assert(blockedAt >= 0);
    if (blocked && waitingFor == fromInterface) {
        blocked = false;
        
        if(!requestQueue.empty()){
            Tick min = requestQueue.front()->time;
            
            if(min >= curTick){
                scheduleArbitrationEvent(min + arbitrationDelay);
            }
            else{
                scheduleArbitrationEvent(curTick + arbitrationDelay);
            }
            
        }
        
        if(pipelined){
            if(!slaveRequestQueue->empty()){
                Tick min = slaveRequestQueue->front()->time;
                if(min >= curTick){
                    scheduleArbitrationEvent(min + arbitrationDelay);
                }
                else{
                    scheduleArbitrationEvent(curTick + arbitrationDelay);
                }
            }
        }
        
        if(!deliverQueue.empty()){
            Tick min = deliverQueue.front()->grantTime;
            if(min >= curTick){
                scheduleDeliverEvent(min + transferDelay);
            }
            else{
                scheduleDeliverEvent(curTick + transferDelay);
            }
        }
        
        numClearBlocked++;
        
        blockedAt = -1;
    }
}

vector<int>
SplitTransBus::getChannelSample(){
    
    if(!doProfile) doProfile = true;
    
    std::vector<int> retval(1, 0);
    retval[0] = useCycleSample;
    useCycleSample = 0;
    
    return retval;
}

#ifdef DEBUG_SPLIT_TRANS_BUS

void
SplitTransBus::checkIfSorted(std::list<InterconnectRequest* >* inList){
    /* check that the queue is sorted */
    InterconnectRequest* prev = NULL;
    bool first = true;
    for(list<InterconnectRequest*>::iterator i=inList->begin();
        i!=inList->end();
        i++){
            if(first){
                first = false;
                prev = *i;
                continue;
            }
            
            assert(prev->time <= (*i)->time);
            prev = *i;
    }
}

void
SplitTransBus::printRequestQueue(){
    cout << "Request queue: ";
    for(list<InterconnectRequest*>::iterator i = requestQueue.begin();
        i != requestQueue.end();
        i++){
        cout << "(" 
                << (*i)->fromID 
                << ", " 
                << (*i)->time 
                << ") ";
    }
    cout << "\n";
    
    if(pipelined){
        cout << "Slave request queue: ";
        for(list<InterconnectRequest*>::iterator i = slaveRequestQueue->begin();
            i != slaveRequestQueue->end();
            i++){
                cout << "(" 
                        << (*i)->fromID 
                        << ", " 
                        << (*i)->time 
                        << ") ";
        }
        cout << "\n";
    }
}

void
SplitTransBus::printDeliverQueue(){
    cout << "Deliver queue: ";
    for(list<InterconnectDelivery*>::iterator i = deliverQueue.begin();
        i != deliverQueue.end();
        i++){
            cout << "(" 
                    << (*i)->fromID 
                    << ", " 
                    << (*i)->toID 
                    << ", " 
                    << (*i)->grantTime 
                    << ") ";
    }
    cout << "\n";
}

#endif //DEBUG_SPLIT_TRANS_BUS

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(SplitTransBus)
        Param<int> width;
        Param<int> clock;
        Param<int> transferDelay;
        Param<int> arbitrationDelay;
        Param<int> cpu_count;
        Param<bool> pipelined;
        SimObjectParam<HierParams *> hier;
END_DECLARE_SIM_OBJECT_PARAMS(SplitTransBus)

BEGIN_INIT_SIM_OBJECT_PARAMS(SplitTransBus)
        INIT_PARAM(width, "bus width in bytes"),
        INIT_PARAM(clock, "bus clock"),
        INIT_PARAM(transferDelay, "bus transfer delay in CPU cycles"),
        INIT_PARAM(arbitrationDelay, "bus arbitration delay in CPU cycles"),
        INIT_PARAM(cpu_count, "the number of CPUs in the system"),
        INIT_PARAM(pipelined, "true if the bus has pipelined arbitration "
                              "and transmission"),
        INIT_PARAM_DFLT(hier,
                        "Hierarchy global variables",
                        &defaultHierParams)
END_INIT_SIM_OBJECT_PARAMS(SplitTransBus)

CREATE_SIM_OBJECT(SplitTransBus)
{
    return new SplitTransBus(getInstanceName(),
                             width,
                             clock,
                             transferDelay,
                             arbitrationDelay,
                             cpu_count,
                             pipelined,
                             hier);
}

REGISTER_SIM_OBJECT("SplitTransBus", SplitTransBus)

#endif //DOXYGEN_SHOULD_SKIP_THIS
