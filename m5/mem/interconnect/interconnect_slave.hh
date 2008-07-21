
#ifndef __INTERCONNECT_SLAVE_HH__
#define __INTERCONNECT_SLAVE_HH__

#include <iostream>

#include "base/range.hh"
#include "targetarch/isa_traits.hh" // for Addr
#include "mem/bus/base_interface.hh"

#include "interconnect_interface.hh"
#include "interconnect.hh"
        
class Interconnect;

/**
* This class implements a slave interface as needed by the M5 cache 
* implementation. In the terminiology of M5, slave is synonymous with 'on the
* memory side of an interconnect'
*
* This class is based on the slave_interface files in the original M5 memory
* system but has been rewritten from scratch.
*
* @author Magnus Jahre
*/
template <class MemType>
class InterconnectSlave : public InterconnectInterface
{
    private:
        
        /**
        * Convenience class for storing cache responses from they are requested
        * until they are granted access.
        *
        * @author Magnus Jahre
        */
        class InterconnectResponse{
            
            public:
                MemReqPtr req;
                Tick time;
            
                /**
                * Stores the request and the time it was requested in this 
                * object.
                *
                * @param _req  The memory request waiting for access to the 
                *              interconnect
                * @param _time The clock cycle the response was recieved
                *
                * @author Magnus Jahre
                */
                InterconnectResponse(MemReqPtr &_req, Tick _time)
                {
                    req = _req;
                    time = _time;
                }
        };

        MemType* thisCache;
        
        std::vector<InterconnectResponse* > responseQueue;
        
    public:
        /**
        * This constructor creates an interconnect slave interface and
        * registers it with the provided interconnect.
        *
        * @param name         The name from the config file
        * @param interconnect A pointer to the interconnect
        * @param cache        A pointer to the cache
        * @param hier         Hierarchy parameters for BaseHier
        */
        InterconnectSlave(const std::string &name,
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
        * The request method is not needed in slave interfaces and is not
        * implemented. If it is called, it issues a fatal error message.
        *
        * @param time Not used.
        */
        void request(Tick time){
            fatal("InterconnectSlave request(Tick time) not implemented");
        }
        
    
        /**
        * The respond method is called when the connected cache responds to an
        * access. Then, an InterconnectResponse object is allocated and put 
        * into a queue. Furthermore, access to the interconnect is requested.
        *
        * @param req  The request being responded to.
        * @param time The time the response is ready.
        *
        * @see InterconnectResponse
        */
        void respond(MemReqPtr &req, Tick time);
    
        /**
        * Called when this interface is granted access to the interconnect.
        *
        * @return True if another request is outstanding.
        */
        bool grantData();
        
        bool grantData(int position);
        
        /**
        * The deliver method is not used in a slave interface and issues a
        * fatal error message if it is called.
        *
        * @param req Not used.
        */
        void deliver(MemReqPtr &req){
            fatal("InterconnectSlave deliver() not implemented");
        }
        
        /**
        * Since this is a slave interface, this method always returns false.
        *
        * @return False, since this is a slave interface. 
        */
        bool isMaster(){
            return false;
        }
        
        std::pair<Addr, int> getTargetAddr(){
            assert(!responseQueue.empty());
            return std::pair<Addr, int>(responseQueue.front()->req->paddr,
                                        responseQueue.front()->req->fromInterfaceID);
        }
        
        MemCmd getCurrentCommand(){
            assert(!responseQueue.empty());
            return responseQueue.front()->req->cmd;
        }
        
        /**
        * This method is used to find the destination interface of the request
        * at the head of response queue. This information is stored in the 
        * request, so this method simply accesses this information.
        *
        * @return The destination interface.
        */
        int getTargetId(){
            assert(!responseQueue.empty());
            return responseQueue.front()->req->fromInterfaceID;
        }
        
        /**
        * Retrieves the name of the associated cache.
        *
        * @return The name of the associated cache.
        */
        std::string getCacheName(){
            return thisCache->name();
        }
        
        /**
        * This method overloaded here to implement modulo bank addressing.
        * The default in M5 is that each bank is responsible for a contigous
        * part of the address space. This functionality is retained, but in 
        * addition it implements modulo addressing. In this case the address 
        * modulo the number of banks is used to decide which bank should 
        * service a given request.
        *
        * A configuration option in the cache selects which bank addressing
        * type should be used.
        *
        * @param addr The address to be checked
        *
        * @return True if this interface is responsible for this address.
        */
        virtual bool inRange(Addr addr);
        
        virtual int getRequestDestination(int numberInQueue);

};

#endif // __INTERCONNECT_SLAVE_HH__
