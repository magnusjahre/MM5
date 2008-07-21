
#ifndef __INTERCONNECT_INTERFACE_HH__
#define __INTERCONNECT_INTERFACE_HH__

#include <iostream>

#include "base/range.hh"
#include "targetarch/isa_traits.hh" // for Addr
#include "mem/bus/base_interface.hh"

#include "interconnect.hh"
        
class Interconnect;

/**
* This class glues the interconnect extensions together with the rest of the M5
* memory system.
*
* This class is based on the bus_interface.hh class in standard M5 but has been
* rewritten from scratch. Consequently, a number of methods that are not used
* are not implemented.
*
* @author Magnus Jahre
*/
class InterconnectInterface : public BaseInterface
{

    protected:
        int interfaceID;
        Interconnect* thisInterconnect;
        bool trace_on;
        
        int dataSends;
        int instSends;
        int coherenceSends;
        int totalSends;
        bool doProfiling;
    
    public:
        
        /**
        * This constructor creates an interconnect interface and initialises
        * a few member variables.
        *
        * @param _interconnect A pointer to the interface this class interfaces
        *                      to.
        * @param name          The name of the class from the config file
        * @param hier          Hierarchy parameters for BaseHier
        */
        InterconnectInterface(Interconnect* _interconnect,
                              const std::string &name,
                              HierParams *hier)
            : BaseInterface(name, hier)
        {
            blocked = false;
            thisInterconnect = _interconnect;
            trace_on = false;
            
            dataSends = 0;
            instSends = 0;
            coherenceSends = 0;
            totalSends = 0;
            doProfiling = false;
        }
        
        /**
        * Mark this interface as blocked.
        */
        void setBlocked();
    
        /**
        * Mark this interface as unblocked.
        */
        void clearBlocked();

        /**
        * Access the connected memory and make it perform a given request.
        *
        * @param req The request to perform.
        *
        * @return The result of the access.
        */
        virtual MemAccessResult access(MemReqPtr &req) = 0;
    
        /**
        * Request access to the interconnect. 
        *
        * @param time The time to request the bus.
        */
        virtual void request(Tick time) = 0;
    
        /**
        * Respond to the given request at the given time.
        *
        * @param req The request being responded to.
        * @param time The time the response is ready.
        */
        virtual void respond(MemReqPtr &req, Tick time) = 0;
    
        /**
        * This method must be implemented to fit into the the rest of the M5
        * memory system. In this implemetation it is never used and issues
        * a fatal error message if it is called.
        *
        * @return Always false
        */
        bool grantAddr(){
            fatal("CrossbarInterface grantAddr() method not implemented\n");
            return false;
        }
    
        /**
        * This method is used when an interface is granted access to the bus.
        *
        * @return True if another request is outstanding.
        */
        virtual bool grantData() = 0;
        
        virtual bool grantData(int position){
            fatal("grantData(int position) should only be called on InterconnectSlave");
        }
    
        /**
        * The interconnects does not support snooping, so this method issues a 
        * fatal error message if it is called.
        *
        * @param req Not used
        */
        void snoop(MemReqPtr &req){
            fatal("CrossbarInterface snoop not implemented");
        }
    
        /**
        * The interconnects does not support snooping, so this method issues a 
        * fatal error message if it is called.
        *
        * @param req Not used
        */
        void snoopResponse(MemReqPtr &req){
            fatal("CrossbarInterface snoopResponse not implemented");
        }
        
        /**
        * The interconnects does not support snooping, so this method issues a 
        * fatal error message if it is called.
        *
        * @param req Not used
        */
        void snoopResponseCall(MemReqPtr &req){
            fatal("CrossbarInterface snoopResponse not implemented");
        }
        
        /**
        * This method is never called with the configurations used in this
        * work. Consequently, it is not used.
        *
        * @param req    Not used.
        * @param update Not used.
        *
        * @return Nothing
        */
        Tick sendProbe(MemReqPtr &req, bool update){
            fatal("CrossbarSlave sendProbe() method not implemented");
            return -1;
        }
    
