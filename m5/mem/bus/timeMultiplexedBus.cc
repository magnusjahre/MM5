
#include "mem/bus/timeMultiplexedBus.hh"
#include "sim/builder.hh"

using namespace std;

TimeMultiplexedBus::TimeMultiplexedBus(const std::string &name,
                                       HierParams *hier_params,
                                       int width,
                                       int clockRate,
                                       AdaptiveMHA* _adaptiveMHA,
                                       int _busCPUCount,
                                       int _busBankCount)
    : Bus(name, hier_params, width, clockRate, _adaptiveMHA, _busCPUCount, _busBankCount){
    
    lastAddrArb = 0;
    lastDataArb = 0;
    
    curDataNum = 0;
    curAddrNum = 0;
}


void
TimeMultiplexedBus::arbitrateAddrBus(){

    assert(transmitInterfaces.size() - 1 == busBankCount);
    assert(masterInterfaces.size() == busBankCount);
    assert(slaveInterfaces.size() == 1);

    // update time-multiplex counter
    assert(curTick % clockRate == 0);
    Tick busCyclesSinceLast = ((curTick - lastAddrArb) / clockRate);
    curAddrNum = (curAddrNum + busCyclesSinceLast) % (busCPUCount + busBankCount);
    
    int grantID = getFairNextInterface(curAddrNum, addrBusRequests);


    if(grantID != -1){

        DPRINTF(TimeMultiplexedBus, "Fair addr bus granted to id %d\n", grantID);

        assert(addrBusRequests[grantID].requestTime < curTick);
        addrBusRequests[grantID].requested = false;
        bool do_request = interfaces[grantID]->grantAddr();

        if (do_request) {
            addrBusRequests[grantID].requested = true;
            addrBusRequests[grantID].requestTime = curTick;
        }
    }
    else{
        // advance bus clock due to empty slot
        nextAddrFree = nextBusClock(curTick, NO_REQ_DELAY);
        DPRINTF(TimeMultiplexedBus, "Next addr free is now %d\n", nextAddrFree);
    }

    if(!blocked){

        int oldestID = -1;
        int secondOldestID = -1;
        bool found = findOldestRequest(addrBusRequests, oldestID, secondOldestID);
        
        DPRINTF(TimeMultiplexedBus, "Addr checking next schedule, oldest=%d, nextFree=%d, idleAdvance=%d\n",
                (found ? addrBusRequests[oldestID].requestTime : -1),
                nextAddrFree,
                2);

        if(found){
            if(!addrArbiterEvent->scheduled()){
                scheduleArbitrationEvent(addrArbiterEvent, addrBusRequests[oldestID].requestTime, nextAddrFree, 2);
            }
        }
    }
    else{
        if (addrArbiterEvent->scheduled()) {
            addrArbiterEvent->deschedule();
        }
    }
    
    lastAddrArb = curTick;
}


void
TimeMultiplexedBus::arbitrateDataBus(){
    assert(curTick>runDataLast);
    runDataLast = curTick;
    assert(doEvents());
    
    // update time-multiplex counter
    assert(curTick % clockRate == 0);
    if(lastDataArb != 0){
        assert(lastTransferCycles != -1);
        Tick increment = ((curTick - lastDataArb) / clockRate);
        increment = increment - (lastTransferCycles > 0 ? lastTransferCycles - 1 : 0);
        curDataNum = (curDataNum + increment) % (busCPUCount + busBankCount);
    }
    else{
        assert(curDataNum == 0);
    }
    
    lastTransferCycles = -1;
    
    int grantID = getFairNextInterface(curDataNum, dataBusRequests);

    if(grantID != -1){
        DPRINTF(TimeMultiplexedBus, "Fair data bus granted to id %d\n", grantID);
        assert(grantID >= 0 && grantID < dataBusRequests.size());
        assert(dataBusRequests[grantID].requestTime < curTick);
        dataBusRequests[grantID].requested = false;
        interfaces[grantID]->grantData();
    }
    else{
        lastTransferCycles = 0;
        nextDataFree = nextBusClock(curTick,NO_REQ_DELAY);
        DPRINTF(TimeMultiplexedBus, "Next data free is now %d\n", nextDataFree);
    }
    
    
    int oldestID = -1;
    int secondOldestID = -1;
    bool found = findOldestRequest(dataBusRequests, oldestID, secondOldestID);

    DPRINTF(TimeMultiplexedBus, "Data checking next schedule, oldest=%d, nextFree=%d, idleAdvance=%d\n",
            (found ? dataBusRequests[oldestID].requestTime : -1),
            nextDataFree,
            1);
    
    if(found){
        scheduleArbitrationEvent(dataArbiterEvent,dataBusRequests[oldestID].requestTime,nextDataFree);
    }

    lastDataArb = curTick;
}

