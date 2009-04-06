
#include "sim/builder.hh"
#include "peer_to_peer_link.hh"

using namespace std;

PeerToPeerLink::PeerToPeerLink(const std::string &_name,
                               int _width,
                               int _clock,
                               int _transDelay,
                               int _arbDelay,
                               int _cpu_count,
                               HierParams *_hier,
                               AdaptiveMHA* _adaptiveMHA,
                               Tick _detailedSimStart)
    : AddressDependentIC(_name,
                         _width,
                         _clock,
                         _transDelay,
                         _arbDelay,
                         _cpu_count,
                         _hier,
                         _adaptiveMHA,
                         NULL)
{
    initQueues(cpu_count, cpu_count);
    queueSize = 32;

    arbEvent = new ADIArbitrationEvent(this);

    slaveInterconnectID = -1;
    attachedCPUID = -1;

    detailedSimStartTick = _detailedSimStart;

    assert(_adaptiveMHA == NULL);
    assert(_transDelay > 0);
}

void
PeerToPeerLink::send(MemReqPtr& req, Tick time, int fromID){

    if(slaveInterconnectID == -1){
        assert(slaveInterfaces.size() == 1);
        for(int i=0;i<allInterfaces.size();i++){
            if(!allInterfaces[i]->isMaster()){
                slaveInterconnectID = i;
                break;
            }
        }
        assert(slaveInterconnectID != -1);
    }

    if(curTick < detailedSimStartTick){

        if(allInterfaces[fromID]->isMaster()){
            req->toInterfaceID = slaveInterconnectID;
            ADIDeliverEvent* delivery = new ADIDeliverEvent(this, req, true);
            delivery->schedule(curTick + transferDelay);
        }
        else{
            ADIDeliverEvent* delivery = new ADIDeliverEvent(this, req, false);
            delivery->schedule(curTick + transferDelay);
        }

        return;
    }

    if(attachedCPUID == -1){
        attachedCPUID = req->adaptiveMHASenderID;
    }
    assert(attachedCPUID == req->adaptiveMHASenderID);

    if(req->finishedInCacheAt < curTick){
        entryDelay += curTick - req->finishedInCacheAt;
    }
    entryRequests++;

    req->inserted_into_crossbar = curTick;

    if(allInterfaces[fromID]->isMaster()){

        p2pRequestQueue.push_back(req);
        if(p2pRequestQueue.size() == queueSize){
            setBlockedLocal(req->adaptiveMHASenderID);
        }
        assert(p2pRequestQueue.size() <= queueSize);
    }
    else{
        p2pResponseQueue.push_back(req);
        assert(p2pResponseQueue.size() <= queueSize);
    }

    if(!arbEvent->scheduled()){

        if(nextLegalArbTime < curTick+1){
            arbEvent->schedule(curTick+1);
        }
        else{
            arbEvent->schedule(nextLegalArbTime);
        }
    }
}

void
PeerToPeerLink::arbitrate(Tick time){

    nextLegalArbTime = curTick + transferDelay;

    assert(slaveInterfaces.size() == 1);

    if(!p2pRequestQueue.empty() && !blockedInterfaces[0]){
        MemReqPtr mreq = p2pRequestQueue.front();
        p2pRequestQueue.pop_front();
        mreq->toInterfaceID = slaveInterconnectID;

        totalArbQueueCycles += curTick - mreq->inserted_into_crossbar;
        arbitratedRequests++;

        ADIDeliverEvent* delivery = new ADIDeliverEvent(this, mreq, true);
        delivery->schedule(curTick + transferDelay);
    }

    if(blockedLocalQueues[attachedCPUID] && p2pRequestQueue.size() < queueSize){
        clearBlockedLocal(attachedCPUID);
    }

    if(!p2pResponseQueue.empty()){
        MemReqPtr sreq = p2pResponseQueue.front();

        totalArbQueueCycles += curTick - sreq->inserted_into_crossbar;
        arbitratedRequests++;

        p2pResponseQueue.pop_front();
        ADIDeliverEvent* delivery = new ADIDeliverEvent(this, sreq, false);
        delivery->schedule(curTick + transferDelay);
    }

    retrieveAdditionalRequests();

    if(isWaitingRequests() && !arbEvent->scheduled()){
        arbEvent->schedule(curTick + transferDelay);
    }
}

void
PeerToPeerLink::retrieveAdditionalRequests(){

    assert(processorIDToInterconnectIDs[attachedCPUID].size() == 2);
    int firstInterfaceID = processorIDToInterconnectIDs[attachedCPUID].front();
    int secondInterfaceID = processorIDToInterconnectIDs[attachedCPUID].back();

    while(notRetrievedRequests[firstInterfaceID] > 0 || notRetrievedRequests[secondInterfaceID] > 0){
        if(p2pRequestQueue.size() < queueSize) findNotDeliveredNextInterface(firstInterfaceID,secondInterfaceID);
        else return;
    }
}

void
PeerToPeerLink::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){

    assert(toID != -1);

    if(allInterfaces[toID]->isMaster()){
        allInterfaces[toID]->deliver(req);
    }
    else{
        assert(slaveInterfaces.size() == 1);
        assert(slaveInterfaces.size() == blockedInterfaces.size());
        if(blockedInterfaces[0] || !deliveryBuffer.empty()){
            deliveryBuffer.push_back(req);
            assert(deliveryBuffer.size() == 1);
        }
        else{
            allInterfaces[toID]->access(req);
        }
    }
}

void
PeerToPeerLink::clearBlocked(int fromInterface){
    Interconnect::clearBlocked(fromInterface);

    while(!deliveryBuffer.empty()){
        MemReqPtr req = deliveryBuffer.front();
        deliveryBuffer.pop_front();
        MemAccessResult res = allInterfaces[slaveInterconnectID]->access(req);
        if(res == BA_BLOCKED) return;
    }

    if(!arbEvent->scheduled()){
        if(nextLegalArbTime < curTick+1){
            arbEvent->schedule(curTick+1);
        }
        else{
            arbEvent->schedule(nextLegalArbTime);
        }
    }
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(PeerToPeerLink)
    Param<int> width;
    Param<int> clock;
    Param<int> transferDelay;
    Param<int> arbitrationDelay;
    Param<int> cpu_count;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
    Param<Tick> detailed_sim_start_tick;
END_DECLARE_SIM_OBJECT_PARAMS(PeerToPeerLink)

BEGIN_INIT_SIM_OBJECT_PARAMS(PeerToPeerLink)
    INIT_PARAM(width, "the width of the crossbar transmission channels"),
    INIT_PARAM(clock, "crossbar clock"),
    INIT_PARAM(transferDelay, "crossbar transfer delay in CPU cycles"),
    INIT_PARAM(arbitrationDelay, "crossbar arbitration delay in CPU cycles"),
    INIT_PARAM(cpu_count, "the number of CPUs in the system"),
    INIT_PARAM_DFLT(hier, "Hierarchy global variables", &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "AdaptiveMHA object", NULL),
    INIT_PARAM(detailed_sim_start_tick, "The tick detailed simulation starts")
END_INIT_SIM_OBJECT_PARAMS(PeerToPeerLink)

CREATE_SIM_OBJECT(PeerToPeerLink)
{
    return new PeerToPeerLink(getInstanceName(),
                              width,
                              clock,
                              transferDelay,
                              arbitrationDelay,
                              cpu_count,
                              hier,
                              adaptive_mha,
                              detailed_sim_start_tick);
}

REGISTER_SIM_OBJECT("PeerToPeerLink", PeerToPeerLink)

#endif //DOXYGEN_SHOULD_SKIP_THIS

