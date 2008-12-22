
#include "sim/builder.hh"
#include "ring.hh"

using namespace std;

#define CHECK_RING_ORDERING

Ring::Ring(const std::string &_name, 
                               int _width, 
                               int _clock,
                               int _transDelay,
                               int _arbDelay,
                               int _cpu_count,
                               HierParams *_hier,
                               AdaptiveMHA* _adaptiveMHA)
    : AddressDependentIC(_name,
                         _width, 
                         _clock, 
                         _transDelay, 
                         _arbDelay,
                         _cpu_count,
                         _hier,
                         _adaptiveMHA)
{
    
    sharedCacheBankCount = 4; //FIXME: parameterize
    queueSize = 32; // FIXME: parameterize
    numberOfRequestRings = 1; // FIXME: parametrize
    numberOfResponseRings = 1;
    
    initQueues(_cpu_count,_cpu_count + sharedCacheBankCount);
    
    recvBufferSize = (_cpu_count/2) + (sharedCacheBankCount/2);
    
    ringRequestQueue.resize(_cpu_count, list<RingRequestEntry>());
    ringResponseQueue.resize(sharedCacheBankCount, list<RingRequestEntry>());
    deliverBuffer.resize(sharedCacheBankCount, list<RingRequestEntry>());
    
    inFlightRequests.resize(sharedCacheBankCount, 0);
    
    assert(_arbDelay != -1);
    assert(_transDelay == _arbDelay);
    arbEvent = new ADIArbitrationEvent(this);
    
    TOP_LINK_ID = 2000;
    BOTTOM_LINK_ID = 3000;
    
    ringLinkOccupied.resize(numberOfRequestRings + numberOfResponseRings, map<int,list<Tick> >());
    
    // initialize occupation storage
    for(int r=0;r<numberOfRequestRings+numberOfResponseRings;r++){
        for(int i=0;i<_cpu_count-1;i++){
            ringLinkOccupied[r][i] = list<Tick>();
        }
        for(int i=0;i<sharedCacheBankCount-1;i++){
            ringLinkOccupied[r][_cpu_count+i] = list<Tick>();
        }
        ringLinkOccupied[r][TOP_LINK_ID] = list<Tick>();
        ringLinkOccupied[r][BOTTOM_LINK_ID] = list<Tick>();
    }
}

void
Ring::send(MemReqPtr& req, Tick time, int fromID){
    
    req->inserted_into_crossbar = curTick;
    
    if(allInterfaces[fromID]->isMaster()){
       vector<int> resourceReq = findResourceRequirements(req, fromID);
        
        ringRequestQueue[req->adaptiveMHASenderID].push_back(RingRequestEntry(req, curTick, resourceReq));
        
        if(ringRequestQueue[req->adaptiveMHASenderID].size() == queueSize){
            setBlockedLocal(req->adaptiveMHASenderID);
        }
        assert(ringRequestQueue[req->adaptiveMHASenderID].size() <= queueSize);
    }
    else{
      vector<int> resourceReq = findResourceRequirements(req,fromID);
        
        int slaveID = interconnectIDToL2IDMap[fromID];
        ringResponseQueue[slaveID].push_back(RingRequestEntry(req, curTick, resourceReq));
        assert(ringResponseQueue.size() <= queueSize);
    }
    
    attemptToScheduleArbEvent();
}

void 
Ring::attemptToScheduleArbEvent(){
    if(!arbEvent->scheduled()){
        Tick nextTime = curTick + (arbitrationDelay - (curTick % arbitrationDelay));
        arbEvent->schedule(nextTime);
        DPRINTF(Crossbar,"Scheduling arbitration event for tick %d\n", nextTime);
    }
}

