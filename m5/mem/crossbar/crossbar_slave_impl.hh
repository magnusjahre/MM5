
#include <iostream>
#include <vector>

//#include "base/range.hh"
//#include "targetarch/isa_traits.hh" // for Addr
//#include "mem/bus/base_interface.hh"

#include "crossbar.hh"
#include "crossbar_slave.hh"

        
using namespace std;

template<class MemType>
CrossbarSlave<MemType>::CrossbarSlave(const string &name, 
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
CrossbarSlave<MemType>::access(MemReqPtr &req){

    //NOTE: more or less copied from crossbar slave interface
    bool already_satisfied = req->isSatisfied();
    if (already_satisfied) {
        warn("Request is allready satisfied (CrossbarSlave: access())");
        return BA_NO_RESULT;
    }
    
    
    if (this->inRange(req->paddr)) {
        assert(!blocked);

        if(trace_on) cout << "TRACE: SLAVE_ACCESS from id " << req->crossbarId << " addr " << req->paddr << " at " << curTick << "\n";
        thisCache->access(req);

        assert(!this->isBlocked() || thisCache->isBlocked());

        if (this->isBlocked()) {
            return BA_BLOCKED; //Out of MSHRS, now we block  
        } else {
            return BA_SUCCESS;// This transaction went through ok
        }
    }
    else{
        warn("Request was not in CrossbarSlave()'s range");
    }

    return BA_NO_RESULT;
}

template<class MemType>
void
CrossbarSlave<MemType>::respond(MemReqPtr &req, Tick time){

    if (!req->cmd.isNoResponse()) {
        if(trace_on) cout << "TRACE: SLAVE_RESPONSE " << req->cmd.toString() << " from id " << req->crossbarId << " addr " << req->paddr << " at " << curTick << "\n";
        responseQueue.push_back(new CrossbarResponse(req, time));
        thisCrossbar->request(time, req->crossbarId, interfaceID);
    }
}


template<class MemType>
bool
CrossbarSlave<MemType>::grantData(){
    
    assert(responseQueue.size() > 0);
    CrossbarResponse* response = responseQueue.front();
    responseQueue.erase(responseQueue.begin());
    
    MemReqPtr req = response->req;
    int toAddr = req->crossbarId;
    thisCrossbar->sendData(req, curTick, toAddr, interfaceID);
    
    delete response;
    
    return !responseQueue.empty();
}


