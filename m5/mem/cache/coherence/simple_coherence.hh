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
 * Declaration of a simple coherence policy.
 */

#ifndef __SIMPLE_COHERENCE_HH__
#define __SIMPLE_COHERENCE_HH__

#include <string>

#include "mem/mem_req.hh"
#include "mem/mem_cmd.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/miss/mshr_queue.hh"
#include "mem/cache/coherence/coherence_protocol.hh"

class BaseCache;

/**
 * A simple MP coherence policy. This policy assumes an atomic bus and only one
 * level of cache.
 */
class SimpleCoherence
{
  protected:
    /** Pointer to the parent cache. */
    BaseCache *cache;
    /** Pointer to the coherence protocol. */
    CoherenceProtocol *protocol;

  public:
    /**
     * Construct and initialize this coherence policy.
     * @param _protocol The coherence protocol to use.
     */
    SimpleCoherence(CoherenceProtocol *_protocol)
	: protocol(_protocol)
    {
    }

    /**
     * Set the pointer to the parent cache.
     * @param _cache The parent cache.
     */
    void setCache(BaseCache *_cache)
    {
	cache = _cache;
    }

    /**
     * Register statistics.
     * @param name The name to prepend to stat descriptions.
     */
    void regStats(const std::string &name)
    {
    }

    /**
     * This policy does not forward invalidates, return NULL.
     * @return NULL.
     */
    MemReqPtr getMemReq()
    {
	return NULL;
    }

    /**
     * Return the proper state given the current state and the bus response.
     * @param req The bus response.
     * @param current The current block state.
     * @return The new state.
     */
    CacheBlk::State getNewState(MemReqPtr &req, CacheBlk::State current)
    {
	return protocol->getNewState(req, current);
    }

    /**
     * Handle snooped bus requests.
     * @param req The snooped bus request.
     * @param blk The cache block corresponding to the request, if any.
     * @param mshr The MSHR corresponding to the request, if any.
     * @param new_state Return the new state for the block.
     */
    bool handleBusRequest(MemReqPtr &req, CacheBlk *blk, MSHR *mshr,
			  CacheBlk::State &new_state)
    {
//	assert(mshr == NULL);
//Got rid of, there could be an MSHR, but it can't be in service
	if (blk != NULL)
	{
	    if (req->cmd != Writeback) {
		return protocol->handleBusRequest(cache, req, blk, mshr, 
					      new_state);
	    }
	    else { //It is a writeback, must be ownership protocol, just keep state
		new_state = blk->status;
	    }
	}
	return false;
    }

    /**
     * Get the proper bus command for the given command and status.
     * @param cmd The request's command.
     * @param state The current state of the cache block.
     * @return The proper bus command, as determined by the protocol.
     */
    MemCmd getBusCmd(MemCmd &cmd, CacheBlk::State state)
    {
	if (cmd == Writeback) return Writeback;
	return protocol->getBusCmd(cmd, state);
    }

    /**
     * Return true if this coherence policy can handle fast cache writes.
     */
    bool allowFastWrites() { return false; }

    bool hasProtocol() { return true; }
};

#endif //__SIMPLE_COHERENCE_HH__








