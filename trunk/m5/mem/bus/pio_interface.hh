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
 * Declaration of a templatized general bus interface class.
 */

#ifndef __MEM_BUS_PIO_INTERFACE_HH__
#define __MEM_BUS_PIO_INTERFACE_HH__

#include "mem/bus/bus_interface.hh"

/**
 * A templatized base class for bus interface objects. Takes the type of bus
 * as a template parameter and implements basic shared functions
 */
template <class BusType, class Device>
class PioInterface : public BusInterface<BusType>
{
  protected:
    /** Typedef for the access function of the device. */
    typedef Tick (Device::*Function)(MemReqPtr &);
    /** The device connected to the interface. */
    Device *device;
    /** The access function of the device. */
    Function function;

  public:
    /**
     * Construct and initialize this interface.
     * @param name The name of this interface.
     * @param hier Pointer to the hierarchy wide parameters.
     * @param bus The bus this interface connects too.
     * @param device The connnected device.
     * @param function The access function for the device.
     */
    PioInterface(const std::string &name, HierParams *hier, BusType *bus,
		 Device *device, Function function);

    /**
     * Access the connect memory to perform the given request.
     * @param req The request to perform.
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
    virtual Tick probe(MemReqPtr &req, bool update);

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

    virtual void snoopResponseCall(MemReqPtr &req)
    {
	//We do nothing on requests no coherence on levels with
	//pio's
    }
};

/**
 * Create and return a new PIOInterface.
 * @param name The name of this interface.
 * @param hier Pointer to the hierarchy wide param.
 * @param bus The bus this interface connects to.
 * @param dev The device this interface is for.
 * @param func The access function for the device.
 * @return Pointer to new PioInterface
 */
template <class BusType, class Device>
inline PioInterface<BusType, Device> *
newPioInterface(const std::string &name, HierParams *hier, BusType *bus,
		Device *dev, Tick (Device::*func)(MemReqPtr &))
{
    return new PioInterface<BusType, Device>(name, hier, bus, dev, func);
}

#endif //__MEM_BUS_PIO_INTERFACE_HH__
