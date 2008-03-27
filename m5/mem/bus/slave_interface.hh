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
 * Declaration of templatized slave interface.
 */

#ifndef __MEM_BUS_SLAVE_INTERFACE_HH__
#define __MEM_BUS_SLAVE_INTERFACE_HH__

#include "mem/bus/bus_interface.hh"

// Forward Decl
class MemTraceWriter;

/**
 * Templatized slave bus interface. Generally connects a memory, cache/main
 * memory, to a bus as a supplier.
 */
template <class MemType, class BusType>
class SlaveInterface : public BusInterface<BusType>
{
  public:
    /** The memory that uses this interface. */
    MemType *mem;

  protected:
    /** A memory trace writer. */
    MemTraceWriter *memTrace;
    
  public:
    /**
     * Constructs and initializes this interface.
     * @param name The name of this interface.
     * @param hier Pointer to the hierarchy wide parameters.
     * @param _mem The memory to connect to.
     * @param bus The bus to connect to.
     * @param mem_trace The memory trace to write references to.
     */
    SlaveInterface(const std::string &name, HierParams *hier, MemType *_mem, 
		   BusType *bus, MemTraceWriter *mem_trace);

    /**
     * Destructor.
     */
    virtual ~SlaveInterface()
    {
    }

    // from CPU -> Memory

    /**
     * Forwards a request to the memory.
     * @param req The request to forward.
     * @return The result of the access.
     */
    virtual MemAccessResult access(MemReqPtr &req);

    /**
     * Called when this interface gets the data bus. Send a queued response.
     */
    virtual bool grantData();

    /**
     * Called by the memory to respond to a request. Queues the response and
     * requests the data bus.
     * @param req The request being responded to.
     * @param time The time the response is ready.
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
	return mem->probe(req,update);
    }

    // from Memory -> CPU

    /**
     * Called when this interface gets the address bus. Gets a queued coherence
     * message from the memory and forward it to the bus.
     * @return True if there are more queued messages.
     */
    virtual bool grantAddr();    

    /**
     * Notify this interface of a range change on the bus.
     */
    virtual void rangeChange() {}

    /**
     * Set the address ranges of this interface to the list provided. This 
     * function removes any existing ranges.
     * @param range_list List of addr ranges to add.
     * @post range_list is empty.
     */
    virtual void setAddrRange(std::list<Range<Addr> > &range_list)
    {
	BaseInterface::setAddrRange(range_list);
	this->bus->rangeChange();
    }

    /**
     * Add an address range for this interface.
     * @param range The addres range to add.
     */
    virtual void addAddrRange(const Range<Addr> &range)
    {
	BaseInterface::addAddrRange(range);
	this->bus->rangeChange();
    }

    virtual void snoopResponseCall(MemReqPtr &req);
    //virtual void addPrewrite(MemReqPtr &req) {
    //    fatal("Not implemented!");
    //}

    virtual bool isActive(MemReqPtr &req);
    virtual bool bankIsClosed(MemReqPtr &req);
    virtual bool isReady(MemReqPtr &req);

    virtual Tick calculateLatency(MemReqPtr &req);
    
    virtual Tick getDataTransTime();
};

#endif // __MEM_BUS_SLAVE_INTERFACE_HH__
