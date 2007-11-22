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
 * Declaration of a general memory interface.
 */

#ifndef __MEMORY_INTERFACE__
#define __MEMORY_INTERFACE__

#include <string>

#include "mem/mem_interface.hh"

// Forward Declaration
class MemTraceWriter;

/**
 * Templatized direct interface to memory.
 */
template<class Mem>
class MemoryInterface : public MemInterface
{
  
  private:
    std::string thisName;
    bool issuedWarning;
    
  protected:
    /** The connected Memory. */
    Mem *mem;

    /** A memory trace writer. */
    MemTraceWriter *memTrace;
    
  public:

    /**
     * Construct this interface.
     * @param name The name of this interface.
     * @param hier Pointer to the hierarchy wide parameters.
     * @param _mem the connected memory.
     * @param mem_trace A memory trace to write to.
     */
    MemoryInterface(const std::string &name, HierParams *hier, Mem *_mem,
		    MemTraceWriter *mem_trace);

    /**
     * Access the connected memory to perform the given request.
     * @param req The request to service.
     * @return the result of the request.
     */
    virtual MemAccessResult access(MemReqPtr &req);

    /**
     * Satisfy the given request. If called on a multilevel hierarchy, the
     * request is forward until satisfied with no timing impact.
     * @param req The request to perform.
     */
    virtual void probe(MemReqPtr &req) const
    {
	assert(0);
    }

    /**
     * Squash all outstanding requests for the given thread.
     * @param thread_number The thread to squash.
     */
    virtual void squash(int thread_number);

    virtual void snoopResponseCall(MemReqPtr &req)
    {
	fatal("Not Implemented\n");
    }

     /**
     * Called by the memory to respond to requests.
     * @param req The request being responded to.
     * @param time The time the response is ready.
     */
    virtual void respond(MemReqPtr &req, Tick time);

    /**
     * Return the number of outstanding misses in a Cache.
     * @return The number of missing still outstanding.
     */
    virtual int outstandingMisses()
    {
	return mem->outstandingMisses();
    }

    /**
     * Probe the hierarchy for the given req. Don't update the state of the 
     * hierarchy. If the request is a write, update the data in every cached
     * location, but don't change the dirty bits.
     * @param req The request to probe.
     * @return The estimated completion time.
     */
    virtual Tick probe(MemReqPtr &req)
    {
	return mem->probe(req,false);
    }

    /**
     * Perform a access in the cache in zero time. Updates all state, including
     * caching the touched blocks, and performing protocol updates.
     * @param req The request to perform
     * @return The estimated completion time.
     */
    virtual Tick probeAndUpdate(MemReqPtr &req)
    {
	return mem->probe(req,true);
    }

    virtual void getRange(std::list<Range<Addr> > &range_list)
    {
	range_list.push_back(RangeIn(0,MaxAddr));
    }
    
};

#endif
