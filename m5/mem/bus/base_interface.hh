/*
 * Copyright (c) 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

/**
 * @file
 * Declaration of base bus interface class.
 */

#ifndef __MEM_BUS_BASE_INTERFACE_HH__
#define __MEM_BUS_BASE_INTERFACE_HH__

#include <list>
#include <vector>

#include <iostream>

#include "base/range.hh"
#include "mem/base_hier.hh"
#include "mem/mem_req.hh"

/* Enumeration for the possible interface types */
typedef enum{CROSSBAR, BUS, MEMORY, INTERCONNECT} InterfaceType;
        
/**
 * A base class for memory interface objects.
 */
class BaseInterface : public BaseHier
{
  protected:
    /**
     * True if this interface is blocked. Occurs when the connected memory is
     * blocked.
     */
    bool blocked;

    /** The address ranges this interface is responsible for. */
    std::vector<Range<Addr> > ranges;

  public:
    
    /**
     * Construct and initialize this interface.
     * @param name The name of this interface.
     * @param hier Pointer to the hierarchy wide parameters.
     */
    BaseInterface(const std::string &name, HierParams *hier);

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
    virtual bool grantAddr() = 0;

    /**
     * Called when this interface gets the data bus.
     * @return True if another request is outstanding.
     */
    virtual bool grantData() = 0;

    /**
     * Snoop for the request in the connected memory.
     * @param req The request to snoop for.
     */
    virtual void snoop(MemReqPtr &req) = 0;

    virtual void snoopResponse(MemReqPtr &req) = 0;

    virtual void snoopResponseCall(MemReqPtr &req) = 0;
    /**
     * Forward a probe to the bus.
     * @param req The request to probe.
     * @param update If true, update the hierarchy.
     * @return The estimated completion time.
     */
    virtual Tick sendProbe(MemReqPtr &req, bool update) = 0;

    /**
     * Probe the attached memory for the given request.
     * @param req The request to probe.
     * @param update If true, update the hierarchy.
     * @return The estimated completion time.
     */
    virtual Tick probe(MemReqPtr &req, bool update) = 0;
    
    /**
     * Returns true if this interface is blocked.
     * @return True if this interface is blocked.
     */
    bool isBlocked() const
    {
	return blocked;
    }

    /**
     * Mark this interface as blocked.
     */
    virtual void setBlocked() = 0;

    /**
     * Mark this interface as unblocked.
     */
    virtual void clearBlocked() = 0;

    /**
     * Collect the address ranges from the bus into the provided list.
     * @param range_list The list to store the address ranges into.
     */
    virtual void collectRanges(std::list<Range<Addr> > &range_list) = 0;

    /**
     * Add the address ranges of this interface to the provided list.
     * @param range_list The list of ranges.
     */
    virtual void getRange(std::list<Range<Addr> > &range_list) = 0;
    
    /**
     * Notify this interface of a range change on the bus.
     */
    virtual void rangeChange() = 0;

    /**
     * Set the address ranges of this interface to the list provided. This 
     * function removes any existing ranges.
     * @param range_list List of addr ranges to add.
     * @post range_list is empty.
     */
    virtual void setAddrRange(std::list<Range<Addr> > &range_list);

    /**
     * Add an address range for this interface.
     * @param range The addres range to add.
     */
    virtual void addAddrRange(const Range<Addr> &range)
    {
        ranges.push_back(range);
    }

    /**
     * Returns true if the address is covered in any of this interfaces ranges.
     * @param addr The address to check.
     * @return True if the address is found in a range.
     */
    virtual bool inRange(Addr addr)
    {
	for (int i = 0; i < ranges.size(); ++i) {
	    if (addr == ranges[i]) {
		return true;
	    }
	}
	return false;
    }

    virtual void addPrewrite(MemReqPtr &req) {
        fatal("ARGLE!");
    }
    
    virtual bool canPrewrite() {
        fatal("ARGLE!");
        return false;
    }

    virtual bool isActive(MemReqPtr &req) {
      fatal("Should not be called");
      return false;
    }

    virtual bool bankIsClosed(MemReqPtr &req) {
      fatal("Should not be called");
      return false;
    }

    virtual bool isReady(MemReqPtr &req) {
      fatal("Should not be called");
      return false;
    }

    virtual Tick calculateLatency(MemReqPtr &req) {
      fatal("Should not be called");
      return 0;
    }
    
    virtual Tick getDataTransTime(){
        fatal("Should not be called");
        return 0;
    }

//     virtual InterfaceType getInterfaceType() = 0;
    
//     virtual void setCurrentRequestAddr(Addr address) = 0;
    
};

#endif //__MEM_BUS_BASE_INTERFACE_HH__