vector<int>
Ring::findResourceRequirements(MemReqPtr& req, int fromIntID){
    
    if(allInterfaces[fromIntID]->isMaster()){
        int toInterfaceID = -1;
        for(int i=0;i<slaveInterfaces.size();i++){
            if(slaveInterfaces[i]->inRange(req->paddr)){
                if(toInterfaceID != -1) fatal("two suppliers for same address in ring");
                toInterfaceID = L2IDMapToInterconnectID[i];
            }
        }
        req->toInterfaceID = toInterfaceID;
    }
    
    vector<int> path;
    int slaveIntID = allInterfaces[fromIntID]->isMaster() ? req->toInterfaceID : req->fromInterfaceID;
    assert(slaveIntID != -1);
    int slaveID = interconnectIDToL2IDMap[slaveIntID];
    int uphops = req->adaptiveMHASenderID + slaveID + 1;
    int downhops = (cpu_count - (req->adaptiveMHASenderID+1)) + (sharedCacheBankCount - (slaveID+1)) + 1;
    
    if(allInterfaces[fromIntID]->isMaster()){   
        path = findMasterPath(req, uphops, downhops);
    }
    else{
        path = findSlavePath(req, uphops, downhops);
    }
    
    stringstream pathstr;
    for(int i=0;i<path.size();i++) pathstr << path[i] << " ";
    
    DPRINTF(Crossbar, "Ring recieved req from icID %d, to ICID %d, proc %d, path: %s, uphops %d, downhops %d\n",
            fromIntID,
            allInterfaces[fromIntID]->isMaster() ? req->toInterfaceID : -1,
            req->adaptiveMHASenderID,
            pathstr.str().c_str(),
            uphops,
            downhops);
    
    return path;
}

vector<int>
Ring::findMasterPath(MemReqPtr& req, int uphops, int downhops){
    
    vector<int> path;
    assert(req->toInterfaceID != -1);
    int toSlaveID = interconnectIDToL2IDMap[req->toInterfaceID];
    assert(sharedCacheBankCount == slaveInterfaces.size());
    
    if(uphops <= downhops){
        for(int i=(req->adaptiveMHASenderID-1);i>=0;i--) path.push_back(i);
        path.push_back(TOP_LINK_ID);
        for(int i=0;i<toSlaveID;i++) path.push_back(i+cpu_count);
    }
    else{
        for(int i=req->adaptiveMHASenderID;i<cpu_count-1;i++) path.push_back(i);
        path.push_back(BOTTOM_LINK_ID);
        for(int i=sharedCacheBankCount-2;i>=toSlaveID;i--) path.push_back(i+cpu_count);
    }
    
    return path;
}

vector<int>
Ring::findSlavePath(MemReqPtr& req, int uphops, int downhops){
    
    vector<int> path;
    
    assert(req->fromInterfaceID != -1);
    int slaveID = interconnectIDToL2IDMap[req->fromInterfaceID];
    
    if(uphops <= downhops){
        for(int i=(slaveID-1);i>=0;i--) path.push_back(i+cpu_count);
        path.push_back(TOP_LINK_ID);
        for(int i=0;i<req->adaptiveMHASenderID;i++) path.push_back(i);
    }
    else{
        for(int i=slaveID;i<sharedCacheBankCount-1;i++) path.push_back(i+cpu_count);
        path.push_back(BOTTOM_LINK_ID);
        for(int i=cpu_count-2;i>=req->adaptiveMHASenderID;i--) path.push_back(i);
    }
    
    return path;
}

vector<int> 
Ring::findServiceOrder(vector<list<RingRequestEntry> >* queue){
    vector<int> order;
    order.resize(queue->size(), -1);
    
    vector<bool> marked;
    marked.resize(queue->size(), false);
    
    if(queue == &ringRequestQueue) DPRINTF(Crossbar, "Master Service order: ");
    else DPRINTF(Crossbar, "Slave Service order: ");
    for(int i=0;i<queue->size();i++){
        
        Tick min = 1000000000000000ull;
        Tick minIndex = -1;
        
        for(int j=0;j<queue->size();j++){
            if(!marked[j] && !(*queue)[j].empty()){
                if((*queue)[j].front().enteredAt < min){
                    min = (*queue)[j].front().enteredAt;
                    minIndex = j;
                }
            }
        }
        
        if(minIndex == -1){
            for(int j=0;j<queue->size();j++){
                if(!marked[j]){
                    assert((*queue)[j].empty());
                    minIndex = j;
                    break;
                }
            }
        }
        
        assert(minIndex != -1);
        marked[minIndex] = true;
        order[i] = minIndex;
        
        DPRINTFR(Crossbar, "%d:%d ", i, minIndex);
    }
    DPRINTFR(Crossbar, "\n");
    
    return order;
}

void 
Ring::removeOldEntries(int ringID){
    map<int,list<Tick> >::iterator remIt;
    for(remIt = ringLinkOccupied[ringID].begin();remIt != ringLinkOccupied[ringID].end();remIt++){
        if(!remIt->second.empty()){
            while(!remIt->second.empty() && remIt->second.front() < curTick){
                remIt->second.pop_front();
            }
            
            if(!remIt->second.empty()) assert(remIt->second.front() >= curTick);
        }
    }
}

