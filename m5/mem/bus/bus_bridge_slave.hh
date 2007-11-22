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
 * Declaration of the slave interface for a BusBridge.
 */

#ifndef __BUS_BRIDGE_SLAVE_HH__
#define __BUS_BRIDGE_SLAVE_HH__

#include "mem/bus/bus_interface.hh"
#include "mem/bus/bus.hh"

class BusBridge;

class BusBridgeSlave : public BusInterface<Bus>
{
  protected:
    BusBridge *bb;

    bool inCollectRanges;
    
    bool changingRange;

  public:
    /**
     * Build this interface and register it with the given bus.
     * @param name The name of this interface.
     * @param hier The memory hierarchy parameters.
     * @param bus The bus this interface interacts with.
     */
    BusBridgeSlave(const std::string &name, HierParams *hier, Bus *bus, 
		   BusBridge *bridge);

    /**
     * Free the internal storage.
     */
    virtual ~BusBridgeSlave() {}
    
    /**
     * Forward the request to the bus bridge.
     * @param req The request to perform.
     * @return The result of the access.
     */
    virtual MemAccessResult access(MemReqPtr &req);

    /**
     * Schedule a response from the BusBridge for the given time. Queues the
     * the request and requests the data bus.
     * @param req The request being responded to.
     * @param time The time the response is ready.
     */
    virtual void respond(MemReqPtr &req, Tick time);

    /**
     * Called when this interface gets the address bus.
     * @return True if a another request is outstanding.
     */
    virtual bool grantAddr();

    /**
     * Called when this interface gets the data bus.
     * @return True if another request is outstanding.
     */
    virtual bool grantData();

    /**
     * Forward a response to the bus bridge.
     * @param req The request being responded to.
     */
    virtual void deliver(MemReqPtr &req);

    /**
     * Forward a probe through the BusBridge.
     * @param req The request to probe.
     * @param update If true, update the hierarchy.
     * @return The estimated completion time.
     */
    virtual Tick probe(MemReqPtr &req, bool update);

    /**
     * Collect the address ranges from the bus into the provided list.
     * @param range_list The list to store the address ranges into.
     */
    virtual void collectRanges(std::list<Range<Addr> > &range_list);
    
    /**
     * Add the address ranges of this interface to the provided list.
     * @param range_list The list of ranges.
     */
    virtual void getRange(std::list<Range<Addr> > &range_list);
    
    /**
     * Notify this interface of a range change on the bus.
     */
    virtual void rangeChange();

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
    virtual void addAddrRange(const Range<Addr> &range);

    virtual void snoopResponseCall(MemReqPtr &req)
    {
	//We do nothing on request responses for now
    }
};

#endif //__BUS_BRIDGE_SLAVE_HH__
