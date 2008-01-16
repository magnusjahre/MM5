
#include "mem/bus/NFQBus.hh"
#include "sim/builder.hh"

using namespace std;

NFQBus::NFQBus(const std::string &name,
               HierParams *hier_params,
               int width,
               int clockRate,
               AdaptiveMHA* _adaptiveMHA,
               int _busCPUCount,
               int _busBankCount)
    : Bus(name, hier_params, width, clockRate, _adaptiveMHA, _busCPUCount, _busBankCount){
    
    fatal("NFQ implementation is not finished");
            
    virtualAddrClock = 0;
    virtualDataClock = 0;
    lastAddrFinishTag.resize(_busCPUCount + _busBankCount, 0);
    lastDataFinishTag.resize(_busCPUCount + _busBankCount, 0);
}

void
NFQBus::arbitrateAddrBus(){
    
    cout << curTick << ": Addr bus arbitration\n";
    
    int grantID = getNFQNextInterface(addrBusRequests, lastAddrFinishTag, true);
    Tick curStartTag = addrBusRequests[grantID].startTag;
    
    DPRINTF(AddrBusVerify, "Arbitrating address bus at tick, %d\n", curTick);

    // grant interface access
    DPRINTF(Bus, "NFQ addr bus granted to id %d\n", grantID);

    addrBusRequests[grantID].requested = false;
    bool do_request = interfaces[grantID]->grantAddr();

    if (do_request) {
        addrBusRequests[grantID].requested = true;
        addrBusRequests[grantID].requestTime = curTick;
        DPRINTF(Bus, "NFQ addr bus re-request from %d\n", grantID);
        
    }

    // schedule next arb event
    if(!blocked){

        int oldestID = -1;
        int secondOldestID = -1;
        bool found = findOldestRequest(addrBusRequests, oldestID, secondOldestID);

        // update virtual clock (reset to zero if there will be an idle period)
        DPRINTF(AddrBusVerify, "Updating virtual clock, %s, %d, %d\n", (found ? "True" : "False"), oldestID, curTick);
        resetVirtualClock(found, addrBusRequests, virtualAddrClock, lastAddrFinishTag, curStartTag, oldestID, true);


        if(found){
            if(!addrArbiterEvent->scheduled()){
                scheduleArbitrationEvent(addrArbiterEvent, addrBusRequests[oldestID].requestTime, nextAddrFree, 2);
            }
        }
    }
    else{
        fatal("Bus blocking not tested with STFQ");
        if (addrArbiterEvent->scheduled()) {
            addrArbiterEvent->deschedule();
        }
    }

    if(do_request){
        // safer to issue this after the vc update
        addrBusRequests[grantID].startTag = virtualAddrClock;
        DPRINTF(AddrBusVerify, "Re-Requesting Address Bus, %d, %d, %d\n", grantID, curTick, virtualAddrClock);
    }
}


void
NFQBus::arbitrateDataBus(){
    assert(curTick>runDataLast);
    runDataLast = curTick;
    assert(doEvents());
    
    cout << curTick << ": Data bus arbitration\n";
   
    cout << "Data bus queue\n";
    for(int i=0;i<dataBusRequests.size();i++){
        cout << i << ": " << (dataBusRequests[i].requested ? "Requested " : "No request" ) << ", " << (dataBusRequests[i].requested ? dataBusRequests[i].requestTime : 0 ) << "\n";
    }
    
    int grantID = getNFQNextInterface(dataBusRequests, lastDataFinishTag, false);
    Tick curStartTag = dataBusRequests[grantID].startTag;

    DPRINTF(Bus, "NFQ data bus granted to id %d\n", grantID);
    assert(grantID >= 0 && grantID < dataBusRequests.size());
    dataBusRequests[grantID].requested = false;
    interfaces[grantID]->grantData();
    
    int oldestID = -1;
    int secondOldestID = -1;
    bool found = findOldestRequest(dataBusRequests, oldestID, secondOldestID);
    resetVirtualClock(found, dataBusRequests, virtualDataClock, lastDataFinishTag, curStartTag, oldestID, false);

    if(found){
        scheduleArbitrationEvent(dataArbiterEvent,dataBusRequests[oldestID].requestTime,nextDataFree);
    }
}