bool 
Ring::checkStateAndSend(RingRequestEntry entry, int ringID, bool toSlave){
    
    Tick transTick = curTick;
    for(int i=0;i<entry.resourceReq.size();i++){
        list<Tick>::iterator checkIt;
        for(checkIt = ringLinkOccupied[ringID][entry.resourceReq[i]].begin(); 
            checkIt != ringLinkOccupied[ringID][entry.resourceReq[i]].end();
            checkIt++){
            
            if(*checkIt == transTick){
                DPRINTF(Crossbar, "CPU %d not granted, conflict for link %d at %d\n", entry.req->adaptiveMHASenderID, entry.resourceReq[i], transTick);
                return false;
            }
        }
        transTick += arbitrationDelay;
    }
    
    if(toSlave){
        int toSlaveID = interconnectIDToL2IDMap[entry.req->toInterfaceID];
        
        if(inFlightRequests[toSlaveID] == recvBufferSize){
            DPRINTF(Crossbar, "CPU %d not granted, %d reqs in flight to slave %d\n", entry.req->adaptiveMHASenderID, inFlightRequests[toSlaveID], toSlaveID);
            return false;
        }
        else{
            inFlightRequests[toSlaveID]++;
        }
    }
    
    // update state
    transTick = curTick;
    for(int i=0;i<entry.resourceReq.size();i++){
        list<Tick>::iterator checkIt;
        bool inserted = false;
        for(checkIt = ringLinkOccupied[ringID][entry.resourceReq[i]].begin(); 
            checkIt != ringLinkOccupied[ringID][entry.resourceReq[i]].end();
            checkIt++){
            
            if(*checkIt > transTick){
                ringLinkOccupied[ringID][entry.resourceReq[i]].insert(checkIt, transTick);
                inserted = true;
                break;
            }
        }
        
        if(!inserted) ringLinkOccupied[ringID][entry.resourceReq[i]].push_back(transTick);
        transTick += arbitrationDelay;
    }
    
    ADIDeliverEvent* delivery = new ADIDeliverEvent(this, entry.req, toSlave);
    delivery->schedule(curTick + (arbitrationDelay * entry.resourceReq.size()));
    
    DPRINTF(Crossbar, "Granting access to CPU %d, from ICID %d, to IDID %d, latency %d, hops %d\n",
            entry.req->adaptiveMHASenderID,
            entry.req->fromInterfaceID,
            entry.req->toInterfaceID,
            arbitrationDelay * entry.resourceReq.size(),
            entry.resourceReq.size());
    
    return true;
}

void 
Ring::checkRingOrdering(int ringID){
    for(int i=0;i<ringLinkOccupied[ringID].size();i++){
        map<int, list<Tick> >::iterator mapIt;
        for(mapIt = ringLinkOccupied[ringID].begin();mapIt != ringLinkOccupied[ringID].end();mapIt++){
            list<Tick> curList = mapIt->second;
            list<Tick>::iterator checkIt;
            Tick current = curList.front();
            for(checkIt = curList.begin();checkIt != curList.end();checkIt++){
                assert(current <= *checkIt);
                current = *checkIt;
            }
        }
    }
}

void 
Ring::arbitrate(Tick time){
    
    assert(curTick % arbitrationDelay == 0);
    
    // TODO: implement fw arbitration
    
    DPRINTF(Crossbar, "Ring arbitrating\n");
    for(int i=0;i<numberOfRequestRings+numberOfResponseRings;i++) removeOldEntries(i);

#ifdef CHECK_RING_ORDERING
    for(int i=0;i<numberOfRequestRings+numberOfResponseRings;i++) checkRingOrdering(i);
#endif
    
    arbitrateRing(&ringRequestQueue,0,numberOfRequestRings, true);
    arbitrateRing(&ringResponseQueue,numberOfResponseRings,numberOfRequestRings+numberOfResponseRings, false);
    
    for(int i=0;i<ringRequestQueue.size();i++){
        if(ringRequestQueue[i].size() < queueSize && blockedLocalQueues[i]){
            clearBlockedLocal(i);
        }
    }
    
    retrieveAdditionalRequests();
    
    if(hasWaitingRequests()) attemptToScheduleArbEvent();
}

