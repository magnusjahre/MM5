
#include "address_dependent_ic.hh"

using namespace std;

AddressDependentIC::AddressDependentIC(const std::string &_name, 
                                       int _width, 
                                       int _clock,
                                       int _transDelay,
                                       int _arbDelay,
                                       int _cpu_count,
                                       HierParams *_hier,
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
    

}

void
AddressDependentIC::initQueues(int localBlockedSize, int expectedInterfaces){
    blockedLocalQueues = vector<bool>(localBlockedSize, false);
    notRetrievedRequests = vector<int>(expectedInterfaces, 0);
}

void
AddressDependentIC::request(Tick cycle, int fromID){
    requests++;
    ADIRetrieveReqEvent* event = new ADIRetrieveReqEvent(this, fromID);
    event->schedule(cycle);
}

void 
AddressDependentIC::retriveRequest(int fromInterface){
    
    DPRINTF(Crossbar, "Request recieved from interface %d, cpu %d\n", fromInterface, interconnectIDToProcessorIDMap[fromInterface]);
    
    if(!allInterfaces[fromInterface]->isMaster()){
        allInterfaces[fromInterface]->grantData();
        return;
    }
    
    if(!blockedLocalQueues[interconnectIDToProcessorIDMap[fromInterface]]){
        allInterfaces[fromInterface]->grantData();
    }
    else{
        notRetrievedRequests[fromInterface]++;
    }
}

void 
AddressDependentIC::setBlockedLocal(int fromCPUId){
    DPRINTF(Blocking, "Blocking the Interconnect due to full local queue for CPU %d\n", fromCPUId);
    assert(!blockedLocalQueues[fromCPUId]);
    blockedLocalQueues[fromCPUId] = true;
}

void 
AddressDependentIC::clearBlockedLocal(int fromCPUId){
    DPRINTF(Blocking, "Unblocking the Interconnect, local queue space available for CPU%d\n", fromCPUId);
    assert(blockedLocalQueues[fromCPUId]);
    blockedLocalQueues[fromCPUId] = false;
}

int 
AddressDependentIC::findNotDeliveredNextInterface(int firstInterfaceID, int secondInterfaceID){
    
    MemReqPtr firstReq = allInterfaces[firstInterfaceID]->getPendingRequest();
    MemReqPtr secondReq = allInterfaces[secondInterfaceID]->getPendingRequest();
    int grantedID = -1;
                    
    if(notRetrievedRequests[firstInterfaceID] > 0 && notRetrievedRequests[secondInterfaceID] == 0){
        allInterfaces[firstInterfaceID]->grantData();
        grantedID = firstInterfaceID;
    }
    else if(notRetrievedRequests[firstInterfaceID] == 0 && notRetrievedRequests[secondInterfaceID] > 0){
        allInterfaces[secondInterfaceID]->grantData();
        grantedID = secondInterfaceID;
    }
    else{
        assert(notRetrievedRequests[firstInterfaceID] > 0);
        assert(notRetrievedRequests[secondInterfaceID] > 0);
                        
        Tick firstTime = firstReq ? firstReq->finishedInCacheAt : 0;
        Tick secondTime = secondReq ? secondReq->finishedInCacheAt : 0;
                        
        if(firstTime <= secondTime){
            allInterfaces[firstInterfaceID]->grantData();
            grantedID = firstInterfaceID;
        }
        else{
            allInterfaces[secondInterfaceID]->grantData();
            grantedID = secondInterfaceID;
        }
    }
    
    notRetrievedRequests[grantedID]--;
    return grantedID;
}