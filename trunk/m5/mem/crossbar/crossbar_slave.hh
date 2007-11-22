
#ifndef __CROSSBAR_SLAVE_HH__
#define __CROSSBAR_SLAVE_HH__

#include <iostream>

#include "base/range.hh"
#include "targetarch/isa_traits.hh" // for Addr
#include "mem/bus/base_interface.hh"

#include "crossbar_interface.hh"
#include "crossbar.hh"
        
class Crossbar;

template <class MemType>
class CrossbarSlave : public CrossbarInterface
{
    private:
        
        class CrossbarResponse{
            
            public:
                MemReqPtr req;
                Tick time;
            
                CrossbarResponse(MemReqPtr &_req, Tick _time)
                : req(_req), time(_time)
                {
                }
        };

        MemType* thisCache;
        bool isL2cache;
        
        std::vector<CrossbarResponse* > responseQueue;
        
    public:
        CrossbarSlave(const std::string &name,
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
        void request(Tick time){
            fatal("CrossbarSlave request(Tick time) not implemented");
        }
        
    
        /**
        * Respond to the given request at the given time.
        * @param req The request being responded to.
        * @param time The time the response is ready.
        */
        void respond(MemReqPtr &req, Tick time);
    
        /**
        * Called when this interface gets the data bus.
        * @return True if another request is outstanding.
        */
        bool grantData();
        
        void addRequest(Addr address, int fromID, MemReqPtr &req){
            /* not needed */
        }
        
        void setCurrentRequestAddr(Addr address){
            /* not needed */
        }
        
        InterfaceType getInterfaceType(){
            return CROSSBAR;
        }
        
        void deliver(MemReqPtr &req){
            fatal("CrossbarSlave deliver() not implemented");
        }
};

#endif // CROSSBAR_INTERFACE_HH__