int
TimeMultiplexedBus::getFairNextInterface(int & counter, vector<BusRequestRecord> & requests){

    int retID = -1;

    DPRINTF(TimeMultiplexedBus, "Retrieving interface, counter is %d\n", counter);
    
    // grant an interface
    if(counter < busCPUCount){
        // search through all master interfaces
        // select the oldest request belonging to the given processor
        assert(masterInterfaces.size() >= 1);
        int smallestInterfaceID = -1;
        Tick smallestReqTime = TICK_T_MAX;
        for(int i=0;i<masterInterfaces.size();i++){

            int fromCPU = masterInterfaces[i]->getCurrentReqSenderID();

            if(fromCPU == counter){
                if(requests[masterIndexToInterfaceIndex[i]].requested
                   && requests[masterIndexToInterfaceIndex[i]].requestTime < smallestReqTime
                   && requests[masterIndexToInterfaceIndex[i]].requestTime < curTick){
                    smallestInterfaceID = masterIndexToInterfaceIndex[i];
                    smallestReqTime = requests[masterIndexToInterfaceIndex[i]].requestTime;
                   }
            }
        }

        assert(slaveInterfaces.size() == 1);
        if(requests[slaveIndexToInterfaceIndex[0]].requested
           && requests[slaveIndexToInterfaceIndex[0]].requestTime < smallestReqTime
           && requests[slaveIndexToInterfaceIndex[0]].requestTime < curTick){
            
            smallestInterfaceID = slaveIndexToInterfaceIndex[0];
            smallestReqTime = requests[slaveIndexToInterfaceIndex[0]].requestTime;
        }

        if(smallestInterfaceID > -1){
            retID = smallestInterfaceID;
        }
    }
    else{
        // dedicated writeback slot for one cache bank
        int tmpMasterID = counter - busCPUCount;
        int fromCPUID = masterInterfaces[tmpMasterID]->getCurrentReqSenderID();
        if(fromCPUID == BUS_WRITEBACK
           && requests[masterIndexToInterfaceIndex[tmpMasterID]].requested
           && requests[masterIndexToInterfaceIndex[tmpMasterID]].requestTime < curTick){
            DPRINTF(TimeMultiplexedBus, "Granting writeback slot to interface %d\n", tmpMasterID);
            retID = masterIndexToInterfaceIndex[tmpMasterID];
        }
    }

    return retID;
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(TimeMultiplexedBus)

    Param<int> width;
    Param<int> clock;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
    Param<int> cpu_count;
    Param<int> bank_count;

END_DECLARE_SIM_OBJECT_PARAMS(TimeMultiplexedBus)


BEGIN_INIT_SIM_OBJECT_PARAMS(TimeMultiplexedBus)

    INIT_PARAM(width, "bus width in bytes"),
    INIT_PARAM(clock, "bus frequency"),
    INIT_PARAM_DFLT(hier,
		    "Hierarchy global variables",
		    &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "Adaptive MHA object",NULL),
    INIT_PARAM(cpu_count, "The number of CPUs in the system"),
    INIT_PARAM(bank_count, "The number of L2 cache banks in the system")

END_INIT_SIM_OBJECT_PARAMS(TimeMultiplexedBus)


CREATE_SIM_OBJECT(TimeMultiplexedBus)
{
    return new TimeMultiplexedBus(getInstanceName(), hier,
                      width, clock, adaptive_mha, cpu_count, bank_count);
}

REGISTER_SIM_OBJECT("TimeMultiplexedBus", TimeMultiplexedBus)

#endif //DOXYGEN_SHOULD_SKIP_THIS
