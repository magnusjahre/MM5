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
 * Describes a tagged prefetcher based on template policies.
 */

#ifndef __MEM_CACHE_PREFETCH_TAGGED_PREFETCHER_HH__
#define __MEM_CACHE_PREFETCH_TAGGED_PREFETCHER_HH__

#include "base/misc.hh" // fatal, panic, and warn

#include "mem/cache/prefetch/prefetcher.hh"

/**
 * A template-policy based cache. The behavior of the cache can be altered by
 * supplying different template policies. TagStore handles all tag and data
 * storage @sa TagStore. Buffering handles all misses and writes/writebacks
 * @sa MissQueue. Coherence handles all coherence policy details @sa
 * UniCoherence, SimpleMultiCoherence.
 */
template <class TagStore, class Buffering>
class TaggedPrefetcher : public Prefetcher<TagStore, Buffering>
{
  protected:

    Buffering* mq;
    TagStore* tags;
    
    Tick latency;
    int degree;

  public:

    TaggedPrefetcher(int size, bool pageStop, bool serialSquash,
		     bool cacheCheckPush, bool onlyData, 
		     Tick latency, int degree)
	:Prefetcher<TagStore, Buffering>(size, pageStop, serialSquash, 
					 cacheCheckPush, onlyData), 
	 latency(latency), degree(degree)
    {
    }

    ~TaggedPrefetcher() {}
    
    void calculatePrefetch(MemReqPtr &req, std::list<Addr> &addresses, 
			   std::list<Tick> &delays)
    {
	Addr blkAddr = req->paddr & ~(Addr)(this->blkSize-1);

	for (int d=1; d <= degree; d++) {
	    Addr newAddr = blkAddr + d*(this->blkSize);
	    if (this->pageStop && 
		(blkAddr & ~(TheISA::VMPageSize - 1)) !=
		(newAddr & ~(TheISA::VMPageSize - 1)))
	    {
		//Spanned the page, so now stop
		this->pfSpanPage += degree - d + 1;
		return;
	    }
	    else
	    {
		addresses.push_back(newAddr);
		delays.push_back(latency);
	    }
	}
    }
};

#endif // __MEM_CACHE_PREFETCH_TAGGED_PREFETCHER_HH__
