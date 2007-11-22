/*
 * Copyright (c) 2005
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
 * Hardware Prefetcher Definition.
 */

#include "base/trace.hh"
#include "cpu/exec_context.hh"
#include "mem/cache/base_cache.hh"
#include "mem/cache/miss/miss_queue.hh"
#include "mem/cache/prefetch/base_prefetcher.hh"
#include "mem/cache/tags/cache_tags.hh"
#include "cpu/exec_context.hh"
#include <list>

BasePrefetcher::BasePrefetcher(int size, bool pageStop, bool serialSquash,
			       bool cacheCheckPush, bool onlyData)
    :size(size), pageStop(pageStop), serialSquash(serialSquash),
     cacheCheckPush(cacheCheckPush), only_data(onlyData)
{
}

void 
BasePrefetcher::setCache(BaseCache *_cache)
{
    cache = _cache;
    blkSize = cache->getBlockSize();
}

void 
BasePrefetcher::regStats(const std::string &name)
{
    pfIdentified
	.name(name + ".prefetcher.num_hwpf_identified")
	.desc("number of hwpf identified")
	;

    pfMSHRHit
	.name(name + ".prefetcher.num_hwpf_already_in_mshr")
	.desc("number of hwpf that were already in mshr")
	;
    
    pfCacheHit
	.name(name + ".prefetcher.num_hwpf_already_in_cache")
	.desc("number of hwpf that were already in the cache")
	;
    
    pfBufferHit
	.name(name + ".prefetcher.num_hwpf_already_in_prefetcher")
	.desc("number of hwpf that were already in the prefetch queue")
	;

    pfRemovedFull
	.name(name + ".prefetcher.num_hwpf_evicted")
	.desc("number of hwpf removed due to no buffer left")
	;
    
    pfRemovedMSHR
	.name(name + ".prefetcher.num_hwpf_removed_MSHR_hit")
	.desc("number of hwpf removed because MSHR allocated")
	;

    pfIssued
	.name(name + ".prefetcher.num_hwpf_issued")
	.desc("number of hwpf issued")
	;
    
    pfSpanPage
	.name(name + ".prefetcher.num_hwpf_span_page")
	.desc("number of hwpf spanning a virtual page")
	;

    pfSquashed
	.name(name + ".prefetcher.num_hwpf_squashed_from_miss")
	.desc("number of hwpf that got squashed due to a miss aborting calculation time")
	;
   
}

MemReqPtr 
BasePrefetcher::getMemReq()
{
    DPRINTF(HWPrefetch, "%s:Requesting a hw_pf to issue\n", cache->name());

    if (pf.empty()) {
	DPRINTF(HWPrefetch, "%s:No HW_PF found\n", cache->name());
	return NULL;
    }

    MemReqPtr req;
    bool keepTrying = false;
    do {
	req = *pf.begin();
	pf.pop_front();
	if (!cacheCheckPush) {
	    keepTrying = inCache(req);
	}
	if (pf.empty()) {
	    cache->clearMasterRequest(Request_PF);
	    if (keepTrying) return NULL; //None left, all were in cache 
	}
    } while (keepTrying);

    pfIssued++;
      req->prefetched = 1;
    return req;
}

void
BasePrefetcher::handleMiss(MemReqPtr &req, Tick time)
{
    if (!req->isUncacheable() && !(req->isInstRead() && only_data))
    {
	//Calculate the blk address
	Addr blkAddr = req->paddr & ~(Addr)(blkSize-1);

	//Check if miss is in pfq, if so remove it
	std::list<MemReqPtr>::iterator iter = inPrefetch(blkAddr);
	if (iter != pf.end()) {
	    DPRINTF(HWPrefetch, "%s:Saw a miss to a queued prefetch, removing it\n", cache->name());
	    pfRemovedMSHR++;
	    pf.erase(iter);
	    if (pf.empty()) 
		cache->clearMasterRequest(Request_PF);
	}
	
	//Remove anything in queue with delay older than time
	//since everything is inserted in time order, start from end 
	//and work until pf.empty() or time is earlier
	//This is done to emulate Aborting the previous work on a new miss
	//Needed for serial calculators like GHB
	if (serialSquash) {
	    iter = pf.end();
	    iter--;
	    while (!pf.empty() && ((*iter)->time >= time)) {
		pfSquashed++;
		pf.pop_back();
		iter--;
	    } 
	    if (pf.empty())
		cache->clearMasterRequest(Request_PF);
	}
	

	std::list<Addr> addresses;
	std::list<Tick> delays;
	calculatePrefetch(req, addresses, delays);
	
	std::list<Addr>::iterator addr = addresses.begin();
	std::list<Tick>::iterator delay = delays.begin();
	while (addr != addresses.end())
	{
	    DPRINTF(HWPrefetch, "%s:Found a pf canidate, inserting into prefetch queue\n", cache->name());
	    //temp calc this here...
	    pfIdentified++;
	    //create a prefetch memreq
	    MemReqPtr prefetch;
	    prefetch = new MemReq();
	    prefetch->paddr = (*addr);
	    prefetch->size = blkSize;
	    prefetch->cmd = Hard_Prefetch;
	    prefetch->xc = req->xc;
	    prefetch->data = new uint8_t[blkSize];
	    prefetch->asid = req->asid;
	    prefetch->thread_num = req->thread_num;
	    prefetch->time = time + (*delay); //@todo ADD LATENCY HERE
	    //... initialize
	    
	    //Check if it is already in the cache 
	    if (cacheCheckPush) {
		if (inCache(prefetch)) {
		    addr++;
		    delay++;
		    continue;
		}
	    }

	    //Check if it is already in the miss_queue
	    if (inMissQueue(prefetch->paddr, prefetch->asid)) {
		addr++;
		delay++;
		continue;
	    }

	    //Check if it is already in the pf buffer
	    if (inPrefetch(prefetch->paddr) != pf.end()) {
		pfBufferHit++;
		addr++;
		delay++;
		continue;
	    }
	    
	    //We just remove the head if we are full
	    if (pf.size() == size)
	    {
		DPRINTF(HWPrefetch, "%s:Inserting into prefetch queue, it was full removing oldest\n", cache->name());
		pfRemovedFull++;
		pf.pop_front();
	    }
	    
	    pf.push_back(prefetch);
	    prefetch->flags |= CACHE_LINE_FILL;
	    
	    //Make sure to request the bus, with proper delay
	    cache->setMasterRequest(Request_PF, prefetch->time);

	    //Increment through the list
	    addr++;
	    delay++;
	}
    }
}

std::list<MemReqPtr>::iterator
BasePrefetcher::inPrefetch(Addr address)
{
    //Guaranteed to only be one match, we always check before inserting
    std::list<MemReqPtr>::iterator iter;
    for (iter=pf.begin(); iter != pf.end(); iter++) {
	if (((*iter)->paddr & ~(Addr)(blkSize-1)) == address) {
	    return iter;
	}
    }
    return pf.end();
}


