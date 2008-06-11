
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
    
    currentAddr = 0;
    currentToCpuId = -1;
    currentValsValid = false;
    
    if(trace_on) cout << "InterconnectMaster with id "
                      << interfaceID << " created\n";
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
    
    if(!req->cmd.isDirectoryMessage()){
        pair<Addr, Tick>* hitPair = NULL;
        int hitIndex = -1;
        for(int i=0;i<outstandingRequestAddrs.size();++i){
            pair<Addr, Tick>* tmpPair = outstandingRequestAddrs[i];
            if(req->paddr == tmpPair->first){
                hitIndex = i;
                hitPair = tmpPair;
            }
        }
        
        /* check if this actually was an answer to something we requested */
        assert(hitIndex >= 0);
        outstandingRequestAddrs.erase(outstandingRequestAddrs.begin()+hitIndex);
        delete hitPair;
    } 
    
    if(trace_on){ 
        cout << "Master "<< interfaceID <<" is waiting for (response): ";
        for(int i=0;i<outstandingRequestAddrs.size();++i){
            cout << "(" << outstandingRequestAddrs[i]->first 
                 << ", " << outstandingRequestAddrs[i]->second << ") ";
        }
        cout << "at tick " << curTick << "\n";
    }
    
    if(trace_on) cout << "TRACE: MASTER_RESPONSE id " << interfaceID 
                      << " addr " << req->paddr << " at " << curTick << "\n";
    thisCache->handleResponse(req);
}

template<class MemType>
void
InterconnectMaster<MemType>::request(Tick time){

    if(trace_on) cout << "TRACE: MASTER_REQUEST id " << interfaceID << " at " 
                      << curTick << "\n";
    
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
    
    if(trace_on) cout << "TRACE: MASTER_SEND " << req->cmd.toString() 
                      << " from id " << interfaceID << " addr " << req->paddr 
                      << " at " << curTick << "\n";
    
    if(!req->cmd.isNoResponse()){
        if(!req->cmd.isDirectoryMessage()){
            outstandingRequestAddrs.push_back(
                    new pair<Addr, Tick>(req->paddr, curTick));
        }
    }
    
    req->fromInterfaceID = interfaceID;
    req->firstSendTime = curTick;
    req->readOnlyCache = thisCache->isInstructionCache();
    req->toInterfaceID = 
            thisInterconnect->getInterconnectID(req->toProcessorID);
    
    // make sure destination was set properly
    if(req->toProcessorID != -1) assert(req->toInterfaceID != -1);

    if(currentValsValid){
        assert(currentAddr == req->paddr);
        assert(currentToCpuId == req->toProcessorID);
        currentValsValid = false;
    }
    
    // Update send profile
    updateProfileValues(req);
    
    //Currently sends can't fail, so all reqs will be a success
    thisCache->sendResult(req, true);
    thisInterconnect->send(req, curTick, interfaceID);
    
    return thisCache->doMasterRequest();
}

template<class MemType>
pair<Addr, int>
InterconnectMaster<MemType>::getTargetAddr(){
    MemReqPtr currentRequest = thisCache->getMemReq();
    
    if(!currentRequest) return pair<Addr,int>(0,-2);
    assert(currentRequest->paddr != 0);
    
    int toInterface = 
            thisInterconnect->getInterconnectID(currentRequest->toProcessorID);
    if(currentRequest->toProcessorID != -1) assert(toInterface != -1);
    
    currentAddr = currentRequest->paddr;
    currentToCpuId = toInterface;
    currentValsValid = true;
    
    return pair<Addr,int>(currentRequest->paddr, toInterface);
}

template<class MemType>
MemCmd
InterconnectMaster<MemType>::getCurrentCommand(){
    MemReqPtr currentRequest = thisCache->getMemReq();
    assert(currentRequest);
    return currentRequest->cmd;
}


