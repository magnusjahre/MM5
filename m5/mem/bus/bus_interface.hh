/*
 * Copyright (c) 2002, 2003, 2004, 2005
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

#ifndef __MEM_BUS_BUS_INTERFACE_HH__
#define __MEM_BUS_BUS_INTERFACE_HH__

#include "mem/bus/base_interface.hh"

/**
 * A templatized base class for bus interface objects. Takes the type of bus
 * as a template parameter and implements basic shared functions
 */
template <class BusType>
class BusInterface : public BaseInterface
{
  protected:
    /** The bus this interface is connected to. */
    BusType *bus;
    
    /** The bus ID of this interface. */
    uint8_t id;

    /**
     * Strutcture to hold the info for a data response or write data delay.
     */
    struct DataResponseEntry
    {
	/** The request being responded to. */
	MemReqPtr req;
	/** The size of the write data to transmit. */
	int size;
	/** The time the data is ready. */
	Tick time;
        
        /** Sender CPU ID, for Adaptive MHA*/
        int senderID;
        
        MemCmdEnum cmd;
	
	/** Contstruct a response entry. */
	DataResponseEntry(MemReqPtr &_req, Tick _time) 
	    : req(_req), size(0), time(_time), senderID(-1), cmd(InvalidCmd) {}

	/** Construct a data delay entry. */
	DataResponseEntry(int _size, Tick _time)
            : req(NULL), size(_size), time(_time), senderID(-1), cmd(InvalidCmd) {}
        
        /** Construct a data delay entry for use with AdaptiveMHA */
        DataResponseEntry(int _size, Tick _time, int _senderID, MemCmdEnum _cmd)
            : req(NULL), size(_size), time(_time), senderID(_senderID), cmd(_cmd) {}

       
    };  
        
    /** A list of responses to send on the data bus. */
    std::list<DataResponseEntry> responseQueue;

  public:
    /**
     * Construct and initialize this interface.
     * @param name The name of this interface.
     * @param hier Pointer to the hierarchy wide parameters.
     * @param bus The bus this interface connects too.
     * @param master True if this interface is a master interface.
     */
    BusInterface(const std::string &name, HierParams *hier, BusType *bus,
		 bool master);

    /**
     * Return the bus id of this interface.
     * @return The bus ID of this interface.
     */
    uint8_t getId() const
    {
	return id;
    }

    /**
     * Access the connect memory to perform the given request.
     * @param req The request to perform.
     * @return The result of the access.
     */
    virtual MemAccessResult access(MemReqPtr &req);

    /**
     * Request the address bus at the given time.
     * @param time The time to request the bus.
     */
    virtual void request(Tick time)
    {
	bus->requestAddrBus(id, time);
    }

    /**
     * Respond to the given request at the given time.
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
     * Deliver a response to the connected memory.
     * @param req The request being responded to.
     */
    virtual void deliver(MemReqPtr &req);

    /**
     * Snoop for the request in the connected memory.
     * @param req The request to snoop for.
     */
    virtual void snoop(MemReqPtr &req);

    virtual void snoopResponse(MemReqPtr &req);

    virtual void snoopResponseCall(MemReqPtr &req)
    {
	fatal("Not Implemented\n");
    }

    /**
     * Forward a probe to the bus.
     * @param req The request to probe.
     * @param update If true, update the hierarchy.
     * @return The estimated completion time.
     */
    virtual Tick sendProbe(MemReqPtr &req, bool update)
    {
	req->busId = id;
	return bus->probe(req, update);
    }

    /**
     * Probe the attached memory for the given request.
     * @param req The request to probe.
     * @param update If true, update the hierarchy.
     * @return The estimated completion time.
     */
    virtual Tick probe(MemReqPtr &req, bool update);
    
    /**
     * Mark this interface as blocked.
     */
    virtual void setBlocked()
    {
	if (!isBlocked()) {
	    blocked = true;
	    bus->setBlocked(id);
	}
    }

    /**
     * Mark this interface as unblocked.
     */
    virtual void clearBlocked()
    {
	if (isBlocked()) {
	    blocked = false;
	    bus->clearBlocked(id);
	}
    }

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
    
    InterfaceType getInterfaceType(){
        return BUS;
    }
    
    void setCurrentRequestAddr(Addr address){
        fatal("BusInterface: setCurrentRequestAddr is not implemented");
    }
};

#endif //__BUS_INT_HH__
