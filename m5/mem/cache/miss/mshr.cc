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
 * Miss Status and Handling Register (MSHR) definitions.
 */

#include <assert.h>
#include <string>
#include <vector>

#include "mem/cache/miss/mshr.hh"
#include "sim/root.hh" // for curTick
#include "sim/host.hh"
#include "base/misc.hh"

using namespace std;

MSHR::MSHR()
{
    inService = false;
    ntargets = 0;
    threadNum = -1;
    mlpCost = 0;
    mlpCostDistribution.clear();
    allocatedAt = 0;
    mshrID = -1;

//     cache = NULL;
}

void
MSHR::allocate(MemCmd cmd, Addr _addr, int _asid, int size,
		MemReqPtr &target)
{

	assert(targets.empty());
	addr = _addr;
	asid = _asid;

	allocatedAt = curTick;
	mlpCost = 0;
	mlpCostDistribution.clear();

	req = new MemReq(); // allocate new memory request
	req->completionEvent = 0; // Don't delete twice!
	req->expectCompletionEvent = false;
	req->paddr = addr; //picked physical address for now
	req->asid = asid;
	req->cmd = cmd;
	req->size = size;
	req->data = new uint8_t[size];
	req->mshr = this;
	//Set the time here for latency calculations
	req->time = curTick;

	if (target) {

		threadNum = target->thread_num;
		req->nic_req = target->nic_req;
		req->xc = target->xc;
		allocateTarget(target);
		req->thread_num = target->thread_num;
		req->flags = target->flags;
		req->pc = target->pc;

		req->oldAddr = target->oldAddr;
		req->enteredMemSysAt = target->enteredMemSysAt;
		req->writebackGeneratedAt = target->writebackGeneratedAt;

		// make sure owner ID is copied to the new request
		// necessary for the AdaptiveMHA
		req->adaptiveMHASenderID = target->adaptiveMHASenderID;
		req->interferenceAccurateSenderID = target->interferenceAccurateSenderID;
		req->interferenceMissAt = target->interferenceMissAt;
		req->isShadowMiss = target->isShadowMiss;

		req->finishedInCacheAt = target->finishedInCacheAt;

		req->memCtrlGeneratingReadSeqNum = target->memCtrlGeneratingReadSeqNum;
		req->memCtrlGenReadInterference = target->memCtrlGenReadInterference;
		req->memCtrlWbGenBy = target->memCtrlWbGenBy;

		req->isSWPrefetch = target->isSWPrefetch;
		req->nfqWBID = target->nfqWBID;

		req->isSharedWB = target->isSharedWB;

		req->isStore = target->isStore;
		req->beenInSharedMemSys = target->beenInSharedMemSys;

		if(cache->isShared){
			for(int i=0;i<MEM_REQ_LATENCY_BREAKDOWN_SIZE;i++) req->interferenceBreakdown[i] += target->interferenceBreakdown[i];
			for(int i=0;i<MEM_REQ_LATENCY_BREAKDOWN_SIZE;i++) req->latencyBreakdown[i] += target->latencyBreakdown[i];
		}
	}
}

// Since we aren't sure if data is being used, don't copy here.
/**
 * @todo When we have a "global" data flag, might want to copy data here.
 */
void
MSHR::allocateAsBuffer(MemReqPtr &target)
{
    addr = target->paddr;
    asid = target->asid;
    threadNum = target->thread_num;
    req = new MemReq();
    req->paddr = target->paddr;
    req->dest = target->dest;
    req->asid = target->asid;
    req->cmd = target->cmd;
    req->size = target->size;
    req->flags = target->flags;
    req->xc = target->xc;
    req->thread_num = target->thread_num;
    req->data = new uint8_t[target->size];
    req->mshr = this;
    req->time = curTick;
    req->pc = target->pc;

    req->oldAddr = target->oldAddr;
    req->writebackGeneratedAt = target->writebackGeneratedAt;

    req->enteredMemSysAt = target->enteredMemSysAt;
    req->adaptiveMHASenderID = target->adaptiveMHASenderID;
    req->interferenceAccurateSenderID = target->interferenceAccurateSenderID;
    req->interferenceMissAt = target->interferenceMissAt;
    req->isShadowMiss = target->isShadowMiss;
    req->finishedInCacheAt = target->finishedInCacheAt;

    req->memCtrlGeneratingReadSeqNum = target->memCtrlGeneratingReadSeqNum;
    req->memCtrlGenReadInterference = target->memCtrlGenReadInterference;
    req->memCtrlWbGenBy = target->memCtrlWbGenBy;

    if(cache->isShared){
        for(int i=0;i<MEM_REQ_LATENCY_BREAKDOWN_SIZE;i++) req->interferenceBreakdown[i] += target->interferenceBreakdown[i];
        for(int i=0;i<MEM_REQ_LATENCY_BREAKDOWN_SIZE;i++) req->latencyBreakdown[i] += target->latencyBreakdown[i];
    }

    req->isSWPrefetch = req->isSWPrefetch;
    req->nfqWBID = target->nfqWBID;
    req->isSharedWB = target->isSharedWB;

    req->isStore = target->isStore;
    req->beenInSharedMemSys = target->beenInSharedMemSys;

    allocatedAt = curTick;
    mlpCost = 0;
    mlpCostDistribution.clear();
}

void
MSHR::deallocate()
{

    assert(targets.empty());
    assert(ntargets == 0);
    req = NULL;
    inService = false;
//     allocIter = NULL;
//     readyIter = NULL;
}

/*
 * Adds a target to an MSHR
 */
void
MSHR::allocateTarget(MemReqPtr &target)
{

    //If we append an invalidate and we issued a read to the bus,
    //but now have some pending writes, we need to move
    //the invalidate to before the first non-read
    if (inService && req->cmd.isRead() && target->cmd.isInvalidate()) {
	std::deque<MemReqPtr> temp;

	while (targets.size() > 0) {
	    if (!targets.front()->cmd.isRead()) break;
	    //Place on top of temp stack
	    temp.push_front(targets.front());
	    //Remove from targets
	    targets.pop_front();
	}

	//Now that we have all the reads off until first non-read, we can
	//place the invalidate on
	targets.push_front(target);

	//Now we pop off the temp_stack and put them back
	while (temp.size() > 0) {
	    targets.push_front(temp.front());
	    temp.pop_front();
	}
    }
    else {
	targets.push_back(target);
    }

    ++ntargets;
    assert(targets.size() == ntargets);
    /**
     * @todo really prioritize the target commands.
     */

    if (!inService && target->cmd.isWrite()) {
    	req->cmd = Write;
    }
}



void
MSHR::dump()
{
    ccprintf(cerr,
	     "inService: %d thread: %d\n"
	     "Addr: %x asid: %d ntargets %d\n"
	     "Targets:\n",
	     inService, threadNum, addr, asid, ntargets);
    for (int i = 0; i < ntargets; i++) {
	ccprintf(cerr, "\t%d: Addr: %x cmd: %d\n",
		 i, targets[i]->paddr, targets[i]->cmd.toIndex());
    }
    ccprintf(cerr, "\n");
}

MSHR::~MSHR()
{
    if (req)
	req = NULL;
}

void
MSHR::setCache(BaseCache* _cache){
    cache = _cache;
}