        /**
        * This method is never called with the configurations used in this
        * work. Consequently, it is not used.
        *
        * @param req    Not used.
        * @param update Not used.
        *
        * @return Nothing
        */
        Tick probe(MemReqPtr &req, bool update){
            fatal("CrossbarSlave probe() method not implemented");
            return -1;
        }
    
        /**
        * This method is never called in the configuration used in this work
        * and is not implemented.
        *
        * @param range_list Not used
        */
        void collectRanges(std::list<Range<Addr> > &range_list){
            fatal("CrossbarSlave collectRanges() method not implemented");
        }
    
        /**
        * Adds the address ranges of this interface to the provided list.
        *
        * @param range_list The list of ranges.
        */
        void getRange(std::list<Range<Addr> > &range_list);
        
        /**
        * Notify this interface of a range change in the interconnect.
        */
        void rangeChange();
    
        /**
        * Set the address ranges of this interface to the list provided. This 
        * function removes any existing ranges.
        *
        * @param range_list List of addr ranges to add.
        */
        void setAddrRange(std::list<Range<Addr> > &range_list);
           
        /**
        * Add an address range for this interface.
        *
        * @param range The addres range to add.
        */
        void addAddrRange(const Range<Addr> &range);
        
        /**
        * This method returns the number of requests sent since the last time
        * it was called. Furthermore, it divides the sends into data sends,
        * instruction sends and coherence sends as well as providing a grand
        * total.
        *
        * @param dataSends  A pointer a memory location where the number of 
        *                   data sends can be stored
        * @param instSends  A pointer a memory location where the number of 
        *                   instruction sends can be stored
        * @param instSends  A pointer a memory location where the number of 
        *                   coherence sends can be stored
        * @param totalSends A pointer a memory location where the total number
        *                   of sends can be stored
        *
        * @see InterconnectProfile
        */
        void getSendSample(int* dataSends,
                           int* instSends,
                           int* coherenceSends,
                           int* totalSends);
        
        /**
        * This method ensures that the profile values are updated consistently
        * from all interfaces. It is called by the subclasses.
        *
        * @param req The current memory request
        * 
        * @see InterconnectProfile
        */
        void updateProfileValues(MemReqPtr &req);
        
        /**
        * This method delivers a reponse to the interconnect. In a slave 
        * interconnect, this sends the request over the interconnect. In a 
        * master interconnect, the request is delivered to the cache.
        *
        * @param req The request that will be delivered
        */
        virtual void deliver(MemReqPtr &req) = 0;
        
        /**
        * The interconnects often need to distinguish between a master and a 
        * slave interface in an efficient manner. This method enables this.
        *
        * @return True if the interface is a master interface
        */
        virtual bool isMaster() = 0;
        
        /**
        * This method accesses the cache and finds out which interface the
        * next request should be sent to. Then, it returns a pair of the 
        * address and the destination interface.
        *
        * Note that the destination interface might be -1. In this case, the
        * interconnect must find the destination itself by inspecting the 
        * destination address.
        *
        * @return The address and the destination interface. If the destination
        *         is -1, the interconnect must derive the destination based on
        *         the requested address.
        */
        virtual std::pair<Addr, int> getTargetAddr() = 0;
        
        virtual MemCmd getCurrentCommand() = 0;
        
        /**
        * This method returns the ID of the destination interface of the 
        * request at the front of the request queue in a slave interface.
        *
        * @return The destination of the next request to be sent from a slave
        *         interface.
        */
        virtual int getTargetId() = 0;
        
        /**
        * Convenience method that returns the name of the cache associated with
        * a given interface.
        *
        * @return The name of the cache associated with a given interface.
        */
        virtual std::string getCacheName() = 0;
        
        virtual int getRequestDestination(int numberInQueue){
            fatal("getRequestDestination should only be called on a InterconnectSlave");
            return -1;
        }

};

#endif // INTERCONNECT_INTERFACE_HH__
