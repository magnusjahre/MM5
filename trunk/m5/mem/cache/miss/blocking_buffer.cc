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
 * Definitions of a simple buffer for a blocking cache.
 */

#include "cpu/exec_context.hh"
#include "cpu/smt.hh" //for maxThreadsPerCPU
#include "mem/cache/base_cache.hh"
#include "mem/cache/miss/blocking_buffer.hh"
#include "mem/cache/prefetch/base_prefetcher.hh"
#include "sim/eventq.hh" // for Event declaration.

/**
 * @todo Move writebacks into shared BaseBuffer class.
 */
void
BlockingBuffer::regStats(const std::string &name)
{
    using namespace Stats;
    writebacks
	.init(maxThreadsPerCPU)
	.name(name + ".writebacks")
	.desc("number of writebacks")
	.flags(total)
	;
}

void
BlockingBuffer::setCache(BaseCache *_cache)
{
    cache = _cache;
    blkSize = cache->getBlockSize();
}

void 
BlockingBuffer::setPrefetcher(BasePrefetcher *_prefetcher)
{
    prefetcher = _prefetcher;
}
void
BlockingBuffer::handleMiss(MemReqPtr &req, int blk_size, Tick time)
{
    Addr blk_addr = req->paddr & ~(Addr)(blk_size - 1);
    if (req->cmd.isWrite() && (req->isUncacheable() || !writeAllocate ||
			       req->cmd.isNoResponse())) {
	if (req->cmd.isNoResponse()) {
	    wb.allocateAsBuffer(req);
	} else {
	    wb.allocate(req->cmd, blk_addr, req->asid, blk_size, req);
	}
	if (cache->doData()) {
	    memcpy(wb.req->data, req->data, blk_size);
	}
	cache->setBlocked(Blocked_NoWBBuffers);
	cache->setMasterRequest(Request_WB, time);
	return;
    }

    if (req->cmd.isNoResponse()) {
	miss.allocateAsBuffer(req);
    } else {
	miss.allocate(req->cmd, blk_addr, req->asid, blk_size, req);
    }
    if (!req->isUncacheable()) {
	miss.req->flags |= CACHE_LINE_FILL;
    }
    cache->setBlocked(Blocked_NoMSHRs);
    cache->setMasterRequest(Request_MSHR, time);
}

MemReqPtr
BlockingBuffer::getMemReq()
{
    if (miss.req && !miss.inService) {
	return miss.req;
    }
    return wb.req;
}

void
BlockingBuffer::setBusCmd(MemReqPtr &req, MemCmd cmd)
{
    MSHR *mshr = req->mshr;
    mshr->originalCmd = req->cmd;
    if (req->isCacheFill())
	req->cmd = cmd;
}

void
BlockingBuffer::restoreOrigCmd(MemReqPtr &req)
{
    req->cmd = req->mshr->originalCmd;
}

void
BlockingBuffer::markInService(MemReqPtr &req)
{
    if (!req->isCacheFill() && req->cmd.isWrite()) {
	// Forwarding a write/ writeback, don't need to change
	// the command
	assert(req->mshr == &wb);
	cache->clearMasterRequest(Request_WB);
	if (req->cmd.isNoResponse()) {
	    assert(wb.getNumTargets() == 0);
	    wb.deallocate();
	    cache->clearBlocked(Blocked_NoWBBuffers);
	} else {
	    wb.inService = true;
	}
    } else {
	assert(req->mshr == &miss);
	cache->clearMasterRequest(Request_MSHR);
	if (req->cmd.isNoResponse()) {
	    assert(miss.getNumTargets() == 0);
	    miss.deallocate();
	    cache->clearBlocked(Blocked_NoMSHRs);
	} else {
	    //mark in service
	    miss.inService = true;
	}
    }
}

void
BlockingBuffer::handleResponse(MemReqPtr &req, Tick time)
{
    if (req->isCacheFill()) {
	// targets were handled in the cache tags
	assert(req->mshr == &miss);
	miss.deallocate();
	cache->clearBlocked(Blocked_NoMSHRs);
    } else {
	if (req->mshr->hasTargets()) {
	    // Should only have 1 target if we had any
	    assert(req->mshr->getNumTargets() == 1);
	    MemReqPtr target = req->mshr->getTarget();
	    req->mshr->popTarget();
	    if (cache->doData() && req->cmd.isRead()) {
		memcpy(target->data, req->data, target->size);
	    }
	    cache->respond(target, time);
	    assert(!req->mshr->hasTargets());
	}

	if (req->cmd.isWrite()) {
	    assert(req->mshr == &wb);
	    wb.deallocate();
	    cache->clearBlocked(Blocked_NoWBBuffers);
	} else {
	    miss.deallocate();
	    cache->clearBlocked(Blocked_NoMSHRs);
	}
    }
}

void
BlockingBuffer::squash(int thread_number)
{
    if (miss.threadNum == thread_number) {
	MemReqPtr target = miss.getTarget();
	miss.popTarget();
	assert(target->thread_num == thread_number);
	if (target->completionEvent != NULL) {
	    delete target->completionEvent;
	}
	target = NULL;
	assert(!miss.hasTargets());
	miss.ntargets=0;
	if (!miss.inService) {
	    miss.deallocate();
	    cache->clearBlocked(Blocked_NoMSHRs);
	    cache->clearMasterRequest(Request_MSHR);
	}
    }
}

void
BlockingBuffer::doWriteback(Addr addr, int asid, ExecContext *xc,
			    int size, uint8_t *data, bool compressed)
{

    // Generate request
    MemReqPtr req = new MemReq();
    req->paddr = addr;
    req->asid = asid;
    req->size = size;
    req->data = new uint8_t[size];
    if (data) {
	memcpy(req->data, data, size);
    }
    /**
     * @todo Need to find a way to charge the writeback to the "correct"
     * thread.
     */
    req->xc = xc;
    if (xc)
	req->thread_num = xc->thread_num;
    else
	req->thread_num = 0;

    req->cmd = Writeback;
    if (compressed) {
	req->flags |= COMPRESSED;
    }

    writebacks[req->thread_num]++;

    wb.allocateAsBuffer(req);
    cache->setMasterRequest(Request_WB, curTick);
    cache->setBlocked(Blocked_NoWBBuffers);
}



void
BlockingBuffer::doWriteback(MemReqPtr &req)
{
    writebacks[req->thread_num]++;

    wb.allocateAsBuffer(req);

    // Since allocate as buffer copies the request,
    // need to copy data here.
    if (cache->doData()) {
	memcpy(wb.req->data, req->data, req->size);
    }
    cache->setBlocked(Blocked_NoWBBuffers);
    cache->setMasterRequest(Request_WB, curTick);
}
