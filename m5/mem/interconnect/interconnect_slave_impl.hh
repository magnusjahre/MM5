
#include <iostream>
#include <vector>

#include "interconnect.hh"
#include "interconnect_slave.hh"

using namespace std;

template<class MemType>
InterconnectSlave<MemType>::InterconnectSlave(const string &name,
                                              Interconnect* interconnect,
                                              MemType* cache,
                                              HierParams *hier)
    : InterconnectInterface(interconnect, name, hier)
{
    thisCache = cache;

    interfaceID = thisInterconnect->registerInterface(this,
                                                      true,
                                                      cache->getProcessorID());
    thisCache->setInterfaceID(interfaceID);

}

template<class MemType>
MemAccessResult
InterconnectSlave<MemType>::access(MemReqPtr &req){

    bool already_satisfied = req->isSatisfied();
    if (already_satisfied && !req->cmd.isDirectoryMessage()) {
        warn("Request is allready satisfied (InterconnectSlave: access())");
        return BA_NO_RESULT;
    }
    
    if (this->inRange(req->paddr)) {
        assert(!blocked);
        
        thisCache->access(req);

        assert(!this->isBlocked() || thisCache->isBlocked());

        if (this->isBlocked()) {
            //Out of MSHRS, now we block
            return BA_BLOCKED; 
        } else {
            // This transaction went through ok
            return BA_SUCCESS;
        }
    }

    return BA_NO_RESULT;
}

template<class MemType>
void
InterconnectSlave<MemType>::respond(MemReqPtr &req, Tick time){
    
    if (!req->cmd.isNoResponse()) {
        
        // handle directory requests
        if(req->toProcessorID != -1){
            // the sender interface recieves slave responses
            req->fromInterfaceID = 
                    thisInterconnect->getInterconnectID(req->toProcessorID);
            assert(req->fromInterfaceID != -1);
        }
        
        responseQueue.push_back(new InterconnectResponse(req, time));
        thisInterconnect->request(time, interfaceID);
    }
}


template<class MemType>
bool
InterconnectSlave<MemType>::grantData(){
    
    assert(responseQueue.size() > 0);
    InterconnectResponse* response = responseQueue.front();
    responseQueue.erase(responseQueue.begin());
    
    MemReqPtr req = response->req;
    
    // Update send profile
    updateProfileValues(req);
    
    thisInterconnect->send(req, curTick, interfaceID);
    
    delete response;
    
    return !responseQueue.empty();

}

template<class MemType>
bool
InterconnectSlave<MemType>::grantData(int position){
    
    assert(position >= 0 && position < responseQueue.size());
    InterconnectResponse* response = responseQueue[position];
    responseQueue.erase(responseQueue.begin() + position);
    
    MemReqPtr req = response->req;
    
    // Update send profile
    updateProfileValues(req);
    
    thisInterconnect->send(req, curTick, interfaceID);
    
    delete response;
    
    return !responseQueue.empty();
}

template<class MemType>
bool
InterconnectSlave<MemType>::inRange(Addr addr)
{
    assert(thisCache != NULL);
    
    if(thisCache->isModuloAddressedBank()){
        
        int bankID = thisCache->getBankID();
        int bankCount = thisCache->getBankCount();
        
        int localBlkSize = thisCache->getBlockSize();
        int bitCnt = 1;
        assert(localBlkSize != 0);
        while((localBlkSize >>= 1) != 1) bitCnt++;
        
        assert((thisCache->getBlockSize()-1 & addr) == 0);
        assert(bankID != -1);
        assert(bankCount != -1);
        
        Addr effectiveAddr = addr >> bitCnt;
        
        if((effectiveAddr % bankCount) == bankID) return true;
        return false;
    }
    else{
        for (int i = 0; i < ranges.size(); ++i) {
            if (addr == ranges[i]) {
                return true;
            }
        }
        return false;
    }
}

