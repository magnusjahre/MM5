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

#ifndef __UNI_COHERENCE_HH__
#define __UNI_COHERENCE_HH__

#include "base/trace.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/miss/mshr_queue.hh"
#include "mem/mem_cmd.hh"
#include "mem/mem_req.hh"

class BaseCache;

class UniCoherence
{
  protected:
    /** Buffers to hold forwarded invalidates. */
    MSHRQueue cshrs;
    /** Pointer to the parent cache. */
    BaseCache *cache;

  public:
    /**
     * Construct and initialize this coherence policy.
     */
    UniCoherence();

    /**
     * Set the pointer to the parent cache.
     * @param _cache The parent cache.
     */
    void setCache(BaseCache *_cache)
    {
	cache = _cache;

        cshrs.setCache(_cache);
    }

    /**
     * Register statistics.
     * @param name The name to prepend to stat descriptions.
     */
    void regStats(const std::string &name)
    {
		cshrs.regStats(".coherence");
    }

    /**
     * Return Read.
     * @param cmd The request's command.
     * @param state The current state of the cache block.
     * @return The proper bus command, as determined by the protocol.
     * @todo Make changes so writebacks don't get here.
     */
    MemCmd getBusCmd(MemCmd &cmd, CacheBlk::State state)
    {
	if (cmd == Hard_Prefetch && state)
	    warn("Trying to issue a prefetch to a block we already have\n");
	if (cmd == Writeback)
	    return Writeback;
	return Read;
    }

    /**
     * Just return readable and writeable.
     * @param req The bus response.
     * @param current The current block state.
     * @return The new state.
     */
    CacheBlk::State getNewState(MemReqPtr &req, CacheBlk::State current)
    {
	if (req->mshr->originalCmd == Hard_Prefetch) {
	    DPRINTF(HWPrefetch, "Marking a hardware prefetch as such in the state\n");
	    return BlkHWPrefetched | BlkValid | BlkWritable;
	}
	else {
	    return BlkValid | BlkWritable;
	}
    }
    /**
     * Return outstanding invalidate to forward.
     * @return The next invalidate to forward to lower levels of cache.
     */
    MemReqPtr getMemReq();

    /**
     * Handle snooped bus requests.
     * @param req The snooped bus request.
     * @param blk The cache block corresponding to the request, if any.
     * @param mshr The MSHR corresponding to the request, if any.
     * @param new_state The new coherence state of the block.
     * @return True if the request should be satisfied locally.
     */
    bool handleBusRequest(MemReqPtr &req, CacheBlk *blk, MSHR *mshr,
			  CacheBlk::State &new_state);

    /**
     * Return true if this coherence policy can handle fast cache writes.
     */
    bool allowFastWrites() { return true; }

    bool hasProtocol() { return false; }
};

#endif //__UNI_COHERENCE_HH__
