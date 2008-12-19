
#include "sim/builder.hh"
#include "ring.hh"

using namespace std;

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
    initQueues(_cpu_count,_cpu_count);
    
    sharedCacheBankCount = 4; //FIXME: parameterize
    queueSize = 32; // FIXME: parameterize
    numberOfRequestRings = 1; // FIXME: parametrize
    numberOfResponseRings = 1;
    
    recvBufferSize = (_cpu_count/2) + (sharedCacheBankCount/2);
    
    ringRequestQueue.resize(_cpu_count, list<RingRequestEntry>());
    ringResponseQueue.resize(sharedCacheBankCount, list<RingRequestEntry>());
    
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
        vector<int> resourceReq = findMasterResourceRequirements(req, fromID);
        
        ringRequestQueue[req->adaptiveMHASenderID].push_back(RingRequestEntry(req, curTick, resourceReq));
        
        if(ringRequestQueue[req->adaptiveMHASenderID].size() == queueSize){
            fatal("ring input queue blocking not impl/tested");
            setBlockedLocal(req->adaptiveMHASenderID);
        }
        assert(ringRequestQueue[req->adaptiveMHASenderID].size() <= queueSize);
    }
    else{
        vector<int> resourceReq = findSlaveResourceRequirements(req);
        
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
Ring::findMasterResourceRequirements(MemReqPtr& req, int fromIntID){
    
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
    assert(req->toInterfaceID != -1);
    int toSlaveID = interconnectIDToL2IDMap[req->toInterfaceID];
    assert(sharedCacheBankCount == slaveInterfaces.size());
    
    int uphops = req->adaptiveMHASenderID + toSlaveID + 1;
    int downhops = (cpu_count - (req->adaptiveMHASenderID+1)) + (sharedCacheBankCount - (toSlaveID+1)) + 1;
    
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
    
    stringstream pathstr;
    for(int i=0;i<path.size();i++) pathstr << path[i] << " ";
    
    DPRINTF(Crossbar, "Ring recieved req from master icID %d, toICID %d, to slave ID %d,  proc %d, path: %s, uphops %d, downhops %d\n",
            fromIntID,
            req->toInterfaceID,
            toSlaveID,
            req->adaptiveMHASenderID,
            pathstr.str().c_str(),
            uphops,
            downhops);
    
    return path;
}

vector<int>
Ring::findSlaveResourceRequirements(MemReqPtr& req){
    fatal("slave res req not impl");
}

vector<int> 
Ring::findServiceOrder(vector<list<RingRequestEntry> >* queue){
    vector<int> order;
    order.resize(queue->size(), -1);
    
    vector<bool> marked;
    marked.resize(queue->size(), false);
    
    DPRINTF(Crossbar, "Service order: ");
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
            if(remIt->second.front() < curTick){
                cout << curTick << ": removing front element with value " << remIt->second.front() << "\n";
                remIt->second.pop_front();
            }
            assert(remIt->second.front() >= curTick);
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
    
    if(toSlave){
        int toSlaveID = interconnectIDToL2IDMap[entry.req->toInterfaceID];
        inFlightRequests[toSlaveID]++;
        if(inFlightRequests[toSlaveID] > recvBufferSize){
            fatal("issuing more reqs that reciever can handle");
        }
    }
    
    return true;
}

void 
Ring::arbitrate(Tick time){
    
    assert(curTick % arbitrationDelay == 0);
    
    // TODO: implement fw arbitration
    
    DPRINTF(Crossbar, "Ring arbitrating\n");
    for(int i=0;i<numberOfRequestRings+numberOfResponseRings;i++) removeOldEntries(i);
    
    arbitrateRing(&ringRequestQueue,0,numberOfRequestRings, true);
    arbitrateRing(&ringResponseQueue,numberOfResponseRings,numberOfRequestRings+numberOfResponseRings, true);
    
    attemptToScheduleArbEvent();
}

void 
Ring::arbitrateRing(std::vector<std::list<RingRequestEntry> >* queue, int startRingID, int endRingID, bool toSlave){
    vector<int> order = findServiceOrder(queue);
    for(int i=0;i<order.size();i++){
        for(int j=startRingID;j<endRingID;j++){
            bool sent = checkStateAndSend(ringRequestQueue[order[i]].front(), j, toSlave);
            if(sent){
                ringRequestQueue[order[i]].pop_front();
                break;
            }
        }
    }
}

void 
Ring::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){
    fatal("deliver ni");
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