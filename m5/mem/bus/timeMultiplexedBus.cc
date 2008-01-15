
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
    
    cout << "Creating a Time multiplexed bus\n";
    
    curDataNum = 0;
    curAddrNum = 0;
}


void
TimeMultiplexedBus::arbitrateAddrBus(){

    assert(transmitInterfaces.size() - 1 == busBankCount);
    assert(masterInterfaces.size() == busBankCount);
    assert(slaveInterfaces.size() == 1);

    cout << curTick << ": arbitrating at tick " << curTick << "\n";
    
    int grantID = getFairNextInterface(curAddrNum, addrBusRequests);

    if(grantID != -1){

        DPRINTF(Bus, "Fair addr bus granted to id %d\n", grantID);

        assert(addrBusRequests[grantID].requestTime < curTick);
        addrBusRequests[grantID].requested = false;
        bool do_request = interfaces[grantID]->grantAddr();

        if (do_request) {
            addrBusRequests[grantID].requested = true;
            addrBusRequests[grantID].requestTime = curTick;
        }
    }

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
        if (addrArbiterEvent->scheduled()) {
            addrArbiterEvent->deschedule();
        }
    }
}


void
TimeMultiplexedBus::arbitrateDataBus(){
    assert(curTick>runDataLast);
    runDataLast = curTick;
    assert(doEvents());

    int grantID = getFairNextInterface(curDataNum, dataBusRequests);

    if(grantID != -1){
        DPRINTF(Bus, "Fair data bus granted to id %d\n", grantID);
        assert(grantID >= 0 && grantID < dataBusRequests.size());
        assert(dataBusRequests[grantID].requestTime < curTick);
        dataBusRequests[grantID].requested = false;
        interfaces[grantID]->grantData();
    }
    int oldestID = -1;
    int secondOldestID = -1;
    bool found = findOldestRequest(dataBusRequests, oldestID, secondOldestID);

    if(found){
        scheduleArbitrationEvent(dataArbiterEvent,dataBusRequests[oldestID].requestTime,nextDataFree);
    }

}

void
TimeMultiplexedBus::scheduleArbitrationEvent(Event * arbiterEvent, Tick reqTime,
                                             Tick nextFreeCycle, Tick idleAdvance)
{
    
    bool bus_idle = (nextFreeCycle <= curTick);
    Tick next_schedule_time;
    if (bus_idle) {
        if (reqTime < curTick) {
            cout << curTick << ": Shed at next bus clock after curTick\n";
            next_schedule_time = nextBusClock(curTick,idleAdvance);
        } else {
            cout << curTick << ": Shed at next bus clock after request time\n";
            next_schedule_time = nextBusClock(reqTime,idleAdvance);
        }
    } else {
        if (reqTime < nextFreeCycle) {
            next_schedule_time = nextFreeCycle;
            cout << curTick << ": Shed at next free cycle, " << next_schedule_time << "\n";
        } else {
            
            next_schedule_time = nextBusClock(reqTime,idleAdvance);
            cout << curTick << ": Shed at next bus clock, " << next_schedule_time << "\n";
        }
    }

    if (arbiterEvent->scheduled()) {
        if (arbiterEvent->when() > next_schedule_time) {
            arbiterEvent->reschedule(next_schedule_time);
            cout << curTick << ": " << (arbiterEvent == dataArbiterEvent ? "data bus" : "addr bus" ) << " rescheduling for tick " << next_schedule_time << "\n";
        }
        else{
            cout << curTick << ": not updated, quitting\n";
        }
    } else {
        arbiterEvent->schedule(next_schedule_time);
        cout << curTick << ": " << (arbiterEvent == dataArbiterEvent ? "data bus" : "addr bus" ) << " scheduling for tick " << next_schedule_time << "\n";
    }
}


int
TimeMultiplexedBus::getFairNextInterface(int & counter, vector<BusRequestRecord> & requests){

    int retID = -1;

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
           && requests[masterIndexToInterfaceIndex[0]].requestTime < curTick){
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
            DPRINTF(Bus, "Granting writeback slot to interface %d\n", tmpMasterID);
            retID = masterIndexToInterfaceIndex[tmpMasterID];
        }
    }

    counter = (counter + 1) % (busCPUCount + busBankCount);

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
