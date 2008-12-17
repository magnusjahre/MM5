
#include "interconnect.hh"
#include "sim/builder.hh"
#include "mem/base_hier.hh"

using namespace std;

Interconnect::Interconnect(const std::string &_name, 
                           int _width, 
                           int _clock,
                           int _transDelay,
                           int _arbDelay,
                           int _cpu_count,
                           HierParams *_hier,
                           AdaptiveMHA* _adaptiveMHA) 
    : BaseHier(_name, _hier){

    width = _width;
    clock = _clock;
    transferDelay = _transDelay;
    arbitrationDelay = _arbDelay;
    cpu_count = _cpu_count;
    
    processorIDToInterconnectIDs.resize(cpu_count, list<int>());
    
    if(_adaptiveMHA != NULL){
        adaptiveMHA = _adaptiveMHA;
        adaptiveMHA->registerInterconnect(this);
    }
    else adaptiveMHA = NULL;
    

    if(clock != 1){
        fatal("The interconnects are only implemented to run "
                "at the same frequency as the CPU core");
    }
    
    if(cpu_count < 1){
        fatal("There must be at least one CPU in the system");
    }
    
    masterInterfaceCount = -1;
    slaveInterfaceCount = -1;
    totalInterfaceCount = -1;
    
    blocked = false;
    blockedAt = -1;
    waitingFor = -1;
    
}

