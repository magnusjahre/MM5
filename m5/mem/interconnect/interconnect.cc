
#include "interconnect.hh"
#include "sim/builder.hh"
#include "mem/base_hier.hh"

using namespace std;


void
Interconnect::regStats(){
    
    using namespace Stats;
    
    
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
                                bool isL2,
                                int processorID){

                                    
    ++totalInterfaceCount;
    allInterfaces.push_back(interface);
    assert(totalInterfaceCount == (allInterfaces.size()-1));
    
    if(isL2){
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
    
    if(processorID != -1){
        assert(processorID >= 0);
        processorIDToInterconnectIDMap.insert(
                make_pair(processorID, totalInterfaceCount));
        interconnectIDToProcessorIDMap.insert(
                make_pair(totalInterfaceCount, processorID));
    }
    else{
        assert(isL2);
        interconnectIDToL2IDMap.insert(
                make_pair(totalInterfaceCount, slaveInterfaceCount));
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
    DPRINTF(Blocking, "Blocking the Interconnect\n");
}
        
void
Interconnect::clearBlocked(int fromInterface){
    numClearBlocked++;
    int unblockedL2ID = interconnectIDToL2IDMap[fromInterface];
    assert(blockedInterfaces[unblockedL2ID]);
    blockedInterfaces[unblockedL2ID] = false;
    DPRINTF(Blocking, "Unblocking the Interconnect\n");
}

int
Interconnect::getInterconnectID(int processorID){
    if(processorID == -1) return -1;
    
    map<int,int>::iterator tmp = 
            processorIDToInterconnectIDMap.find(processorID);
    
    // make sure at least one result is returned
    assert(tmp != processorIDToInterconnectIDMap.end());

    return tmp->second;
}

int
Interconnect::getDestinationId(int fromID){
    
    if(allInterfaces[fromID]->isMaster()){
        
        pair<Addr,int> tmp = allInterfaces[fromID]->getTargetAddr();
        Addr targetAddr = tmp.first;
        int toInterfaceId = tmp.second;
        
        // The request was a null request, remove
        if(targetAddr == 0) return -1;
        
        // we allready know the to interface id if it's an L1 to L1 transfer
        if(toInterfaceId != -1) return toInterfaceId;
        
        return getTarget(targetAddr);
    }
    
    int retID = allInterfaces[fromID]->getTargetId();
    assert(retID != -1);
    return retID;
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
        
