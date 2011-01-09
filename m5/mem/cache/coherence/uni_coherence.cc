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

#include "mem/cache/coherence/uni_coherence.hh"
#include "mem/cache/base_cache.hh"

#include "base/trace.hh"

using namespace std;

UniCoherence::UniCoherence()
    : cshrs(50, false, false)
{
}

MemReqPtr
UniCoherence::getMemReq()
{
    bool unblock = cshrs.isFull();
    MemReqPtr req = cshrs.getReq();
    cshrs.markInService(req->mshr);
    if (!cshrs.havePending()) {
	cache->clearSlaveRequest(Request_Coherence);
    }
    if (unblock) {
	//since CSHRs are always used as buffers, should always get rid of one
	assert(!cshrs.isFull());
	cache->clearBlocked(Blocked_Coherence);
    }
    return req;
}

/**
 * @todo add support for returning slave requests, not doing them here.
 */
bool
UniCoherence::handleBusRequest(MemReqPtr &req, CacheBlk *blk, MSHR *mshr,
			       CacheBlk::State &new_state)
{
    new_state = 0;
    if (req->cmd.isInvalidate()) {
	DPRINTF(Cache, "snoop inval on blk %x (blk ptr %x)\n",
		req->paddr, blk);
	if (!cache->isTopLevel()) {
	    // Forward to other caches
	    MemReqPtr tmp = new MemReq();
	    tmp->cmd = Invalidate;
	    tmp->paddr = req->paddr;
	    tmp->size = req->size;
	    cshrs.allocate(tmp);
	    cache->setSlaveRequest(Request_Coherence, curTick);
	    if (cshrs.isFull()) {
		cache->setBlockedForSnoop(Blocked_Coherence);
	    }
	}
    } else {
	if (blk) {
	    new_state = blk->status;
	}
    }
    return false;
}
