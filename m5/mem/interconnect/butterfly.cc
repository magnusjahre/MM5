
#include "sim/builder.hh"
#include "butterfly.hh"
        
#include <math.h>
        
using namespace std;

Butterfly::Butterfly(const std::string &_name, 
                     int _width, 
                     int _clock,
                     int _transDelay,
                     int _arbDelay,
                     int _cpu_count,
                     HierParams *_hier,
                     int _switchDelay,
                     int _radix,
                     int _banks,
                     AdaptiveMHA* _adaptiveMHA)
                : Interconnect(_name,
                               _width,
                               _clock,
                               _transDelay,
                               _arbDelay,
                               _cpu_count,
                               _hier,
                               _adaptiveMHA)
{
    switchDelay = _switchDelay;
    radix = _radix;
    butterflyCpuCount = _cpu_count;
    butterflyCacheBanks = _banks;
            
    if(butterflyCacheBanks != 4){
        fatal("mappings only implemented for 4 L2 cache banks");
    }
            
    if(cpu_count == 2){
        
        cpuIDtoNode[0] = 0;
        cpuIDtoNode[1] = 1;
        
        l2IDtoNode[0] = 2;
        l2IDtoNode[1] = 2;
        l2IDtoNode[2] = 3;
        l2IDtoNode[3] = 3;
                
        terminalNodes = 4;
    }
    else if(cpu_count == 4){
        
        cpuIDtoNode[0] = 0;
        cpuIDtoNode[1] = 1;
        cpuIDtoNode[2] = 2;
        cpuIDtoNode[3] = 3;
        
        l2IDtoNode[0] = 4;
        l2IDtoNode[1] = 5;
        l2IDtoNode[2] = 6;
        l2IDtoNode[3] = 7;
                
        terminalNodes = 8;
    }
    else if(cpu_count == 8){
        
        cpuIDtoNode[0] = 0;
        cpuIDtoNode[1] = 1;
        cpuIDtoNode[2] = 2;
        cpuIDtoNode[3] = 3;
        cpuIDtoNode[4] = 4;
        cpuIDtoNode[5] = 5;
        cpuIDtoNode[6] = 6;
        cpuIDtoNode[7] = 7;
        
        cpuIDtoNode[8] = -1;
        cpuIDtoNode[9] = -1;
        cpuIDtoNode[10] = -1;
        cpuIDtoNode[11] = -1;
        
        l2IDtoNode[0] = 12;
        l2IDtoNode[1] = 13;
        l2IDtoNode[2] = 14;
        l2IDtoNode[3] = 15;
                
        terminalNodes = 16;
    }
    else{
        fatal("The butterfly only supports 2, 4 or 8 processors");
    }
    
    // compute topology utility vars

    // ceil needed because the answer is not completely accurate
    double tmp = (log10((double) terminalNodes) / log10((double) radix));
    stages = (int) ceil(tmp-1e-9);

    switches = stages * (terminalNodes / radix);
    butterflyHeight = (terminalNodes / radix);
    hopCount = stages + 1;
    chanBetweenStages = butterflyHeight * radix;
    int totalChannels = hopCount * chanBetweenStages;
    
    butterflyStatus.insert(butterflyStatus.begin(), totalChannels, false);
    channelUsage.insert(channelUsage.begin(), totalChannels, 0);
    
    if(radix != 2) fatal("Only radix 2 butterflies are implemented");
}



void
Butterfly::send(MemReqPtr& req, Tick time, int fromID){
    
    assert(!blocked);
    assert((req->size / width) <= 1);
    
    int toID = -1;
    if(allInterfaces[fromID]->isMaster() && req->toInterfaceID != -1){
        toID = req->toInterfaceID;
    }
    else if(allInterfaces[fromID]->isMaster()){
        toID = getTarget(req->paddr);
    }
    else{
        toID = req->fromInterfaceID;
    }
    
    grantQueue.push_back(new InterconnectDelivery(time, fromID, toID, req));
    scheduleDeliveryQueueEvent(time + (stages*switchDelay + hopCount*transferDelay));
}

