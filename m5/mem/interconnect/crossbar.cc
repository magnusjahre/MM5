
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
    
    detailedSimStartTick = _detailedSimStartTick;
    crossbarTransferDelay = _transDelay + _arbDelay;
    crossbarRequests = vector<list<pair<MemReqPtr, int> > >(_cpu_count, list<pair<MemReqPtr, int> >());
    
    
    perEndPointQueueSize = 16; // FIXME: parameterize
    requestOccupancyTicks = 2; // FIXME: parameterize
    requestL2BankCount = 4;
    crossbarResponses = vector<list<pair<MemReqPtr, int> > >(requestL2BankCount, list<pair<MemReqPtr, int> >());
    
    slaveDeliveryBuffer = vector<list<pair<MemReqPtr, Tick> > >(requestL2BankCount, list<pair<MemReqPtr, Tick> >());
    
    blockedLocalQueues = vector<bool>(_cpu_count, false);
    requestsInProgress = vector<int>(requestL2BankCount, 0);
    
    notRetrievedRequests = vector<int>((_cpu_count * 2) + requestL2BankCount, 0);
    
    crossbarArbEvent = new CrossbarArbitrationEvent(this);
    
    if(requestL2BankCount + _cpu_count > 32){
        fatal("The current crossbar implementation supports maximum 32 endpoints");
    }
}

void
Crossbar::request(Tick cycle, int fromID){
    CrossbarRetrieveReqEvent* event = new CrossbarRetrieveReqEvent(this, fromID);
    event->schedule(cycle);
}

void 
Crossbar::retriveRequest(int fromInterface){
    
    DPRINTF(Crossbar, "Request recieved from interface %d, cpu %d\n", fromInterface, interconnectIDToProcessorIDMap[fromInterface]);
    
    if(!blockedLocalQueues[interconnectIDToProcessorIDMap[fromInterface]] ||
       !allInterfaces[fromInterface]->isMaster()){
        allInterfaces[fromInterface]->grantData();
    }
    else{
        notRetrievedRequests[fromInterface]++;
    }
}

void
Crossbar::send(MemReqPtr& req, Tick time, int fromID){
    
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
            assert(crossbarRequests[req->adaptiveMHASenderID].back().first->finishedInCacheAt <= req->finishedInCacheAt);
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
            assert(crossbarResponses[bankID].back().first->finishedInCacheAt <= req->finishedInCacheAt);
        }
        crossbarResponses[bankID].push_back(pair<MemReqPtr, int>(req, resources));
    }
    
    if(!crossbarArbEvent->scheduled()){
        crossbarArbEvent->schedule(curTick);
    }
}

void
Crossbar::arbitrate(Tick time){
    
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
                if(req->finishedInCacheAt < min){
                    minIndex = j;
                    min = req->finishedInCacheAt;
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
    
    fatal("implement lowest first retrieval for interfaces that share a queue");
    
    // for all processors, retrieve as many requests as possible without filling the queues
    // FIFO ordering between interfaces!!
    
//     int minIndex = -1;
//     Tick lowest = 100000000000000000ull;
//     
//     stringstream tmp;
//     int chkCnt = 0;
//     
//     for(int i=0;i<notRetrievedRequests.size();i++){
//         
//         if(notRetrievedRequests[i] > 0){
//             
//             assert(allInterfaces[i]->isMaster());
//             int fromCPU = interconnectIDToProcessorIDMap[i];
//             
//             if(!blockedLocalQueues[fromCPU]){
//                 MemReqPtr req = allInterfaces[i]->getPendingRequest();
//                 if(req){
//                     chkCnt++;
//                     tmp << curTick << " Req from " << i << ", ready at tick " << req->finishedInCacheAt << "\n";
//                     
//                     if(req->finishedInCacheAt < lowest){
//                         tmp << curTick << " Lowest!\n";
//                         lowest = req->finishedInCacheAt;
//                         minIndex = i;
//                     }
//                 }
//                 else{
//                     // null request, just grant access
//                     bool res = allInterfaces[i]->grantData();
//                     assert(!res);
//                 }
//             }
//             
//         }
//     }
//     
//     
//     if(minIndex != -1){
//         tmp << curTick << " Granting interface " << minIndex << "\n";
//         allInterfaces[minIndex]->grantData();
//         notRetrievedRequests[minIndex]--;
//     }
    
//     if(chkCnt > 1) cout << tmp.str() << "\n";
    
}

bool
Crossbar::attemptDelivery(list<pair<MemReqPtr, int> >* currentQueue, int* crossbarState, bool toSlave){

    if(!currentQueue->empty()){
        if((currentQueue->front().second & *crossbarState) == 0){
            CrossbarDeliverEvent* delivery = new CrossbarDeliverEvent(this, currentQueue->front().first, toSlave);
            delivery->schedule(curTick + crossbarTransferDelay);
            
            int fromCPU = currentQueue->front().first->adaptiveMHASenderID;
            
            if(toSlave){
                int toSlaveID = interconnectIDToL2IDMap[currentQueue->front().first->toInterfaceID];
                requestsInProgress[toSlaveID]++;
            }
            
            DPRINTF(Crossbar, "Granting access to proc %d, addr %x, cb state %x, deliver at %d\n", fromCPU, currentQueue->front().first->paddr, *crossbarState, curTick + crossbarTransferDelay);
            
            if(blockedLocalQueues[fromCPU] &&
                crossbarRequests[fromCPU].size() < perEndPointQueueSize){
                clearBlockedLocal(fromCPU);
            }
            
            *crossbarState |= currentQueue->front().second; 
            currentQueue->pop_front();
            
            return true;
        }
    }
    return false;
}

void
Crossbar::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){
    
    DPRINTF(Crossbar, "Delivering to %d from %d, proc %d, addr %x\n", toID, fromID, req->adaptiveMHASenderID, req->paddr);
    
    if(allInterfaces[toID]->isMaster()){
        allInterfaces[toID]->deliver(req);
    }
    else{
        int toSlaveID = interconnectIDToL2IDMap[toID];
        if(blockedInterfaces[toSlaveID]){
            slaveDeliveryBuffer[toSlaveID].push_back(pair<MemReqPtr, Tick>(req, curTick));
            
            DPRINTF(Crossbar, "Delivery queued, %d requests in buffer for slave %d\n", slaveDeliveryBuffer[toSlaveID].size(), toSlaveID);
            assert(slaveDeliveryBuffer[toSlaveID].size() <= crossbarTransferDelay / requestOccupancyTicks);
        }
        else{
            
            allInterfaces[toID]->access(req);
            requestsInProgress[toSlaveID]--;
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
        
        MemAccessResult res = allInterfaces[fromInterface]->access(slaveDeliveryBuffer[unblockedSlaveID].front().first);
        slaveDeliveryBuffer[unblockedSlaveID].pop_front();
        
        requestsInProgress[unblockedSlaveID]--;

        // the interface is blocked again, stop sending requests
        if(res = BA_BLOCKED) break;
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
    warn("cb retrive interference stats not impl");
    return retval;
}

void 
Crossbar::resetInterferenceStats(){
    warn("cb reset interference stats not impl");
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