void
NFQBus::scheduleArbitrationEvent(Event * arbiterEvent, Tick reqTime,
                                 Tick nextFreeCycle, Tick idleAdvance)
{
    bool bus_idle = (nextFreeCycle <= curTick);
    Tick next_schedule_time;
    if (bus_idle) {
        if (reqTime < curTick) {
            next_schedule_time = nextBusClock(curTick,idleAdvance);
        } else {
            next_schedule_time = nextBusClock(reqTime,idleAdvance);
        }
    } else {
        if (reqTime < nextFreeCycle) {
            next_schedule_time = nextFreeCycle;
        } else {
            next_schedule_time = nextBusClock(reqTime,idleAdvance);
        }
    }

    if (arbiterEvent->scheduled()) {
        if (arbiterEvent->when() > next_schedule_time) {
            arbiterEvent->reschedule(next_schedule_time);
            DPRINTF(Bus, "Rescheduling arbiter event for cycle %d\n",
                    next_schedule_time);
        }
    } else {
        arbiterEvent->schedule(next_schedule_time);
        DPRINTF(Bus, "scheduling arbiter event for cycle %d\n",
                next_schedule_time);
    }
}




int
NFQBus::getNFQNextInterface(vector<BusRequestRecord> & requests, vector<Tick> & finishTags, bool addr){

    Tick lowestVirtClock = TICK_T_MAX;
    Tick lowestReqTime = TICK_T_MAX;
    int lowestInternalID = -1;
    Tick lowestStartStamp = -1;
    int grantID = -1;

    for(int i=0;i<interfaces.size();i++){
        if(requests[i].requested && requests[i].requestTime < curTick){

            int internalSenderID = interfaces[i]->getCurrentReqSenderID();

            // if the sender is unknown (writeback or null request) the req is on the L2 bank's quota
            if(internalSenderID == BUS_WRITEBACK || internalSenderID == BUS_NO_REQUEST){
                assert(interfaceIndexToMasterIndex.find(i) != interfaceIndexToMasterIndex.end());
                internalSenderID = interfaceIndexToMasterIndex[i] + busCPUCount;
            }

            Tick startStamp = requests[i].startTag > finishTags[internalSenderID] ? requests[i].startTag : finishTags[internalSenderID];

            if(addr) DPRINTF(AddrBusVerify, "Checking for request from interface, %d, %d, %d\n", i, internalSenderID, startStamp);
            
            // policy: first prioritize start tag, then actual request time, then lower interface ID
            bool update = false;
            if(startStamp < lowestVirtClock) update = true;
            else if(startStamp == lowestVirtClock && requests[i].requestTime < lowestReqTime) update = true;
            
            if(update){
                lowestVirtClock = requests[i].startTag;
                lowestReqTime = requests[i].requestTime;
                grantID = i;
                lowestInternalID = internalSenderID;
                lowestStartStamp = startStamp;
            }
        }
    }

    assert(grantID != -1);
    assert(lowestInternalID != -1);
    assert(lowestStartStamp != -1);

    if(addr) DPRINTF(AddrBusVerify, "Granting access to interface, %d, %d, %d, %d\n", grantID, lowestInternalID, lowestStartStamp, curTick);
    
    // update finish time
    finishTags[lowestInternalID] = lowestStartStamp + (clockRate * (busCPUCount + busBankCount));
    
    if(addr) DPRINTF(AddrBusVerify, "Setting new finish tag, %d\n", finishTags[lowestInternalID]);

    return grantID;
}

void
NFQBus::resetVirtualClock(bool found, vector<BusRequestRecord> & requests, Tick & clock, vector<Tick> & tags, Tick startTag, Tick oldest, bool addr){

    if(!found){
        if(addr) DPRINTF(AddrBusVerify, "Resetting clock and start tags (1), %d\n", curTick);
        clock = 0;
        for(int i=0;i<tags.size();i++) tags[i] = 0;
    }
    else if(requests[oldest].requestTime > nextAddrFree){
        if(addr)  DPRINTF(AddrBusVerify, "Resetting clock and start tags (2), %d\n", curTick);
        clock = 0;
        for(int i=0;i<tags.size();i++) tags[i] = 0;
    }
    else{
        clock = startTag;
    }
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(NFQBus)

    Param<int> width;
    Param<int> clock;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
    Param<int> cpu_count;
    Param<int> bank_count;

END_DECLARE_SIM_OBJECT_PARAMS(NFQBus)


BEGIN_INIT_SIM_OBJECT_PARAMS(NFQBus)

    INIT_PARAM(width, "bus width in bytes"),
    INIT_PARAM(clock, "bus frequency"),
    INIT_PARAM_DFLT(hier,
		    "Hierarchy global variables",
		    &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "Adaptive MHA object",NULL),
    INIT_PARAM(cpu_count, "The number of CPUs in the system"),
    INIT_PARAM(bank_count, "The number of L2 cache banks in the system")

END_INIT_SIM_OBJECT_PARAMS(NFQBus)


CREATE_SIM_OBJECT(NFQBus)
{
    return new NFQBus(getInstanceName(), hier,
                      width, clock, adaptive_mha, cpu_count, bank_count);
}

REGISTER_SIM_OBJECT("NFQBus", NFQBus)

#endif //DOXYGEN_SHOULD_SKIP_THIS
