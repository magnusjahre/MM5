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
 * Miss and writeback queue declarations.
 */

#ifndef __MEM_CACHE_PREFETCH_BASE_PREFETCHER_HH__
#define __MEM_CACHE_PREFETCH_BASE_PREFETCHER_HH__

#include "base/trace.hh"
#include "cpu/exec_context.hh"
#include "mem/cache/base_cache.hh"
#include "mem/cache/miss/miss_queue.hh"
#include "mem/cache/tags/cache_tags.hh"
#include "cpu/exec_context.hh"
#include <list>

class BasePrefetcher
{
  protected:
    /** The Prefetch Queue. */
    std::list<MemReqPtr> pf;

    // PARAMETERS

    /** The number of MSHRs in the Prefetch Queue. */
    const int size;

    /** Pointr to the parent cache. */
    BaseCache* cache;

    /** The block size of the parent cache. */
    int blkSize;

    /** Do we prefetch across page boundaries. */
    bool pageStop;

    /** Do we remove prefetches with later times than a new miss.*/
    bool serialSquash;

    /** Do we check if it is in the cache when inserting into buffer, 
	or removing.*/
    bool cacheCheckPush;

    /** Do we prefetch on only data reads, or on inst reads as well. */
    bool only_data;

  public:

    Stats::Scalar<> pfIdentified;
    Stats::Scalar<> pfMSHRHit;
    Stats::Scalar<> pfCacheHit;
    Stats::Scalar<> pfBufferHit;
    Stats::Scalar<> pfRemovedFull;
    Stats::Scalar<> pfRemovedMSHR;
    Stats::Scalar<> pfIssued;
    Stats::Scalar<> pfSpanPage;
    Stats::Scalar<> pfSquashed;

    void regStats(const std::string &name);

  public:
    BasePrefetcher(int numMSHRS, bool pageStop, bool serialSquash,
		   bool cacheCheckPush, bool onlyData);

    virtual ~BasePrefetcher() {}

    void setCache(BaseCache *_cache);

    void handleMiss(MemReqPtr &req, Tick time);

    MemReqPtr getMemReq();

    bool havePending()
    {
	return !pf.empty();    
    }

    virtual void calculatePrefetch(MemReqPtr &req, 
				   std::list<Addr> &addresses, 
				   std::list<Tick> &delays) = 0;

    virtual bool inCache(MemReqPtr &req) = 0;

    virtual bool inMissQueue(Addr address, int asid) = 0;

    std::list<MemReqPtr>::iterator inPrefetch(Addr address);
};


#endif //__MEM_CACHE_PREFETCH_BASE_PREFETCHER_HH__
