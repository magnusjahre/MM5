
#include <iostream>
#include <vector>


#include "crossbar.hh"
#include "crossbar_master.hh"


using namespace std;

template<class MemType>
CrossbarMaster<MemType>::CrossbarMaster(const string &name, 
                                        bool isL2,
                                        Crossbar* crossbar,
                                        MemType* cache,
                                        HierParams *hier)
    : CrossbarInterface(crossbar, name, hier)
{
    thisCache = cache;
    isL2cache = isL2;

    interfaceID = thisCrossbar->registerInterface(this, isL2cache);
}

template<class MemType>
MemAccessResult
CrossbarMaster<MemType>::access(MemReqPtr &req){
    
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
CrossbarMaster<MemType>::deliver(MemReqPtr &req){
    
    
    pair<Addr, Tick>* hitPair = NULL;
    int hitIndex = -1;
    for(int i=0;i<outstandingRequestAddrs.size();++i){
        pair<Addr, Tick>* tmpPair = outstandingRequestAddrs[i];
        if(req->paddr == tmpPair->first){
            hitIndex = i;
            hitPair = tmpPair;
        }
    }
    
    /* if this is a request we don't need, we won't deliver it to the cache*/
    //if(hitIndex < 0) return;
    
    assert(hitIndex >= 0); /* check if this actually was an answer to something we requested */
    outstandingRequestAddrs.erase(outstandingRequestAddrs.begin()+hitIndex);
    delete hitPair;
    
    if(trace_on){
        cout << "Master "<< interfaceID <<" is waiting for: ";
        for(int i=0;i<outstandingRequestAddrs.size();++i){
            cout << "(" << outstandingRequestAddrs[i]->first << ", " << outstandingRequestAddrs[i]->second << ") ";
        }
        cout << "at tick " << curTick << "\n";
    }
    
    
    if(trace_on) cout << "TRACE: MASTER_RESPONSE id " << interfaceID << " addr " << req->paddr << " at " << curTick << "\n";
    thisCache->handleResponse(req);
}

template<class MemType>
void
CrossbarMaster<MemType>::request(Tick time){

    if(trace_on) cout << "TRACE: MASTER_REQUEST id " << interfaceID << " at " << curTick << "\n";
    
    thisCrossbar->request(time,
                          thisCrossbar->getL2interfaceID(),
                          interfaceID);
}

template<class MemType>
bool
CrossbarMaster<MemType>::grantData(){
    
//     cout << "MASTER (" << interfaceID << ") calling getReq, current addr " << currentRequest->address << " time is " << curTick << "\n";
    MemReqPtr req = thisCache->getMemReq();
    
    if(!req){
        thisCrossbar->incNullRequests();
        return false;
    }
    if(trace_on) cout << "TRACE: MASTER_SEND " << req->cmd.toString() << " id " << interfaceID << " addr " << req->paddr << " at " << curTick << "\n";
    
    if(!req->cmd.isNoResponse()){
        outstandingRequestAddrs.push_back(new pair<Addr, Tick>(req->paddr, curTick));
    }
    
    req->crossbarId = interfaceID;

    thisCrossbar->sendData(req, curTick, thisCrossbar->getL2interfaceID(), interfaceID);
    thisCache->sendResult(req, true); //Currently sends can't fail, so all reqs will be a success
    
    return thisCache->doMasterRequest();
}


template<class MemType>
void
CrossbarMaster<MemType>::setCurrentRequestAddr(Addr address){
    /* not needed */
}

