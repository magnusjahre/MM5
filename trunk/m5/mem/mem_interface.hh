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
 * Declaration of a purely virtual base class for a direct interface to memory.
 */
#ifndef __MEM_INTERFACE_HH__
#define __MEM_INTERFACE_HH__

#include <string>

#include "base/trace.hh"
#include "mem/bus/bus_interface.hh"

/**
 * Base class that defines a direct interface to memory. Used to interface
 * between processor objects and memory objects.
 */
class MemInterface : public BaseInterface
{
  private:
    /** Local copy of the connected memory's hit latency. */
    const int hitLatency;
    /** Local copy of the connected memory's block size. */
    const int blkSize;

  public:
    /**
     * Set local constants.
     */
    MemInterface(const std::string &name, HierParams *hier,
		 int hit_latency, int block_size)
	: BaseInterface(name, hier),
	  hitLatency(hit_latency),
	  blkSize(block_size)
    {
    }

    /**
     * Access the connected memory to perform the given request.
     * @param req The request to service.
     * @return the result of the request.
     */
    virtual MemAccessResult access(MemReqPtr &req) = 0;

    /**
     * Satisfy the given request. If called on a multilevel hierarchy, the
     * request is forward until satisfied with no timing impact.
     * @param req The request to perform.
     */
    virtual void probe(MemReqPtr &req) const = 0;

    /**
     * Squash all outstanding requests for the given thread.
     * @param thread_number The thread to squash.
     */
    virtual void squash(int thread_number) = 0;

    /**
     * Called by the memory to respond to requests.
     * @param req The request being responded to.
     * @param time The time the response is ready.
     */
    virtual void respond(MemReqPtr &req, Tick time) = 0;

    /**
     * Return the hit latency of the memory.
     * @return the hit latency.
     */
    int getHitLatency() const
    {
	return hitLatency;
    }

    /**
     * Return the block size of the memory.
     * @return block size.
     */
    int getBlockSize() const
    {
        return blkSize;
    }

    /**
     * Return the number of outstanding misses in a Cache.
     * @return The number of missing still outstanding.
     */
    virtual int outstandingMisses() = 0;

    /**
     * Probe the hierarchy for the given req. Don't update the state of the 
     * hierarchy. If the request is a write, update the data in every cached
     * location, but don't change the dirty bits.
     * @param req The request to probe.
     * @return The estimated completion time.
     */
    virtual Tick probe(MemReqPtr &req) = 0;

    /**
     * Perform a access in the cache in zero time. Updates all state, including
     * caching the touched blocks, and performing protocol updates.
     * @param req The request to perform
     * @return The estimated completion time.
     */
    virtual Tick probeAndUpdate(MemReqPtr &req) = 0;

    virtual void setBlocked()
    {
	DPRINTF(Cache,"Blocking CPU->Mem interface\n");
	blocked = true;
    }

    virtual void clearBlocked()
    {
	DPRINTF(Cache,"Blocking CPU->Mem interface\n");
	blocked = false;
    }

    // Default implementations of unneeded functions
    virtual void request(Tick time)
    {
	fatal("No implementation");
    }
    
    virtual bool grantAddr()
    {
	fatal("No implementation");
    }

    virtual bool grantData()
    {
	fatal("No implementation");
    }

    virtual void snoop(MemReqPtr &req)
    {
	fatal("No implementation");
    }

    virtual void snoopResponse(MemReqPtr &req)
    {
	fatal("No implementation");
    }

    virtual Tick sendProbe(MemReqPtr &req, bool update)
    {
	fatal("No implementation");
    }

    virtual Tick probe(MemReqPtr &req, bool update)
    {
	fatal("No implementation");
    }

    virtual void collectRanges(std::list<Range<Addr> > &range_list)
    {
	fatal("No implementation");
    }

    virtual void rangeChange()
    {
	fatal("No implementation");
    }
    
    InterfaceType getInterfaceType(){
        return MEMORY;
    }
    
    void setCurrentRequestAddr(Addr address){
        fatal("MemInterface: setCurrentRequestAddr is not implemented");
    }
	
};

#endif