void
Interconnect::regStats(){
    
    using namespace Stats;
    
    
    entryDelay
        .name(name() + ".total_entry_delay")
        .desc("total number of cycles before a request was granted access to the crossbar")
        ;
    
    entryRequests
        .name(name() + ".total_entry_requests")
        .desc("total number of requests in entry delay measurements")
        ;
    
    avgDelayBeforeEntry
            .name(name() + ".avg_delay_before_entry")
            .desc("average number of wait cycles before entry to crossbar per requests")
            ;
    
    entryReadDelay
        .name(name() + ".total_read_entry_delay")
        .desc("total number of read cycles before a request was granted access to the crossbar")
        ;
    
    entryReadRequests
        .name(name() + ".total_read_entry_requests")
        .desc("total number of read requests in entry delay measurements")
        ;
    
    avgReadDelayBeforeEntry
        .name(name() + ".avg_read_delay_before_entry")
        .desc("average number of wait cycles before entry to crossbar per read requests")
        ;
    
    avgDelayBeforeEntry = entryDelay / entryRequests;
    avgReadDelayBeforeEntry = entryReadDelay / entryRequests;
    
    deliverBufferDelay
        .name(name() + ".deliver_buffer_delay")
        .desc("total number of cycles spent in the delivery buffer")
        ;
            
    deliverBufferRequests
        .name(name() + ".deliver_buffer_requests")
        .desc("total number of delivered requests")
        ;
            
    avgDeliverBufferDelay
            .name(name() + ".avg_deliver_buffer_delay")
            .desc("average delivery buffer delay per request")
            ;
    
    avgDeliverBufferDelay = deliverBufferDelay / deliverBufferRequests;
    
    /* Arbitration */
    totalArbitrationCycles
        .name(name() + ".total_arbitration_cycles")
        .desc("total number of arbitration cycles for all requests")
        ;
    
    avgArbCyclesPerRequest
        .name(name() + ".avg_arbitration_cycles_per_req")
        .desc("average number of arbitration cycles per requests")
        ;
    avgArbCyclesPerRequest = totalArbitrationCycles / arbitratedRequests;
        
    totalArbQueueCycles
        .name(name() + ".total_arbitration_queue_cycles")
        .desc("total number of cycles in the arbitration queue "
              "for all requests")
        ;
    
    avgArbQueueCyclesPerRequest
        .name(name() + ".avg_arbitration_queue_cycles_per_req")
        .desc("average number of arbitration queue cycles per requests")
        ;
    avgArbQueueCyclesPerRequest = totalArbQueueCycles / arbitratedRequests;
    
    
    /* Transfer */
    totalTransferCycles
        .name(name() + ".total_transfer_cycles")
        .desc("total number of transfer cycles for all requests")
        ;
    
    avgTransCyclesPerRequest
        .name(name() + ".avg_transfer_cycles_per_request")
        .desc("average number of transfer cycles per requests")
        ;
    
    avgTransCyclesPerRequest = totalTransferCycles / sentRequests;
    
    totalTransQueueCycles
        .name(name() + ".total_transfer_queue_cycles")
        .desc("total number of transfer queue cycles for all requests")
        ;
    
    avgTransQueueCyclesPerRequest
        .name(name() + ".avg_transfer_queue_cycles_per_request")
        .desc("average number of transfer queue cycles per request")
        ;
    
    avgTransQueueCyclesPerRequest = totalTransQueueCycles / sentRequests;

    perCpuTotalTransferCycles
        .init(cpu_count)
        .name(name() + ".per_cpu_total_transfer_cycles")
        .desc("total number of transfer cycles per cpu for all requests")
        .flags(total)
        ;
    
    perCpuTotalTransQueueCycles
        .init(cpu_count)
        .name(name() + ".per_cpu_total_transfer_queue_cycles")
        .desc("total number of cycles in the transfer queue per cpu "
              "for all requests")
        .flags(total)
        ;
    
    cpuEntryInterferenceCycles
        .init(cpu_count)
        .name(name() + ".cpu_entry_interference_cycles")
        .desc("total number of cycles the requests of a CPU was delayed before crossbar entry")
        .flags(total)
        ;
    
    cpuTransferInterferenceCycles
        .init(cpu_count)
        .name(name() + ".cpu_transfer_interference_cycles")
        .desc("total number of interferece cycles the requests of a CPU due to crossbar queue")
        .flags(total)
        ;
    
    cpuDeliveryInterferenceCycles
        .init(cpu_count)
        .name(name() + ".cpu_delivery_interference_cycles")
        .desc("total number of interferece cycles in the delivery buffer")
        .flags(total)
        ;
    
    cpuInterferenceCycles
        .name(name() + ".cpu_interference_cycles")
        .desc("total number of cycles the requests of a CPU was delayed")
        .flags(total)
        ;

    cpuInterferenceCycles = cpuEntryInterferenceCycles + cpuTransferInterferenceCycles + cpuDeliveryInterferenceCycles;
    
    perCpuTotalDelay
        .init(cpu_count)
        .name(name() + ".cpu_total_delay_cycles")
        .desc("total number of latency cycles")
        .flags(total)
        ;

    perCpuRequests
        .init(cpu_count)
        .name(name() + ".cpu_total_delay_requests")
        .desc("number of requests i the total delay measurements")
        .flags(total)
        ;

    perCpuAvgDelay
        .name(name() + ".cpu_avg_delay")
        .desc("average delay per request for eachcpu")
        .flags(total)
        ;

    perCpuAvgDelay = perCpuTotalDelay / perCpuRequests;
    
    /* Other statistics */
    avgTotalDelayCyclesPerRequest
        .name(name() + ".avg_total_delay_cycles_per_request")
        .desc("average number of delay cycles per request")
        ;
    
    avgTotalDelayCyclesPerRequest =   avgArbCyclesPerRequest 
                                    + avgArbQueueCyclesPerRequest
                                    + avgTransCyclesPerRequest
                                    + avgTransQueueCyclesPerRequest;

    requests
            .name(name() + ".requests")
            .desc("total number of requests")
            ;
    
    masterRequests
            .name(name() + ".master_requests")
            .desc("total number of requests from a master")
            ;
    
    arbitratedRequests
            .name(name() + ".arbitrated_requests")
            .desc("total number of requests that reached arbitration")
            ;
    
    sentRequests
            .name(name() + ".sent_requests")
            .desc("total number of requests that are actually sent")
            ;
    
    nullRequests
            .name(name() + ".null_requests")
            .desc("total number of null requests")
            ;
    
    nullRequests
            .name(name() + ".null_requests")
            .desc("total number of null requests")
            ;
    
//     duplicateRequests
//             .name(name() + ".duplicate_requests")
//             .desc("total number of duplicate requests")
//             ;
    
    numSetBlocked
            .name(name() + ".num_set_blocked")
            .desc("the number of times the interconnect has been blocked")
            ;
    
    numClearBlocked
            .name(name() + ".num_clear_blocked")
            .desc("the number of times the interconnect has been cleared")
            ;

}

