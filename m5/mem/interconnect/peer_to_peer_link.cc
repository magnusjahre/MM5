
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
    initQueues(2, 3);
    p2pRequestQueues.resize(2, list<P2PRequestEntry>());
    queueSize = 32;
    
    arbEvent = new ADIArbitrationEvent(this);
    
    assert(_adaptiveMHA != NULL);
    assert(_transDelay > 0);
}

void
PeerToPeerLink::send(MemReqPtr& req, Tick time, int fromID){
    
    cout << curTick << " " << name() << ": sending from interface " << fromID << ", proc " << req->adaptiveMHASenderID << "\n";
    
    if(allInterfaces[fromID]->isMaster()){
        p2pRequestQueues[fromID].push_back(P2PRequestEntry(req,curTick));
        if(requestQueue.size() >= queueSize) fatal("P2P blocking not implemented");
    }
    else{
        p2pResponseQueue.push_back(req);
    }
    
    if(!arbEvent->scheduled()){
        arbEvent->schedule(curTick);
    }
}

void 
PeerToPeerLink::arbitrate(Tick time){
    
    assert(slaveInterfaces.size() == 1);
    assert(p2pRequestQueues.size() == 2);
    
    int sendFromQueue = -1;
    if(!p2pRequestQueues[0].empty() && !p2pRequestQueues[1].empty()){
        if(p2pRequestQueues[0].front().entry <= p2pRequestQueues[1].front().entry){
            sendFromQueue = 0;
        }
        else{
            sendFromQueue = 1;
        }
    }
    else if(!p2pRequestQueues[0].empty() && p2pRequestQueues[1].empty()){
        sendFromQueue = 0;
    }
    else if(p2pRequestQueues[0].empty() && !p2pRequestQueues[1].empty()){
        sendFromQueue = 1;
    }
    
    if(sendFromQueue != -1){
        
        int toID = -1;
        assert(slaveInterfaces.size() == 1);
        for(int i=0;i<allInterfaces.size();i++){
            if(!allInterfaces[i]->isMaster()){
                toID = i;
                break;
            }
        }
        assert(toID != -1);
        
        P2PRequestEntry entry = p2pRequestQueues[sendFromQueue].front();
        p2pRequestQueues[sendFromQueue].pop_front();
        entry.req->toInterfaceID = toID;
        ADIDeliverEvent* delivery = new ADIDeliverEvent(this, entry.req, true);
        delivery->schedule(curTick + transferDelay);
    }
    
    if(!p2pResponseQueue.empty()){
        MemReqPtr sreq = p2pResponseQueue.front();
        p2pResponseQueue.pop_front();
        ADIDeliverEvent* delivery = new ADIDeliverEvent(this, sreq, false);
        delivery->schedule(curTick + transferDelay);
    }
    
    if(!p2pRequestQueues[0].empty() || !p2pRequestQueues[1].empty() || !p2pResponseQueue.empty()){
        arbEvent->schedule(curTick + 1);
    }
}

void 
PeerToPeerLink::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){
    
    if(allInterfaces[toID]->isMaster()){
        allInterfaces[toID]->deliver(req);
    }
    else{
        cout << curTick << " " << name() << ": delivering to interface " << toID << "\n";
        MemAccessResult res = allInterfaces[toID]->access(req);
        if(res == BA_BLOCKED){
            fatal("P2P slave blocking not handled");
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
END_DECLARE_SIM_OBJECT_PARAMS(PeerToPeerLink)

BEGIN_INIT_SIM_OBJECT_PARAMS(PeerToPeerLink)
    INIT_PARAM(width, "the width of the crossbar transmission channels"),
    INIT_PARAM(clock, "crossbar clock"),
    INIT_PARAM(transferDelay, "crossbar transfer delay in CPU cycles"),
    INIT_PARAM(arbitrationDelay, "crossbar arbitration delay in CPU cycles"),
    INIT_PARAM(cpu_count, "the number of CPUs in the system"),
    INIT_PARAM_DFLT(hier, "Hierarchy global variables", &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "AdaptiveMHA object", NULL)
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
                              adaptive_mha);
}

REGISTER_SIM_OBJECT("PeerToPeerLink", PeerToPeerLink)

#endif //DOXYGEN_SHOULD_SKIP_THIS