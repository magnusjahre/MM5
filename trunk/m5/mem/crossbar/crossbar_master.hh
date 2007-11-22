
#ifndef __CROSSBAR_MASTER_HH__
#define __CROSSBAR_MASTER_HH__

#include <iostream>

#include "base/range.hh"
#include "targetarch/isa_traits.hh" // for Addr
#include "mem/bus/base_interface.hh"

#include "crossbar_interface.hh"
#include "crossbar.hh"
        
class Crossbar;

template <class MemType>
class CrossbarMaster : public CrossbarInterface
{
    private:

        MemType* thisCache;
        bool isL2cache;
        
        void printRequestQueue();
        
        //debug
        std::vector<std::pair<Addr, Tick>* > outstandingRequestAddrs;
    
    public:
        CrossbarMaster(const std::string &name,
                       bool isL2,
                       Crossbar* crossbar,
                       MemType* cache,
                       HierParams *hier);

        /**
        * Access the connect memory to perform the given request.
        * @param req The request to perform.
        * @return The result of the access.
        */
        MemAccessResult access(MemReqPtr &req);
    
        /**
        * Request the address bus at the given time.
        * @param time The time to request the bus.
        */
        void request(Tick time);
    
        /**
        * Respond to the given request at the given time.
        * @param req The request being responded to.
        * @param time The time the response is ready.
        */
        void respond(MemReqPtr &req, Tick time){
            fatal("CrossbarMaster respond method not implemented");
        }

    
        /**
        * Called when this interface gets the data bus.
        * @return True if another request is outstanding.
        */
        bool grantData();
        
        void setCurrentRequestAddr(Addr address);
        
        InterfaceType getInterfaceType(){
            return CROSSBAR;
        }
        
        void addRequest(Addr address, int fromID, MemReqPtr &req){
            fatal("CrossbarMaster addRequest() not implemented");
        }
        
        void deliver(MemReqPtr &req);

};

#endif // CROSSBAR_MASTER_HH__
