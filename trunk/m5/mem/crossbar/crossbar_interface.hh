
#ifndef __CROSSBAR_INTERFACE_HH__
#define __CROSSBAR_INTERFACE_HH__

#include <iostream>

#include "base/range.hh"
#include "targetarch/isa_traits.hh" // for Addr
#include "mem/bus/base_interface.hh"

#include "crossbar.hh"
        
class Crossbar;

class CrossbarInterface : public BaseInterface
{

    protected:
        int interfaceID;
        Crossbar* thisCrossbar;
        bool trace_on;
    
    public:
        CrossbarInterface(Crossbar* _crossbar,
                          const std::string &name,
                          HierParams *hier)
            : BaseInterface(name, hier)
        {
            blocked = false;
            thisCrossbar = _crossbar;
//             trace_on = true;
            trace_on = false;
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
        * Access the connect memory to perform the given request.
        * @param req The request to perform.
        * @return The result of the access.
        */
        virtual MemAccessResult access(MemReqPtr &req) = 0;
    
        /**
        * Request the address bus at the given time.
        * @param time The time to request the bus.
        */
        virtual void request(Tick time) = 0;
    
        /**
        * Respond to the given request at the given time.
        * @param req The request being responded to.
        * @param time The time the response is ready.
        */
        virtual void respond(MemReqPtr &req, Tick time) = 0;
    
        /**
        * Called when this interface gets the address bus.
        * @return True if a another request is outstanding.
        */
        bool grantAddr(){
            fatal("CrossbarInterface grantAddr() method not implemented\n");
            return false;
        }
    
        /**
        * Called when this interface gets the data bus.
        * @return True if another request is outstanding.
        */
        virtual bool grantData() = 0;
    
        /**
        * Snoop for the request in the connected memory.
        * @param req The request to snoop for.
        */
        void snoop(MemReqPtr &req){
            fatal("CrossbarInterface snoop not implemented");
        }
    
        void snoopResponse(MemReqPtr &req){
            fatal("CrossbarInterface snoopResponse not implemented");
        }
    
        void snoopResponseCall(MemReqPtr &req){
            fatal("CrossbarInterface snoopResponse not implemented");
        }
        
        /**
        * Forward a probe to the bus.
        * @param req The request to probe.
        * @param update If true, update the hierarchy.
        * @return The estimated completion time.
        */
        Tick sendProbe(MemReqPtr &req, bool update){
            fatal("CrossbarSlave sendProbe() method not implemented");
            return -1;
        }
    
        /**
        * Probe the attached memory for the given request.
        * @param req The request to probe.
        * @param update If true, update the hierarchy.
        * @return The estimated completion time.
        */
        Tick probe(MemReqPtr &req, bool update){
            fatal("CrossbarSlave probe() method not implemented");
            return -1;
        }
    
        /**
        * Collect the address ranges from the bus into the provided list.
        * @param range_list The list to store the address ranges into.
        */
        void collectRanges(std::list<Range<Addr> > &range_list){
            fatal("CrossbarSlave collectRanges() method not implemented");
        }
    
        /**
        * Add the address ranges of this interface to the provided list.
        * @param range_list The list of ranges.
        */
        void getRange(std::list<Range<Addr> > &range_list);
        
        /**
        * Notify this interface of a range change on the bus.
        */
        void rangeChange();
    
        /**
        * Set the address ranges of this interface to the list provided. This 
        * function removes any existing ranges.
        * @param range_list List of addr ranges to add.
        * @post range_list is empty.
        */
        void setAddrRange(std::list<Range<Addr> > &range_list);
           
        /**
        * Add an address range for this interface.
        * @param range The addres range to add.
        */
        void addAddrRange(const Range<Addr> &range);
        
        virtual void setCurrentRequestAddr(Addr address) = 0;
        
        virtual InterfaceType getInterfaceType() = 0;
        
        virtual void addRequest(Addr address, int fromID, MemReqPtr &req) = 0;
        
        virtual void deliver(MemReqPtr &req) = 0;

};

#endif // CROSSBAR_INTERFACE_HH__