void
Interconnect::resetStats(){
    /* seems like function is not needed as measurements only are taken in the 
     * second phase when using fast forwarding. Consequently, it is not 
     * implemented.
     */
}

int
Interconnect::registerInterface(InterconnectInterface* interface,
                                bool isSlave,
                                int processorID){

                                    
    ++totalInterfaceCount;
    allInterfaces.push_back(interface);
    assert(totalInterfaceCount == (allInterfaces.size()-1));
    
    if(isSlave){
        // This is a slave interface (i.e. interface to a L2 bank)
        ++slaveInterfaceCount;
        slaveInterfaces.push_back(interface);
        assert(slaveInterfaceCount == (slaveInterfaces.size()-1));
    }
    else{
        // This is a master interface (i.e. interface to a L1 cache)
        ++masterInterfaceCount;
        masterInterfaces.push_back(interface);
        assert(masterInterfaceCount == (masterInterfaces.size()-1));
    }
    
    if(!isSlave){
        assert(processorID >= 0);
        processorIDToInterconnectIDs[processorID].push_back(totalInterfaceCount);
        interconnectIDToProcessorIDMap.insert(make_pair(totalInterfaceCount, processorID));
    }
    else{
        interconnectIDToL2IDMap.insert(make_pair(totalInterfaceCount, slaveInterfaceCount));
        L2IDMapToInterconnectID.insert(make_pair(slaveInterfaceCount, totalInterfaceCount));
        blockedInterfaces.push_back(false);
    }
    assert(blockedInterfaces.size() == slaveInterfaces.size());
    
    return totalInterfaceCount;
}

void
Interconnect::rangeChange(){
    for(int i=0;i<allInterfaces.size();++i){
        list<Range<Addr> > range_list;
        allInterfaces[i]->getRange(range_list);
    }
}

void
Interconnect::incNullRequests(){
    nullRequests++;
}

void
Interconnect::request(Tick time, int fromID){
    
    if(allInterfaces[fromID]->isMaster()) masterRequests++;
    requests++;
    
    if(requestQueue.empty() || requestQueue.back()->time < time){
        requestQueue.push_back(new InterconnectRequest(time, fromID));
    }
    else{
        list<InterconnectRequest*>::iterator pos;
        for(pos = requestQueue.begin();
            pos != requestQueue.end();
            pos++){
                if((*pos)->time > time) break;
        }
        requestQueue.insert(pos, new InterconnectRequest(time, fromID));
    }
    
    assert(isSorted(&requestQueue));

    scheduleArbitrationEvent(time + arbitrationDelay);
}

void
Interconnect::scheduleArbitrationEvent(Tick candidateTime){
    
    int found = false;
    for(int i=0;i<arbitrationEvents.size();i++){
        if(arbitrationEvents[i]->when() == candidateTime) found = true;
    }
    
    if(!found){
        InterconnectArbitrationEvent* event = 
                new InterconnectArbitrationEvent(this);
        event->schedule(candidateTime);
        arbitrationEvents.push_back(event);
    }
}

void
Interconnect::scheduleDeliveryQueueEvent(Tick candidateTime){
    
    /* check if we need to schedule a deliver event */
    bool found = false;
    for(int i=0;i<deliverEvents.size();i++){
        if(deliverEvents[i]->when() == candidateTime) found = true;
    }
    
    if(!found){
        InterconnectDeliverQueueEvent* event = 
                new InterconnectDeliverQueueEvent(this);
        event->schedule(candidateTime);
        deliverEvents.push_back(event);
    }
}

void
Interconnect::setBlocked(int fromInterface){
    numSetBlocked++;
    int blockedL2ID = interconnectIDToL2IDMap[fromInterface];
    assert(!blockedInterfaces[blockedL2ID]);
    blockedInterfaces[blockedL2ID] = true;
    DPRINTF(Blocking, "Blocking the interconnect for slave %d\n", blockedL2ID);
}
        