void
Butterfly::arbitrate(Tick cycle){
    
    // reset internal state
    for(int i=0;i<butterflyStatus.size();i++) butterflyStatus[i] = false;
    
    list<InterconnectRequest* > notGrantedReqs;
    Tick legalRequestTime = cycle - arbitrationDelay;
    
    list<InterconnectRequest*>::iterator pos;
    while(!requestQueue.empty()){
        if(requestQueue.front()->time <= legalRequestTime){

            int toInterface = getDestinationId(requestQueue.front()->fromID);
            if(toInterface == -1){
                // null request, remove
                delete requestQueue.front();
                requestQueue.pop_front();
                continue;
            }
            
            // check if destination is blocked
            if(!(allInterfaces[toInterface]->isMaster()) 
                 && blockedInterfaces[interconnectIDToL2IDMap[toInterface]]){
                // the destination cache is blocked, so we can not deliver to it
                notGrantedReqs.push_back(requestQueue.front());
                requestQueue.pop_front();
                continue;
            }
            
            if(setChannelsOccupied(
                    requestQueue.front()->fromID,
                    toInterface)){
                
                // update statistics
                arbitratedRequests++;
                totalArbQueueCycles += 
                    (cycle - requestQueue.front()->time) - arbitrationDelay;
                totalArbitrationCycles += arbitrationDelay;
                
                // grant access
                allInterfaces[requestQueue.front()->fromID]->grantData();
                delete requestQueue.front();
                requestQueue.pop_front();

            }
            else{
                notGrantedReqs.push_back(requestQueue.front());
                requestQueue.pop_front();
            }

        }
        else{
            notGrantedReqs.push_back(requestQueue.front());
            requestQueue.pop_front();
        }
    }
    
    assert(requestQueue.empty());
    if(!notGrantedReqs.empty()){
        // there where requests we could not issue
        // put them back in the queue and schedule new arb event
        requestQueue.splice(requestQueue.begin(), notGrantedReqs);
        
        if(requestQueue.front()->time <= cycle){
            scheduleArbitrationEvent(cycle+1);
        }
        else{
            scheduleArbitrationEvent(
                    requestQueue.front()->time + arbitrationDelay);
        }
    }
    
    assert(butterflyStatus.size() == channelUsage.size());
    for(int i=0;i<butterflyStatus.size();i++){
        if(butterflyStatus[i]) channelUsage[i]++;
    }
}

bool
Butterfly::setChannelsOccupied(int fromInterfaceID, int toInterfaceID){
    
    assert(fromInterfaceID >= 0 && toInterfaceID >=  0);
    
    // translate into butterfly node IDs
    int fromNodeId = (allInterfaces[fromInterfaceID]->isMaster() ?
                      cpuIDtoNode[interconnectIDToProcessorIDMap[fromInterfaceID]]
                    : l2IDtoNode[interconnectIDToL2IDMap[fromInterfaceID]]);
    
    int toNodeId = (allInterfaces[toInterfaceID]->isMaster() ?
                    cpuIDtoNode[interconnectIDToProcessorIDMap[toInterfaceID]]
                  : l2IDtoNode[interconnectIDToL2IDMap[toInterfaceID]]);
    
    // store old state in case we can't grant the request
    vector<bool> tmpState = butterflyStatus;
    
    int atSwitch = -1;
    for(int i=0;i<hopCount;i++){
    
        if(i == 0){
            
            if(butterflyStatus[fromNodeId]){
                butterflyStatus = tmpState;
                return false;
            }
            
            butterflyStatus[fromNodeId] = true;
            
            atSwitch = fromNodeId / radix;
            
        }
        else if(i == hopCount-1){
            int lastStageChanID = (chanBetweenStages * i) + toNodeId;
            if(butterflyStatus[lastStageChanID]){
                butterflyStatus = tmpState;
                return false;
            }
            butterflyStatus[lastStageChanID] = true;
        }
        else{
    
            int useChannelNum = -1;
            if((toNodeId & (1 << (stages - i))) > 0) useChannelNum = 1;
            else useChannelNum = 0;
            int channelID = (atSwitch * 2) + useChannelNum;
            
            int offset = 1 << (stages - i - 1);
            int nextSwitch = -1;
            if((atSwitch & offset) == 0 && useChannelNum == 1){
                nextSwitch = atSwitch + offset;
            }
            else if((atSwitch & offset) > 0 && useChannelNum == 0){
                nextSwitch = atSwitch - offset;
            }
            else{
                nextSwitch = atSwitch;
            }
            
            int stageOffset = chanBetweenStages * i;
            if(butterflyStatus[stageOffset + channelID]){
                butterflyStatus = tmpState;
                return false;
            }
            
            butterflyStatus[stageOffset + channelID] = true;
            
            atSwitch = nextSwitch;
        }
    }
    return true;
}

void
Butterfly::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){
    
    assert(!blocked);
    
    assert(!req);
    assert(toID == -1);
    assert(fromID == -1);
    
    assert(isSorted(&grantQueue));
    
    int butterflyTransDelay = stages*switchDelay + hopCount*transferDelay;
    Tick legalGrantTime = cycle - butterflyTransDelay;
    
    list<InterconnectDelivery* > notDeliveredRequests;

    /* attempt to deliver as many requests as possible */
    while(!grantQueue.empty()){
        InterconnectDelivery* delivery = grantQueue.front();
        grantQueue.pop_front();
        
        /* check if this grant has experienced the proper delay */
        /* since the requests are sorted, we know that all other are later */
        if(delivery->grantTime > legalGrantTime){
            notDeliveredRequests.push_back(delivery);
            continue;
        }
        
        // check if destination is blocked
        if(!(allInterfaces[delivery->toID]->isMaster()) 
             && blockedInterfaces[interconnectIDToL2IDMap[delivery->toID]]){
            // the destination cache is blocked, so we can not deliver to it
            notDeliveredRequests.push_back(delivery);
            continue;
        }
        
        /* update statistics */
        sentRequests++;
        int curCpuId = delivery->req->xc->cpu->params->cpu_id;
        int queueCycles = (cycle - delivery->grantTime) - butterflyTransDelay;
        
        totalTransQueueCycles += queueCycles;
        totalTransferCycles += butterflyTransDelay;
        perCpuTotalTransQueueCycles[curCpuId] += queueCycles;
        perCpuTotalTransferCycles[curCpuId] += butterflyTransDelay;
        
        if(allInterfaces[delivery->toID]->isMaster()){
            allInterfaces[delivery->toID]->deliver(delivery->req);
        }
        else{
            allInterfaces[delivery->toID]->access(delivery->req);
        }
        
        delete delivery;
    }
    
    assert(grantQueue.empty());
    grantQueue.splice(grantQueue.begin(), notDeliveredRequests);
    
    if(!grantQueue.empty()){
        if(grantQueue.front()->grantTime <= legalGrantTime){
            scheduleDeliveryQueueEvent(cycle + 1);
        }
        else{
            scheduleDeliveryQueueEvent(grantQueue.front()->grantTime
                                       + butterflyTransDelay);
        }
    }
}

