
#ifndef __INTERCONNECT_MASTER_HH__
#define __INTERCONNECT_MASTER_HH__

#include <iostream>

#include "base/range.hh"
#include "targetarch/isa_traits.hh" // for Addr
#include "mem/bus/base_interface.hh"

#include "interconnect_interface.hh"
#include "interconnect.hh"
        
class Interconnect;

/**
* This class implements a master interface as needed by the M5 cache 
* implementation. In the terminiology of M5, master is synonymous with 'on the
* processor side of an interconnect'.
*
* This class is based on the master_interface files in the original M5 memory
* system but has been rewritten from scratch.
*
* @author Magnus Jahre
*/
template <class MemType>
class InterconnectMaster : public InterconnectInterface
{
    private:

        MemType* thisCache;
    
    public:
        /**
        * This constructor creates a master interface and register it with the
        * associated interconnect. In the terminiology of M5, master is 
        * synonymous with 'on the processor side of an interconnect'
        *
        * @param name         The name of the interface from the config file
        * @param interconnect A pointer to the associated interconnect
        * @param cache        A pointer to the associated cache
        * @param hier         Hierarchy parameters for BaseHier
        */
        InterconnectMaster(const std::string &name,
                           Interconnect* interconnect,
                           MemType* cache,
                           HierParams *hier);

        /**
        * Access the connect memory to perform the given request.
        *
        * @param req The request to perform.
        *
        * @return The result of the access.
        */
        MemAccessResult access(MemReqPtr &req);
    
        /**
        * Request access to the interconnect at the given time.
        *
        * @param time The time to request the bus.
        */
        void request(Tick time);
    
        /**
        * Responses are carried out through the deliver method in the master
        * interface. Consequently, this method exits with an error message if
        * it is called.
        *
        * @param req  Not used.
        * @param time Not used.
        */
        void respond(MemReqPtr &req, Tick time){
            fatal("CrossbarMaster respond method not implemented");
        }

        /**
        * When the interface is granted access to the interconnect, this method
        * is called. It retrieves the request with the highest priority from
        * the cache and provides it to the send method in the interconnect.
        *
        * @return True if another request is outstanding.
        */
        bool grantData();
        
        /**
        * This method delivers the request to the associated cache.
        *
        * @param req The memory request to deliver
        */
        void deliver(MemReqPtr &req);
        
        /**
        * Convenience method that identifies this interface as a master
        * interface.
        *
        * @return True, since this is a master interface
        */
        bool isMaster(){
            return true;
        }
        
        /**
        * This method is only valid for slave interfaces and produces a fatal
        * error message if it is called.
        *
        * @return Nothing
        */
        int getTargetId(){
            fatal("getTargetId() not valid for a MasterInterface");
            return -1;
        }
        
        /**
        * Convenience method that returns the name of the associated cache.
        *
        * @return The name of associated cache
        */
        std::string getCacheName(){
            return thisCache->name();
        }
        
        virtual MemReqPtr getPendingRequest();
};

#endif // __INTERCONNECT_MASTER_HH__
