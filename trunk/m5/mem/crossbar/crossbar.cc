
#include "mem/crossbar/crossbar.hh"
#include "sim/builder.hh"
#include "mem/base_hier.hh"

using namespace std;

/** The maximum value of type Tick. */
#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)

Crossbar::Crossbar(const string& name, 
                   int width,
                   int clock,
                   int transDelay,
                   int arbDelay,
                   int L1cacheCount,
                   HierParams *hier)
    : BaseHier(name, hier)
{
    
    crossbarWidth = width;
    crossbarClock = clock;
    crossbarDelay = transDelay;
    arbitrationDelay = arbDelay;
    numL1Caches = L1cacheCount;
    
    blocked = false;
    
    if(crossbarClock != 1){
        fatal("The crossbar is only implemented to run at the same frequency as the CPU core");
    }
    
    interfaceCount = -1;
    l2InterfaceID = -1;
    blockedAt = -1;
}

Crossbar::~Crossbar(){
    /* does nothing */
}

void
Crossbar::regStats(){
    
    using namespace Stats;
    
    requests
            .name(name() + ".requests")
            .desc("number of crossbar requests")
            ;
    
    totalArbitrationCycles
            .name(name() + ".arbitrationCycles")
            .desc("number cycles used for giving access to the crossbar")
            ;
    
    lineUseCycles
            .name(name() + ".line_use_cycles")
            .desc("total number of cycles the diffent lines are in use")
            ;
    
    queueCycles
            .name(name() + ".queue_cycles")
            .desc("total number of cycles the interfaces has been waiting in queue")
            ;
    
    nullRequests
            .name(name() + ".nullRequests")
            .desc("number of pointless crossbar requests")
            ;
    
    duplicateRequests
            .name(name() + ".duplicateRequests")
            .desc("number of duplicate crossbar requests")
            ;
    
    numClearBlocked
            .name(name() + ".numClearBlocked")
            .desc("number of times the crossbar is cleared from blocking")
            ;
    
    numSetBlocked
            .name(name() + ".numSetBlocked")
            .desc("number of times the crossbar is set blocked")
            ;
}

void
Crossbar::resetStats(){
    // Currently not needed
}

int
Crossbar::getL2interfaceID(){
    assert(l2InterfaceID >= 0);
    return l2InterfaceID;
}

int
Crossbar::registerInterface(CrossbarInterface* interface,
                            bool isL2){
    
    ++interfaceCount;
    interfaces.push_back(interface);
    assert(interfaceCount == (interfaces.size()-1));

    if(isL2){
        l2InterfaceID = interfaceCount;
    }

//     assert(interfaceCount < (numL1Caches+1));
    return interfaceCount;
}

void
Crossbar::rangeChange(){
    for(int i=0;i<interfaces.size();++i){
        list<Range<Addr> > range_list;
        interfaces[i]->getRange(range_list);
    }
}

void
Crossbar::request(Tick cycle, int toID, int fromID){

//     cout << "CROSSBAR: request at " << cycle << " from " << fromID << " to " << toID << "\n";
    
    //cout << "request at" << cycle << "\n";
    assert(l2InterfaceID >= 0);
    assert(toID >= 0);
    
    // update stats
    requests++;
    
    requestQueue.push_back(new CrossbarRequest(cycle, fromID));
    
    scheduleArbitrationEvent(cycle);
}

void
Crossbar::scheduleArbitrationEvent(Tick now){
    
//     printArbCycles();
    /* do not schedule arbitration events while we are blocked */
    if(blocked){
        return;
    }
    
//     printArbCycles();
    
    int arbCycle = now + arbitrationDelay;
    bool addArbCycle = true;
    for(int i=0;i<arbitrationCycles.size();++i){
        if(arbitrationCycles[i] == arbCycle) addArbCycle = false;
    }
    
    if(addArbCycle){
        CrossbarArbitrationEvent* event = new CrossbarArbitrationEvent(this);
        event->schedule(arbCycle);
        arbitrationCycles.push_back(arbCycle);
        arbitrationEvents.push_back(event);
    }
}


void
Crossbar::arbitrateCrossbar(Tick cycle){

    assert(!blocked);
    
//     cout << "CROSSBAR: arbitrating at " << cycle << "\n";
    
    vector<Tick>::iterator thisCycle;
    int hits = 0;
    for(vector<Tick>::iterator i = arbitrationCycles.begin(); i != arbitrationCycles.end(); ++i){
        if(*i == cycle){
            thisCycle = i;
            hits++;
        }
    }
    assert(hits == 1);
    arbitrationCycles.erase(thisCycle);
    
    Tick nextArbTime = -1;
    CrossbarRequest* grantReq = (CrossbarRequest*) getRequest(cycle, &nextArbTime);
    interfaces[grantReq->fromID]->grantData();
    
    if(nextArbTime >= 0){
        scheduleArbitrationEvent(nextArbTime);
    }
    
    queueCycles += (curTick - grantReq->time);
    delete grantReq;
    
//     printRequestQueues();
//     printArbCycles();
}

void*
Crossbar::getRequest(Tick time, Tick* nextArbitrationTime){
    assert(time < TICK_T_MAX);
    
    Tick minCycle = TICK_T_MAX;
    int foundIndex = -1;
    bool noneFound = true;
    for(int i=0;i<requestQueue.size();++i){
        CrossbarRequest* reqObj = requestQueue[i];
        if(reqObj->time < minCycle && (reqObj->time + arbitrationDelay) <= time){
            minCycle = reqObj->time;
            foundIndex = i;
            noneFound = false;
        }
    }
    
    /* remove request from queue */
    CrossbarRequest* returnReq = NULL;
    if(!noneFound){
        assert(foundIndex >= 0);
        returnReq = requestQueue[foundIndex];
        requestQueue.erase(requestQueue.begin()+foundIndex);
    }
    
    /* find the first request left in the queue it it exists */
    Tick smallest = TICK_T_MAX;
    for(int i=0;i<requestQueue.size();++i){
        CrossbarRequest* reqObj = requestQueue[i];
        if(reqObj->time < smallest){
            smallest = reqObj->time;
        }
    }
    
    /* return the next arbitration time and the current request object */
    if(smallest < TICK_T_MAX){
        if(smallest < curTick) smallest = curTick;
        *nextArbitrationTime = smallest;
    }
    assert(returnReq != NULL);
    return returnReq;
}