int
Butterfly::getChannelCount(){
    return hopCount * chanBetweenStages;
}

vector<int>
Butterfly::getChannelSample(){
    vector<int> copy = channelUsage;
    assert(channelUsage.size() == getChannelCount());
    for(int i=0;i<channelUsage.size();i++) channelUsage[i] = 0;
    return copy;
}

void
Butterfly::writeChannelDecriptor(std::ofstream &stream){
    stream << "Interfaces:\n";
    for(int i=0;i<allInterfaces.size();i++){
        stream << "Interface " << i 
               << " (" << allInterfaces[i]->getCacheName() << "): "
               << " mapped to node id " 
               << (allInterfaces[i]->isMaster() ? 
                  cpuIDtoNode[interconnectIDToProcessorIDMap[i]] :
                  l2IDtoNode[interconnectIDToL2IDMap[i]] ) 
               << "\n";
    }
    
    stream << "\nChannels:\n";
    
    int chanSet = 0;
    for(int i=0;i<(hopCount * chanBetweenStages);i++){
        
        if(i != 0 && i % chanBetweenStages == 0){
            chanSet++;
            stream << "\n";
        }
        
        stream << "Channel ID " << i << ": In set " 
                << chanSet << ", id in set " 
                << (i % chanBetweenStages) << "\n";
    }
}

void
Butterfly::printChannelStatus(){
    
    cout << "ID:          ";
    for(int i=0;i<chanBetweenStages;i++){
        if(i<=10) cout << "    " << i << " ";
        else cout << "   " << i << " ";
    }
    cout << "\n";
    
    int chanGroup = 0;
    cout << "Channel Group " << chanGroup << ": ";
    for(int i=0;i<butterflyStatus.size();i++){
        cout << (butterflyStatus[i] ? " true" : "false") << " ";
        if(i != 1 &&  (i+1) % chanBetweenStages == 0){
            chanGroup++;
            cout << "\n";
            if(i != butterflyStatus.size()-1){
                cout << "Channel Group " << chanGroup << ": ";
            }
        }
    }
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Butterfly)
    Param<int> width;
    Param<int> clock;
    Param<int> transferDelay;
    Param<int> arbitrationDelay;
    Param<int> cpu_count;
    SimObjectParam<HierParams *> hier;
    Param<int> switch_delay;
    Param<int> radix;
    Param<int> banks;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
END_DECLARE_SIM_OBJECT_PARAMS(Butterfly)

BEGIN_INIT_SIM_OBJECT_PARAMS(Butterfly)
    INIT_PARAM(width, "the width of the crossbar transmission channels"),
    INIT_PARAM(clock, "butterfly clock"),
    INIT_PARAM(transferDelay, "butterfly transfer delay in CPU cycles"),
    INIT_PARAM(arbitrationDelay, "butterfly arbitration delay in CPU cycles"),
    INIT_PARAM(cpu_count, "the number of CPUs in the system"),
    INIT_PARAM_DFLT(hier,
                    "Hierarchy global variables",
                    &defaultHierParams),
    INIT_PARAM_DFLT(switch_delay,
                    "The delay of a switch in CPU cycles",
                    1),
    INIT_PARAM(radix, "The switching-degree of each switch"),
    INIT_PARAM(banks, "the number of last-level cache banks"),
    INIT_PARAM_DFLT(adaptive_mha, "AdaptiveMHA object", NULL)
END_INIT_SIM_OBJECT_PARAMS(Butterfly)

CREATE_SIM_OBJECT(Butterfly)
{
    return new Butterfly(getInstanceName(),
                         width,
                         clock,
                         transferDelay,
                         arbitrationDelay,
                         cpu_count,
                         hier,
                         switch_delay,
                         radix,
                         banks,
                         adaptive_mha);
}

REGISTER_SIM_OBJECT("Butterfly", Butterfly)

#endif //DOXYGEN_SHOULD_SKIP_THIS

