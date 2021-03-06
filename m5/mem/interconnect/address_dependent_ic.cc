
#include "address_dependent_ic.hh"

using namespace std;

AddressDependentIC::AddressDependentIC(const std::string &_name,
                                       int _width,
                                       int _clock,
                                       int _transDelay,
                                       int _arbDelay,
                                       int _cpu_count,
                                       HierParams *_hier,
                                       AdaptiveMHA* _adaptiveMHA,
                                       InterferenceManager* _interferenceManager)
    : Interconnect(_name,
                   _width,
                   _clock,
                   _transDelay,
                   _arbDelay,
                   _cpu_count,
                   _hier,
                   _adaptiveMHA)
{

	interferenceManager = _interferenceManager;

}

void
AddressDependentIC::initQueues(int localBlockedSize, int expectedInterfaces){
    blockedLocalQueues.resize(localBlockedSize, false);
    notRetrievedRequests.resize(expectedInterfaces, 0);
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

void
AddressDependentIC::retrieveAdditionalRequests(){

    bool allZero = true;
    for(int i=0;i<allInterfaces.size();i++){
        if(!allInterfaces[i]->isMaster()){
            assert(notRetrievedRequests[i] == 0);
        }
        if(notRetrievedRequests[i] > 0) allZero = false;
    }

    if(!allZero){
        for(int i=0;i<cpu_count;i++){

            if(processorIDToInterconnectIDs[i].empty()) continue;

            if(processorIDToInterconnectIDs[i].size() == 2){
                int firstInterfaceID = processorIDToInterconnectIDs[i].front();
                int secondInterfaceID = processorIDToInterconnectIDs[i].back();

                while((notRetrievedRequests[firstInterfaceID] > 0 || notRetrievedRequests[secondInterfaceID] > 0)
                    && !blockedLocalQueues[i]){

                    int grantedID = findNotDeliveredNextInterface(firstInterfaceID, secondInterfaceID);

                    DPRINTF(Crossbar, "Accepting new request from interface %d, proc %d, first waiting %d, second waiting %d\n", grantedID, i, notRetrievedRequests[firstInterfaceID], notRetrievedRequests[secondInterfaceID]);
                }
            }
            else{
                assert(processorIDToInterconnectIDs[i].size() == 1);
                int iID = processorIDToInterconnectIDs[i].front();
                while(notRetrievedRequests[iID] > 0 && !blockedLocalQueues[i]){
                    allInterfaces[iID]->grantData();
                    notRetrievedRequests[iID]--;

                    DPRINTF(Crossbar, "Accepting new request from interface %d, proc %d, still waiting %d\n", iID, i, notRetrievedRequests[iID]);
                }
            }
        }
    }
}

void
AddressDependentIC::updateEntryInterference(MemReqPtr& req, int fromID){

    // FIXME: what about entries from the slave side?
    if(allInterfaces[fromID]->isMaster()){
        assert(curTick >= req->finishedInCacheAt);
        int waitTime = curTick - req->finishedInCacheAt;
        entryDelay += waitTime;

        req->latencyBreakdown[INTERCONNECT_ENTRY_LAT] += waitTime;

        //TODO: might need to add a more sophisticated measurement scheme
        // assumes that all entry latency is interference
        req->interferenceBreakdown[INTERCONNECT_ENTRY_LAT] += waitTime;

        if(req->cmd == Read){
        	interferenceManager->addInterference(InterferenceManager::InterconnectEntry, req, waitTime);
        	interferenceManager->addLatency(InterferenceManager::InterconnectEntry, req, waitTime);
        }
    }
    entryRequests++;
    if(req->cmd == Read) entryReadRequests++;
}

void
AddressDependentIC::createFixedLatencyResponse(int latency, int fromID, MemReqPtr& req){

	assert(req->cmd == Read || req->cmd == Writeback);
	if(req->cmd == Read){
		if(allInterfaces[fromID]->isMaster()){
			req->toInterfaceID = fromID;
		}

		ADIDeliverEvent* delivery = new ADIDeliverEvent(this, req, allInterfaces[fromID]->isMaster());
		delivery->schedule(curTick + latency);
	}
}


