/*
 * Copyright (c) 2003, 2004, 2005
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
 * Declaration of a master bus interface.
 */

#ifndef __MEM_BUS_MASTER_INTERFACE_HH__
#define __MEM_BUS_MASTER_INTERFACE_HH__

#include "mem/bus/bus_interface.hh"

/**
 * Templatized master bus interface. Generally connects a cache to the bus
 * with its supplier.
 */
template<class MemType, class BusType>
class MasterInterface : public BusInterface<BusType>
{
  protected:
    /** The memory that uses this interface. */
    MemType *mem;
  public:

    /**
     * Constructs and initializes this interface.
     * @param name The name of this interface.
     * @param hier Pointer to the hierarchy wide parameters.
     * @param _mem The memory to connect to.
     * @param bus The bus to connect to.
     */
    MasterInterface(const std::string &name, HierParams *hier,
		    MemType *_mem, BusType *bus);

    /**
     * Destructor.
     */
    virtual ~MasterInterface()
    {
    }

    // From CPU -> Memory
    /**
     * Called when this interface gets the address bus. Gets a queued
     * request from the memory and forwards it on the bus.
     * @return True if a another request is outstanding.
     */
    virtual bool grantAddr(Tick requestedAt);

    /**
     * Deliver a response to the connected memory.
     * @param req The request being responded to.
     */
    virtual void deliver(MemReqPtr &req);

    // from Memory ->CPU
    /**
     * Snoop the memory for a response to the bus request.
     * @param req The request on the bus.
     * @return The result of the snoop.
     */
    virtual MemAccessResult access(MemReqPtr &req);

    /**
     * Called when this interface gets the data bus. Send an queued
     * snoop response.
     * @return True if another response is outstanding.
     */
    virtual bool grantData();

    /**
     * Called by the cache to respond to a snoop at the given time.
     * Queues the response and requests the data bus.
     * @param req The bus request being responded to.
     * @param time The time the response it ready.
     */
    virtual void respond(MemReqPtr &req, Tick time);

    /**
     * Probe the attached memory for the given request.
     * @param req The request to probe.
     * @param update If true, update the hierarchy.
     * @return The estimated completion time.
     */
    virtual Tick probe(MemReqPtr &req, bool update)
    {
	return mem->snoopProbe(req, update);
    }

    /**
     * Add the address ranges of this interface to the provided list.
     * @param range_list The list of ranges.
     */
    virtual void getRange(std::list<Range<Addr> > &range_list) 
    {
	range_list.push_back(RangeIn((Addr)0, MaxAddr));
    }
		
    /**
     * Notify this interface of a range change on the bus.
     */
    virtual void rangeChange()
    {
	mem->rangeChange();
    }

    virtual void snoopResponseCall(MemReqPtr &req);

    virtual void addPrewrite(MemReqPtr &req); 
    virtual bool canPrewrite(); 
};

#endif // __MEM_BUS_MASTER_INTERFACE_HH__