void
Interconnect::clearBlocked(int fromInterface){
    numClearBlocked++;
    int unblockedL2ID = interconnectIDToL2IDMap[fromInterface];
    assert(blockedInterfaces[unblockedL2ID]);
    blockedInterfaces[unblockedL2ID] = false;
    DPRINTF(Blocking, "Unblocking the interconnect for slave %d\n", unblockedL2ID);
}

int
Interconnect::getInterconnectID(int processorID){
    if(processorID == -1) return -1;
    
    fatal("conversion from proc ID to interconnect id is broken");
//     map<int,int>::iterator tmp = processorIDToInterconnectIDMap.find(processorID);
    
    // make sure at least one result is returned
//     assert(tmp != processorIDToInterconnectIDMap.end());

//     return tmp->second;
    return -1;
}

int
Interconnect::getDestinationId(int fromID){
    fatal("ni");
    return 0;
}

Addr
Interconnect::getDestinationAddr(int fromID){
    fatal("ni");
    return 0;
}

MemCmd
Interconnect::getCurrentCommand(int fromID){
    fatal("ni");
    return InvalidCmd;
}

void
Interconnect::getSendSample(int* dataSends,
                            int* instSends,
                            int* coherenceSends,
                            int* totalSends){
    
    assert(*dataSends == 0);
    assert(*instSends == 0);
    assert(*coherenceSends == 0);
    assert(*totalSends == 0);
    
    int tmpSends, tmpInsts, tmpCoh, tmpTotal;
    
    for(int i=0;i<allInterfaces.size();i++){
        allInterfaces[i]->getSendSample(&tmpSends,
                                        &tmpInsts,
                                        &tmpCoh,
                                        &tmpTotal);
        *dataSends += tmpSends;
        *instSends += tmpInsts;
        *coherenceSends += tmpCoh;
        *totalSends += tmpTotal;
    }
}

int
Interconnect::getTarget(Addr address){
    int toID = -1;
    int hitCount = 0;
    
    for(int i=0;i<allInterfaces.size();i++){
        
        if(allInterfaces[i]->isMaster()) continue;
        
        if(allInterfaces[i]->inRange(address)){
            toID = i;
            hitCount++;
        }
    }
    if(hitCount == 0) fatal("No supplier for address in interconnect");
    if(hitCount > 1) fatal("More than one supplier for address in interconnect");
    
    return toID;
}

bool
Interconnect::isSorted(list<InterconnectDelivery*>* inList){
    InterconnectDelivery* prev = NULL;
    bool first = true;
    bool nonSeqDataExists = false;
    for(list<InterconnectDelivery*>::iterator i=inList->begin();
        i!=inList->end();
        i++){
            if(first){
                first = false;
                prev = *i;
                continue;
            }
            
            if(prev->grantTime > (*i)->grantTime) nonSeqDataExists = true;
            prev = *i;
    }
    return !nonSeqDataExists;
}

bool
Interconnect::isSorted(list<InterconnectRequest*>* inList){
    InterconnectRequest* prev = NULL;
    bool first = true;
    bool nonSeqDataExists = false;
    for(list<InterconnectRequest*>::iterator i=inList->begin();
        i!=inList->end();
        i++){
            if(first){
                first = false;
                prev = *i;
                continue;
            }
            
            if(prev->time > (*i)->time) nonSeqDataExists = true;
            prev = *i;
    }
    return !nonSeqDataExists;
}   

void
InterconnectArbitrationEvent::process(){
    
    int foundIndex = -1;
    int eventHitCount = 0;
    for(int i=0;i<interconnect->arbitrationEvents.size();++i){
        if(interconnect->arbitrationEvents[i] == this){
            foundIndex = i;
            eventHitCount++;
        }
    }
    assert(foundIndex >= 0);
    assert(eventHitCount == 1);
    interconnect->arbitrationEvents.erase(
            interconnect->arbitrationEvents.begin()+foundIndex);
    
    interconnect->arbitrate(this->when());
    delete this;
}

const char*
InterconnectArbitrationEvent::description(){
    return "Interconnect arbitration event";
}

void
InterconnectDeliverEvent::process(){
    interconnect->deliver(this->req, this->when(), this->toID, this->fromID);
    delete this;
}

const char*
InterconnectDeliverEvent::description(){
    return "Interconnect deliver event";
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("Interconnect", Interconnect);

#endif
        