bool
Ring::hasWaitingRequests(){
    for(int i=0;i<ringRequestQueue.size();i++){
        if(!ringRequestQueue[i].empty()){
            DPRINTF(Crossbar, "CPU %d has a waiting request\n", i);
            return true;
        }
    }
    for(int i=0;i<ringResponseQueue.size();i++){
        if(!ringResponseQueue[i].empty()){
            DPRINTF(Crossbar, "Slave %d has a waiting request\n", i);
            return true;
        }
    }
    return false;
}

void 
Ring::arbitrateRing(std::vector<std::list<RingRequestEntry> >* queue, int startRingID, int endRingID, bool toSlave){
    vector<int> order = findServiceOrder(queue);
    for(int i=0;i<order.size();i++){
        for(int j=startRingID;j<endRingID;j++){
	    if(!(*queue)[order[i]].empty()){
	        bool sent = checkStateAndSend((*queue)[order[i]].front(), j, toSlave);
                if(sent){
	            (*queue)[order[i]].pop_front();
                    break;
                }
	    }
        }
    }
}

void 
Ring::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){
    if(allInterfaces[toID]->isMaster()){
        DPRINTF(Crossbar, "Delivering to master %d, req from CPU %d\n", toID, req->adaptiveMHASenderID);
        allInterfaces[toID]->deliver(req);
    }
    else{
        int toSlaveID = interconnectIDToL2IDMap[toID];
        if(!blockedInterfaces[toSlaveID] && deliverBuffer[toSlaveID].empty()){
            DPRINTF(Crossbar, "Delivering to slave IC ID %d, slave id %d, req from CPU %d, %d reqs in flight\n", toID, toSlaveID, req->adaptiveMHASenderID, inFlightRequests[toSlaveID]);
            allInterfaces[toID]->access(req);
            inFlightRequests[toSlaveID]--;
        }
        else{
            DPRINTF(Crossbar, "Slave IC ID %d (slave id %d) is blocked, delivery queued req from CPU %d, %d reqs in flight\n", toID, toSlaveID, req->adaptiveMHASenderID, inFlightRequests[toSlaveID]);
            deliverBuffer[toSlaveID].push_back(RingRequestEntry(req, curTick, vector<int>()));
            assert(deliverBuffer[toSlaveID].size() <= recvBufferSize);
        }
    }
}

void 
Ring::clearBlocked(int fromInterface){
    Interconnect::clearBlocked(fromInterface);
    
    int unblockedSlaveID = interconnectIDToL2IDMap[fromInterface];
    while(!deliverBuffer[unblockedSlaveID].empty()){
        RingRequestEntry entry = deliverBuffer[unblockedSlaveID].front();
        deliverBuffer[unblockedSlaveID].pop_front();
        
        DPRINTF(Crossbar, "Delivering to slave IC ID %d, slave id %d, req from CPU %d, %d reqs in flight, %d buffered\n", fromInterface, unblockedSlaveID, entry.req->adaptiveMHASenderID, inFlightRequests[unblockedSlaveID], deliverBuffer[unblockedSlaveID].size());
        
        MemAccessResult res = allInterfaces[fromInterface]->access(entry.req);
        inFlightRequests[unblockedSlaveID]--;
        
        if(res == BA_BLOCKED) break;
    }
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Ring)
    Param<int> width;
    Param<int> clock;
    Param<int> transferDelay;
    Param<int> arbitrationDelay;
    Param<int> cpu_count;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
END_DECLARE_SIM_OBJECT_PARAMS(Ring)

BEGIN_INIT_SIM_OBJECT_PARAMS(Ring)
    INIT_PARAM(width, "the width of the crossbar transmission channels"),
    INIT_PARAM(clock, "crossbar clock"),
    INIT_PARAM(transferDelay, "crossbar transfer delay in CPU cycles"),
    INIT_PARAM(arbitrationDelay, "crossbar arbitration delay in CPU cycles"),
    INIT_PARAM(cpu_count, "the number of CPUs in the system"),
    INIT_PARAM_DFLT(hier, "Hierarchy global variables", &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "AdaptiveMHA object", NULL)
END_INIT_SIM_OBJECT_PARAMS(Ring)

CREATE_SIM_OBJECT(Ring)
{
    return new Ring(getInstanceName(),
                              width,
                              clock,
                              transferDelay,
                              arbitrationDelay,
                              cpu_count,
                              hier,
                              adaptive_mha);
}

REGISTER_SIM_OBJECT("Ring", Ring)

#endif //DOXYGEN_SHOULD_SKIP_THIS
