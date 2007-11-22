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

/** @file
 * Declaration of a unidirectional bus bridge.
 */
#include <list>
#include <vector>

#include "base/hashmap.hh"
#include "mem/mem_cmd.hh" // MemAccessResult
#include "mem/mem_req.hh"
#include "mem/base_hier.hh"
#include "mem/bus/base_interface.hh"

#include "base/range.hh"


/**
 * A bi-directional bus bridge that buffers requests from one bus to another.
 */
class BusBridge : public BaseHier
{
  private:
    /** The slave bus interface. */
    BaseInterface *si;
    
    /** The master bus interface. */
    BaseInterface *mi;

    /** The buffer latency. */
    int latency;
   
    /** The maximum number of buffered requests in each direction. */
    const int max;

    /** The width in bytes of the slave bus. */
    const int inWidth;
    /** The clock rate of the slave bus. */
    const int inClock;
    /** The width in bytes of the master bus. */
    const int outWidth;
    /** The clock rate of the master bus. */
    const int outClock;
    /** Should this bus bridge ack writes. */
    const bool ackWrites;
    /** The delay of any acked writes. */
    const int ackDelay;
    
    /** Internal Buffering for slave requests. */
    MemReqList slaveBuffer;
    /** The times each slave request is ready. */
    std::list<Tick> slaveTimes;
    /** The number of slave requests being buffered. */
    int slaveCount;
    /** Blocked flag for slave to master path. */
    bool slaveBlocked;

    /** Internal Buffering for master requests. */
    MemReqList masterBuffer;
    /** The times each master request is ready. */
    std::list<Tick> masterTimes;
    /** The number of master requests being buffered. */
    int masterCount;
    /** Blocked flag for master to slave path. */
    bool masterBlocked;

    /** 
     * A hashmap mapping a request pointer to in original bus id.
     * @todo Assumes 32 bit pointer.
     */
    m5::hash_map<intptr_t, int> requests;
    
  public:
    /**
     * Collection of parameters for this BusBridge.
     */
    class Params
    {
      public:
	/** Pointer to hierarchy wide parameters. */
	HierParams *hier;
	/** The delay to transmit a request from one bus to the other. */
	int latency;
	/** The maximum number of requests that can be buffered. */
	int max;
	/** The width in bytes of the slave bus. */
	int inWidth;
	/** The clock rate of the slave bus. */
	int inClock;
	/** The width in bytes of the master bus. */
	int outWidth;
	/** The clock rate of the master bus. */
	int outClock;
        /** Should this bridge ack any writes it recieves? */
        bool ackWrites;
        /** If this bridge is acking writes, what is the delay */
        int ackDelay;
    };
    
    /**
     * Construct a BusBridge.
     * @param name The name of this bridge.
     * @param params The paramters for this BusBridge.
     */
    BusBridge(const std::string &name, Params params);

    /**
     * Set the slave and master interfaces for this BusBridge. This snoops
     * both busses to set the address ranges that this bridge is responsible
     * for.
     * @param slave The new slave interface.
     * @param master The new master interface.
     */
    void setInterfaces(BaseInterface *slave, BaseInterface *master)
    {
	si = slave;
	mi = master;
	slaveRangeChange();
	masterRangeChange();
    }
    
    /**
     * Returns true if the slave to master buffer is full.
     */
    bool isSlaveBlocked()
    {
	return slaveBlocked;
    }

    /**
     * Returns true if the master to slave buffer is full.
     */
    bool isMasterBlocked()
    {
	return masterBlocked;
    }
    
    /**
     * Forwards the request from slave to master bus.
     * @param req The request to perform.
     * @return The result of the access.
     */
    MemAccessResult slaveAccess(MemReqPtr &req); 
    
    /**
     * Forwards the request from master to slave bus.
     * @param req The request to perform.
     * @return The result of the access.
     */
    MemAccessResult masterAccess(MemReqPtr &req); 

    /**
     * Forward a probe to the master bus.
     * @param req The request being probed.
     * @param update Update the hierarchy state.
     * @return The estimated completion time to this point.
     *
     * @warning Assumes that the request does not hit in the buffered requests.
     * @todo Figure out what to do with hits in buffered requests.
     */
    Tick slaveProbe(MemReqPtr &req, bool update)
    {
	return mi->sendProbe(req, update);
    }   

    /**
     * Forward a probe to the slave bus.
     * @param req The request being probed.
     * @param update Update the hierarchy state.
     * @return The estimated completion time to this point.
     *
     * @warning Assumes that the request does not hit in the buffered requests.
     * @todo Figure out what to do with hits in buffered requests.
     */
    Tick masterProbe(MemReqPtr &req, bool update)
    {
	return si->sendProbe(req, update);
    }
    
    /**
     * Return the next request to forward to the slave bus.
     * @return The coherence message to forward.
     */
    MemReqPtr getSlaveReq()
    {
	assert(!masterBuffer.empty());
	return masterBuffer.front();
    }
    
    /**
     * Return true if the slave bus should be requested.
     * @return True if there are outstanding requests for the slave bus.
     */
    bool doSlaveRequest()
    {
	if (!masterBuffer.empty()) {
	    return masterTimes.front() <= curTick;
	}
	return false;
    }

    /**
     * Return the next request to forward to the master bus.
     * @return The request to forward.
     */
    MemReqPtr getMasterReq()
    {
	assert(!slaveBuffer.empty());
	return slaveBuffer.front();
    }

    /**
     * Signal if the request was succesfully sent to the slave bus.
     * @param req The request.
     * @param success True if the request was sent successfully.
     */
    void sendSlaveResult(MemReqPtr &req, bool success); 
    
    /**
     * Signal if the request was succesfully sent to the master bus.
     * @param req The request.
     * @param success True if the request was sent successfully.
     */
    void sendMasterResult(MemReqPtr &req, bool success); 
    
    /**
     * Return true if the master bus should be requested.
     * @return True if there are outstanding requests for the master bus.
     */
    bool doMasterRequest()
    {
	if (!slaveBuffer.empty()) {
	    return slaveTimes.front() <= curTick;
	}
	return false;
    }

    /**
     * Forward a response to the master bus from the slave bus.
     * @param req The request being responded to.
     * @todo does this work with critical word first??
     */
    void handleSlaveResponse(MemReqPtr &req);

    /**
     * Forward a response to the slave bus from the master bus.
     * @param req The request being responded to.
     * @todo does this work with critical word first??
     */
    void handleMasterResponse(MemReqPtr &req);
    
    /**
     * Notification from master interface that a address range changed. Get 
     * the new address range list from the master interface and reset the 
     * slave range.
     */
    void masterRangeChange()
    {
	std::list<Range<Addr> > range_list;
	mi->collectRanges(range_list);
	si->setAddrRange(range_list);
    }
    
    /**
     * Notification from the slave interface that an address range changed.
     * Get the new address range list from the master interface and reset the
     * master range.
     */
    void slaveRangeChange()
    {
	std::list<Range<Addr> > range_list;
	si->collectRanges(range_list);
	mi->setAddrRange(range_list);
    }
};
