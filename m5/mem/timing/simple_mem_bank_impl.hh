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
 * Definition of a simple main memory.
 */

#include <sstream>
#include <string>
#include <vector>

#include "config/full_system.hh"
#include "cpu/base.hh"
#include "cpu/exec_context.hh" // for reading from memory
#include "cpu/smt.hh"
#include "mem/bus/bus.hh"
#include "mem/bus/bus_interface.hh"
#include "mem/bus/slave_interface.hh"
#include "mem/functional/functional.hh"
#include "mem/memory_interface.hh"
#include "mem/mem_req.hh"
#include "mem/timing/simple_mem_bank.hh"
#include "sim/builder.hh"

using namespace std;

template <class Compression>
SimpleMemBank<Compression>::SimpleMemBank(const string &name, HierParams *hier,
					  BaseMemory::Params params)
    : BaseMemory(name, hier, params)
{
}

template <class Compression>
void
SimpleMemBank<Compression>::regStats()
{
    using namespace Stats;
    BaseMemory::regStats();

    bytesRequested
	.init(maxThreadsPerCPU)
	.name(name() + ".bytes_requested")
	.desc("total number of bytes requested")
	.flags(total)
	;

    bytesSent
	.init(maxThreadsPerCPU)
	.name(name() + ".bytes_sent")
	.desc("total number of bytes sent")
	.flags(total)
	;

    compressedAccesses
	.init(maxThreadsPerCPU)
	.name(name() + ".compressed_responses")
	.desc("total number of accesses that are compressed")
	.flags(total)
	;
    
}

/* Handle memory bank access latencies */
template <class Compression>
MemAccessResult
SimpleMemBank<Compression>::access(MemReqPtr &req)
{
    Tick response_time;
    
#if !FULL_SYSTEM
    // For full system, there is just one funcMem that is a
    // data member of the BaseMemory object.  For syscall
    // emulation, the functional memory is per thread, so we
    // have to get it from the request.
    assert(req->xc != NULL);
    FunctionalMemory *funcMem = req->xc->mem;
#endif

    assert(req->thread_num < SMT_MAX_THREADS);
    accesses[req->thread_num]++;

    if (!req->isUncacheable()) {
	response_time = hitLatency + curTick;
    } else {
	response_time = uncacheLatency + curTick;
    }

    if (!req->isSatisfied()) {
	req->flags |= SATISFIED;
	if (doData()) {
	    // doing real data transfers: read or write data on
	    // functional memory object
	    /**
	     * @todo
	     * Need to remove this dummy untranslation once we unify the
	     * memory objects.
	     */
	    Addr addr = (req->paddr & ULL(1) << 47) ?
		(req->paddr | ULL(0xff) << 48) :
		(req->paddr & ((ULL(1) << 48) - ULL(1)));

	    if (req->cmd.isRead()) {
		bytesRequested[req->thread_num] += req->size;
		uint8_t tmp_data[req->size];
		int data_size = req->size;
		funcMem->access(Read, addr, req->data,
					  data_size);
		if (!(req->flags & UNCACHEABLE)){
		    // Compress data to ship
		    data_size = compress.compress(tmp_data, req->data, 
						  data_size);
		    if (data_size < req->size) {
			req->flags |= COMPRESSED;
			req->actualSize = req->size;
			req->size = data_size;
		    }
		}
		bytesSent[req->thread_num] += req->size;
	    }
	    else if (req->cmd.isWrite() && doWrites) {
		if (req->isCompressed()) {
		    funcMem->access(Write, addr, req->data,
					 req->actualSize);
		} else {
		    funcMem->access(Write, addr, req->data,
					 req->size);
		}   
	    }
	}
	if (!(req->cmd.isInvalidate() && !req->cmd.isRead()
		&& !req->cmd.isWrite())) {
	    //No response on upgrade/invalidates
	    si->respond(req, response_time);
	}
    }
    else if (snarfUpdates) {
	// Memory request has been satisfied by another device (i.e.,
	// a cache-to-cache transfer).  Update memory value from
	// request.  This line should be removed if an ownerhsip-based
	// coherence protocol is used.
	if (doData() && doWrites && (req->cmd.isRead() || req->cmd.isWrite())){
	    Addr addr = (req->paddr & ULL(1) << 47) ?
	    	(req->paddr | ULL(0xff) << 48) :
		(req->paddr & ((ULL(1) << 48) - ULL(1)));
	    if (req->isCompressed()) {
		funcMem->access(Write, addr, req->data,
				     req->actualSize);
	    } else {
		funcMem->access(Write, addr, req->data,
				     req->size);
	    }   
	}
    }
    if (req->flags & COMPRESSED) {
	compressedAccesses[req->thread_num]++;
    }
    return MA_HIT;
}

template <class Compression>
Tick
SimpleMemBank<Compression>::probe(MemReqPtr &req, bool update)
{
    Tick response_time;

#if !FULL_SYSTEM
    // For full system, there is just one funcMem that is a
    // data member of the BaseMemory object.  For syscall
    // emulation, the functional memory is per thread, so we
    // have to get it from the request.
    assert(req->xc != NULL);
    FunctionalMemory *funcMem = req->xc->mem;
#endif

    if (update) {
	accesses[req->thread_num]++;
    }

    if (!req->isUncacheable()) {
	response_time = hitLatency + curTick;
    } else {
	response_time = uncacheLatency + curTick;
    }

    if (!req->isSatisfied()) {
	if (doData()) {
	    // doing real data transfers: read or write data on
	    // functional memory object
	    /**
	     * @todo
	     * Need to remove this dummy untranslation once we unify the
	     * memory objects.
	     */
	    Addr addr = (req->paddr & ULL(1) << 47) ?
		(req->paddr | ULL(0xff) << 48) :
		(req->paddr & ((ULL(1) << 48) - ULL(1)));


	    if (req->cmd.isRead()) {
		funcMem->access(Read, addr, req->data, req->size);
	    }
	    else if (req->cmd.isWrite() && doWrites) {
		if (req->isCompressed()) {
		    funcMem->access(Write, addr, req->data, 
					 req->actualSize);
		} else {
		    funcMem->access(Write, addr, req->data, req->size);
		}
	    }
	}
	req->flags |= SATISFIED;
    }
    else if (snarfUpdates) {
	// Memory request has been satisfied by another device (i.e.,
	// a cache-to-cache transfer).  Update memory value from
	// request.  This line should be removed if an ownerhsip-based
	// coherence protocol is used.
	if (doData() && doWrites && (req->cmd.isRead() || req->cmd.isWrite())){
	    Addr addr = (req->paddr & ULL(1) << 47) ?
	    	(req->paddr | ULL(0xff) << 48) :
		(req->paddr & ((ULL(1) << 48) - ULL(1)));
	    if (req->isCompressed()) {
		funcMem->access(Write, addr, req->data, req->actualSize);
	    } else {
		funcMem->access(Write, addr, req->data, req->size);
	    }
	}
    }
    return response_time;
}
