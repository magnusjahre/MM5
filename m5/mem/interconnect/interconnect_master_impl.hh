
#include <iostream>
#include <vector>


#include "interconnect.hh"
#include "interconnect_master.hh"


using namespace std;

template<class MemType>
InterconnectMaster<MemType>::InterconnectMaster(const string &name, 
                                                Interconnect* interconnect,
                                                MemType* cache,
                                                HierParams *hier)
    : InterconnectInterface(interconnect, name, hier)
{
    thisCache = cache;

    interfaceID = thisInterconnect->registerInterface(this,
                                                      false,
                                                      cache->getProcessorID());
    thisCache->setInterfaceID(interfaceID);
}

template<class MemType>
MemAccessResult
InterconnectMaster<MemType>::access(MemReqPtr &req){

    // NOTE: copied from MasterInterface
    int satisfied_before = req->flags & SATISFIED;
    // Cache Coherence call goes here
    //mem->snoop(req);
    if (satisfied_before != (req->flags & SATISFIED)) {
        return BA_SUCCESS;
    }
    
    return BA_NO_RESULT;
}

template<class MemType>
void
InterconnectMaster<MemType>::deliver(MemReqPtr &req){

    thisCache->handleResponse(req);
}

template<class MemType>
void
InterconnectMaster<MemType>::request(Tick time){
    
    thisInterconnect->request(time, interfaceID);
}

template<class MemType>
bool
InterconnectMaster<MemType>::grantData(){
    
    
    MemReqPtr req = thisCache->getMemReq();
    
    if(!req){
        thisInterconnect->incNullRequests();
        return false;
    }
    
    req->fromInterfaceID = interfaceID;
    req->firstSendTime = curTick;
    req->readOnlyCache = thisCache->isInstructionCache();
    req->toInterfaceID = thisInterconnect->getInterconnectID(req->toProcessorID);
    
    // make sure destination was set properly
    if(req->toProcessorID != -1) assert(req->toInterfaceID != -1);
    
    // Update send profile
    updateProfileValues(req);
    
    //Currently sends can't fail, so all reqs will be a success
    thisCache->sendResult(req, true);
    thisInterconnect->send(req, curTick, interfaceID);
    
    return thisCache->doMasterRequest();
}
template<class MemType>
MemReqPtr
InterconnectMaster<MemType>::getPendingRequest(){
    return thisCache->getMemReq();
}




