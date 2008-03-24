
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
    
    lastAddrFinishTag.resize(_busCPUCount + _busBankCount, 0);
    lastDataFinishTag.resize(_busCPUCount + _busBankCount, 0);
}

void
NFQBus::arbitrateAddrBus(){
    
    int grantID = getNFQNextInterface(addrBusRequests, lastAddrFinishTag, true);

    // grant interface access
    DPRINTF(NFQBus, "NFQ addr bus granted to id %d\n", grantID);

    assert(addrBusRequests[grantID].requestTime < curTick);
    addrBusRequests[grantID].requested = false;
    bool do_request = interfaces[grantID]->grantAddr();

    if (do_request) {
        addrBusRequests[grantID].requested = true;
        addrBusRequests[grantID].requestTime = curTick;
        DPRINTF(NFQBus, "NFQ addr bus re-request from %d\n", grantID);
    }

    // schedule next arb event
    if(!blocked){

        int oldestID = -1;
        int secondOldestID = -1;
        bool found = findOldestRequest(addrBusRequests, oldestID, secondOldestID);

        if(found){
            if(!addrArbiterEvent->scheduled()){
                scheduleArbitrationEvent(addrArbiterEvent, addrBusRequests[oldestID].requestTime, nextAddrFree, 2);
            }
        }
    }
    else{
        fatal("Bus blocking not tested with NFQ");
        if (addrArbiterEvent->scheduled()) {
            addrArbiterEvent->deschedule();
        }
    }
}


// void
// NFQBus::arbitrateDataBus(){
//     assert(curTick>runDataLast);
//     runDataLast = curTick;
//     assert(doEvents());
//     
//     lastTransferCycles = -1;
//     
//     int grantID = getNFQNextInterface(dataBusRequests, lastDataFinishTag, false);
// 
//     DPRINTF(NFQBus, "NFQ data bus granted to id %d\n", grantID);
//     assert(grantID >= 0 && grantID < dataBusRequests.size());
//     assert(dataBusRequests[grantID].requestTime < curTick);
//     dataBusRequests[grantID].requested = false;
//     interfaces[grantID]->grantData();
//     
//     int oldestID = -1;
//     int secondOldestID = -1;
//     bool found = findOldestRequest(dataBusRequests, oldestID, secondOldestID);
// 
//     if(found){
//         scheduleArbitrationEvent(dataArbiterEvent,dataBusRequests[oldestID].requestTime,nextDataFree);
//     }
// }

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

            Tick startStamp = requests[i].requestTime > finishTags[internalSenderID] ?
                    requests[i].requestTime : finishTags[internalSenderID];

            if(addr) DPRINTF(NFQBus, "Checking for request from interface, %d, %d, %d\n", i, internalSenderID, startStamp);
            
            // policy: first prioritize start tag, then actual request time, then lower interface ID
            bool update = false;
            if(startStamp < lowestVirtClock 
               && requests[i].requestTime < curTick) update = true;
            else if(startStamp == lowestVirtClock 
                    && requests[i].requestTime < lowestReqTime 
                    && requests[i].requestTime < curTick) update = true;
            
            if(update){
                lowestVirtClock = startStamp;
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

    if(addr) DPRINTF(NFQBus, "Granting access to interface, %d, %d, %d, %d\n", grantID, lowestInternalID, lowestStartStamp, curTick);
    
    // update finish time
    finishTags[lowestInternalID] = lowestStartStamp + (clockRate * (busCPUCount + busBankCount));
    
    if(addr) DPRINTF(NFQBus, "Setting new finish tag, %d\n", finishTags[lowestInternalID]);

    return grantID;
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

#endif //DOXYGEN_SHOULD_SKIP_THIS*/