void
Crossbar::sendData(MemReqPtr &req, Tick time, int toID, int fromID){

    assert(!blocked);
    
//     cout << "CROSSBAR: sending data at " << time << " from " << fromID << " to " << toID << "\n";
    
    int transferCount = req->size / crossbarWidth;
    if(transferCount == 0) transferCount = 1;
    
    if(toID == l2InterfaceID){
        int retval = interfaces[toID]->access(req);
        if(retval == BA_BLOCKED){
            /* the L2 cache will call setBlocked, so we don't need to do it here */
            return;
        }
    }
    else{
        /* the L2 access has allready been accessed */
        CrossbarDeliverEvent* event = new CrossbarDeliverEvent(this, req, toID, fromID);
        event->schedule(time + (transferCount*crossbarDelay));
    }
        
    lineUseCycles += (transferCount * crossbarDelay);
}

void
Crossbar::deliverData(MemReqPtr &req, Tick cycle, int toID, int fromID){
    //if(toID == l2InterfaceID) interfaces[toID]->access(req);
    assert(toID != l2InterfaceID);
    interfaces[toID]->deliver(req);
}

void
Crossbar::incNullRequests(){
    nullRequests++;
}
        
void
Crossbar::incDuplicateRequests(){
    duplicateRequests++;
}

void
Crossbar::setBlocked(int fromInterface){
//     cout << "CROSSBAR: set blocked called from " << fromInterface << " at cycle " << curTick << "\n";
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
    arbitrationCycles.clear();
    
    assert(blockedAt == -1);
    blockedAt = curTick;
}

void
Crossbar::clearBlocked(int fromInterface){
//     cout << "CROSSBAR: clear blocked called from interface " << fromInterface << " at cycle " << curTick << "\n";

    assert(blocked);
    assert(blockedAt >= 0);
    if (blocked && waitingFor == fromInterface) {
        blocked = false;
        
        if(!requestQueue.empty()){
            Tick min = TICK_T_MAX;
            for(int i=0;i<requestQueue.size();++i){
                if(requestQueue[i]->time < min) min = requestQueue[i]->time;
            }
            assert(min < TICK_T_MAX);
            if(min >= curTick) scheduleArbitrationEvent(min);
            else scheduleArbitrationEvent(curTick);
            
        }
        numClearBlocked++;
    }
    
    blockedAt = -1;
}

/**********************************************/
/* DEBUG METHODS                              */

void
Crossbar::printRequestQueue(){
    cout << "Request queue: ";
    for(int i=0;i<requestQueue.size();++i){
        cout << "(" << requestQueue[i]->fromID << ", " << requestQueue[i]->time << ") ";
    }
    cout << "\n";
}

void
Crossbar::printArbCycles(){
    cout << "Arb cycles: ";
    for(int i=0;i<arbitrationCycles.size();++i){
        cout << arbitrationCycles[i] << " ";
    }
    cout << "\n";
}


/*********************************************/

void
CrossbarArbitrationEvent::process(){
    int foundIndex = -1;
    int eventHitCount = 0;
    for(int i=0;i<crossbar->arbitrationEvents.size();++i){
        if(crossbar->arbitrationEvents[i] == this){
            foundIndex = i;
            eventHitCount++;
        }
    }
    assert(foundIndex >= 0);
    assert(eventHitCount == 1);
    crossbar->arbitrationEvents.erase(crossbar->arbitrationEvents.begin()+foundIndex);
    
    crossbar->arbitrateCrossbar(this->when());
    delete this;
}

const char*
CrossbarArbitrationEvent::description(){
    return "Crossbar arbitration event";
}

void
CrossbarDeliverEvent::process(){
    crossbar->deliverData(this->req, this->when(), this->toID, this->fromID);
    delete this;
}

const char*
CrossbarDeliverEvent::description(){
    return "Crossbar deliver event";
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Crossbar)
        Param<int> delay;
        Param<int> clock;
        Param<int> width;
        Param<int> arbDelay;
        Param<int> L1CacheCount;
        SimObjectParam<HierParams *> hier;
END_DECLARE_SIM_OBJECT_PARAMS(Crossbar)

BEGIN_INIT_SIM_OBJECT_PARAMS(Crossbar)
        INIT_PARAM(delay, "crossbar delay in CPU cycles"),
        INIT_PARAM(clock, "crossbar clock"),
        INIT_PARAM(width, "crossbar width in bytes"),
        INIT_PARAM(arbDelay, "crossbar arbitration delay in CPU cycles"),
        INIT_PARAM(L1CacheCount, "number of L1 caches"),
        INIT_PARAM_DFLT(hier,
		    "Hierarchy global variables",
		    &defaultHierParams)
END_INIT_SIM_OBJECT_PARAMS(Crossbar)

CREATE_SIM_OBJECT(Crossbar)
{
    return new Crossbar(getInstanceName(),
                        width,
                        clock,
                        delay,
                        arbDelay,
                        L1CacheCount,
                        hier);
}

REGISTER_SIM_OBJECT("Crossbar", Crossbar)

#endif //DOXYGEN_SHOULD_SKIP_THIS

