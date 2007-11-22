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
 * Declaration a base object for all hierarchy memories.
 */

#ifndef __MEM_BASE_MEM_HH__
#define __MEM_BASE_MEM_HH__

#include <vector>

#include "base/range.hh"
#include "mem/base_hier.hh"
#include "mem/crossbar/crossbar_interface.hh" // for CrossbarInterface
#include "targetarch/isa_traits.hh" // for Addr

#define CACHE_DEBUG 1

class BaseInterface;
class MemInterface;

/**
 * Very basic generic memory class. This class really exists solely to provide
 * the slave interface to the CPU. Since it exists, we place some common
 * variables here.
 */

class BaseMem : public BaseHier
{
  public:
    /*HACK: needed to do the memory address bug workaround */
    int cpuCount;
    bool isMultiprogWorkload;
    int cacheCpuID;
    
  protected:
    /** The hit latency of this memory. */
    int hitLatency;
    /** The slave interface of this memory. */
    BaseInterface *si;

    /**
     * The starting addresses for the address ranges this memory is responsible
     * for.
     */
    std::vector<Range<Addr> > addrRanges;

  public:
    /**
     * Create and initialize this memory.
     * @param name The name of this memory.
     * @param hier Pointer to the hierarchy wide parameters.
     * @param hit_latency The hit latency of this memory.
     * @param ranges The memory address ranges served by this memory.
     */
    BaseMem(const std::string &name, HierParams *hier, int hit_latency,
	    std::vector<Range<Addr> > ranges)
	: BaseHier(name, hier), hitLatency(hit_latency), addrRanges(ranges)
    {
    }

    /**
     * Set the slave interface for this memory to the one provided.
     * @param i The new slave interface.
     */
    void setSlaveInterface(BaseInterface *i);

    /**
     * Returns the slave interface of this memory.
     * @return The MemInterface of this memory.
     * @pre si is a subclass of MemInterface.
     */
    MemInterface* getInterface()
    {
	//if(si != NULL){
            return (MemInterface*)si;
        /*}
        else if(ci != NULL){
            return (MemInterface*)ci;
        }
        fatal("Attempting to return non-existent memory interface");
        return NULL;*/
    }

    /**
     * Returns the hit latency of this memory.
     * @return The hit latency of this memory.
     */
    int getHitLatency()
    {
	return hitLatency;
    }
    
    bool isCache() { return false; }
    
#ifdef CACHE_DEBUG
    virtual void removePendingRequest(Addr address, MemReqPtr& req) = 0;
    virtual void addPendingRequest(Addr address, MemReqPtr& req) = 0;
#endif
    
};

#endif //__BASE_MEM_HH__
