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
 * Definitions a fully associative LRU tagstore.
 */

#include <sstream>

#include <assert.h>

#include "mem/cache/tags/fa_lru.hh"
#include "base/intmath.hh"

using namespace std;

FALRU::FALRU(int _blkSize, int _size, int hit_latency)
    : blkSize(_blkSize), size(_size),
      numBlks(size/blkSize), hitLatency(hit_latency)
{
    if ((blkSize & (blkSize - 1)) != 0)
	fatal("cache block size (in bytes) `%d' must be a power of two",
	      blkSize);
    if (!(hitLatency > 0))
	fatal("Access latency in cycles must be at least one cycle");
    if ((size & (size - 1)) !=0)
	fatal("Cache Size must be power of 2 for now");

    // Track all cache sizes from 128K up by powers of 2
    numCaches = FloorLog2(size) - 17;
    if (numCaches >0){
	cacheBoundaries = new FALRUBlk *[numCaches];
	cacheMask = (1 << numCaches) - 1;
    } else {
	cacheMask = 0;
    }
    
    warmedUp = false;
    warmupBound = size/blkSize;

    blks = new FALRUBlk[numBlks];
    head = &(blks[0]);
    tail = &(blks[numBlks-1]);

    head->prev = NULL;
    head->next = &(blks[1]);
    head->inCache = cacheMask;

    tail->prev = &(blks[numBlks-2]);
    tail->next = NULL;
    tail->inCache = 0;

    int index = (1 << 17) / blkSize;
    int j = 0;
    int flags = cacheMask;
    for (int i = 1; i < numBlks-1; i++) {
	blks[i].inCache = flags;
	if (i == index - 1){
	    cacheBoundaries[j] = &(blks[i]);
	    flags &= ~ (1<<j);
	    ++j;
	    index = index << 1;
  	}
	blks[i].prev = &(blks[i-1]);
	blks[i].next = &(blks[i+1]);
	blks[i].isTouched = false;
    }
    assert(j == numCaches);
    assert(index == numBlks);
    //assert(check());
}

void
FALRU::regStats(const string &name)
{
    using namespace Stats;
    BaseTags::regStats(name);
    hits
	.init(numCaches+1)
	.name(name + ".falru_hits")
	.desc("The number of hits in each cache size.")
	;
    misses
	.init(numCaches+1)
	.name(name + ".falru_misses")
	.desc("The number of misses in each cache size.")
	;
    accesses
	.name(name + ".falru_accesses")
	.desc("The number of accesses to the FA LRU cache.")
	;
    
    for (int i = 0; i < numCaches+1; ++i) {
	stringstream size_str;
	if (i < 3){
	    size_str << (1<<(i+7)) <<"K";
	} else {
	    size_str << (1<<(i-3)) <<"M";
	}
	    
	hits.subname(i, size_str.str());
	hits.subdesc(i, "Hits in a " + size_str.str() +" cache");
	misses.subname(i, size_str.str());
	misses.subdesc(i, "Misses in a " + size_str.str() +" cache");
    }
}

FALRUBlk *
FALRU::hashLookup(Addr addr) const
{
    tagIterator iter = tagHash.find(addr);
    if (iter != tagHash.end()) {
	return (*iter).second;
    }
    return NULL;
}

bool
FALRU::probe(int asid, Addr addr) const
{
    Addr blkAddr = blkAlign(addr);
    FALRUBlk* blk = hashLookup(blkAddr);
    return blk && blk->tag == blkAddr && blk->isValid();
}

void
FALRU::invalidateBlk(int asid, Addr addr)
{
    Addr blkAddr = blkAlign(addr);
    FALRUBlk* blk = (*tagHash.find(blkAddr)).second;
    if (blk) {
	assert(blk->tag == blkAddr);
	blk->status = 0;
	blk->isTouched = false;
	tagsInUse--;
    }
}

FALRUBlk*
FALRU::findBlock(Addr addr, int asid, int &lat, int *inCache)
{
    accesses++;
    int tmp_in_cache = 0;
    Addr blkAddr = blkAlign(addr);
    FALRUBlk* blk = hashLookup(blkAddr);

    if (blk && blk->isValid()) {
	assert(blk->tag == blkAddr);
	tmp_in_cache = blk->inCache;
	for (int i = 0; i < numCaches; i++) {
	    if (1<<i & blk->inCache) {
		hits[i]++;
	    } else {
		misses[i]++;
	    }
	}
	hits[numCaches]++;
	if (blk != head){
	    moveToHead(blk);
	}
    } else {
	blk = NULL;
	for (int i = 0; i < numCaches+1; ++i) {
	    misses[i]++;
	}
    }
    if (inCache) {
	*inCache = tmp_in_cache;
    }

    lat = hitLatency;
    //assert(check());
    return blk;
}

FALRUBlk*
FALRU::findBlock(MemReqPtr &req, int &lat, int *inCache)
{
    Addr addr = req->paddr;

    accesses++;
    int tmp_in_cache = 0;
    Addr blkAddr = blkAlign(addr);
    FALRUBlk* blk = hashLookup(blkAddr);

    if (blk && blk->isValid()) {
	assert(blk->tag == blkAddr);
	tmp_in_cache = blk->inCache;
	for (int i = 0; i < numCaches; i++) {
	    if (1<<i & blk->inCache) {
		hits[i]++;
	    } else {
		misses[i]++;
	    }
	}
	hits[numCaches]++;
	if (blk != head){
	    moveToHead(blk);
	}
    } else {
	blk = NULL;
	for (int i = 0; i < numCaches+1; ++i) {
	    misses[i]++;
	}
    }
    if (inCache) {
	*inCache = tmp_in_cache;
    }

    lat = hitLatency;
    //assert(check());
    return blk;
}

FALRUBlk*
FALRU::findBlock(Addr addr, int asid) const
{
    Addr blkAddr = blkAlign(addr);
    FALRUBlk* blk = hashLookup(blkAddr);

    if (blk && blk->isValid()) {
	assert(blk->tag == blkAddr);
    } else {
	blk = NULL;
    }
    return blk;
}

FALRUBlk*
FALRU::findReplacement(MemReqPtr &req, MemReqList &writebacks, 
		       BlkList &compress_blocks)
{
    FALRUBlk * blk = tail;
    assert(blk->inCache == 0);
    moveToHead(blk);
    tagHash.erase(blk->tag);
    tagHash[blkAlign(req->paddr)] = blk;
    if (blk->isValid()) {
	int thread_num = (blk->xc) ? blk->xc->thread_num : 0;
	replacements[thread_num]++;
    } else {
	tagsInUse++;
	blk->isTouched = true;
	if (!warmedUp && tagsInUse.value() >= warmupBound) {
	    warmedUp = true;
	    warmupCycle = curTick;
	}
    }
    //assert(check());
    return blk;
}

void
FALRU::moveToHead(FALRUBlk *blk)
{
    int updateMask = blk->inCache ^ cacheMask;
    for (int i = 0; i < numCaches; i++){
 	if ((1<<i) & updateMask) {
 	    cacheBoundaries[i]->inCache &= ~(1<<i);
 	    cacheBoundaries[i] = cacheBoundaries[i]->prev;
 	} else if (cacheBoundaries[i] == blk) {
	    cacheBoundaries[i] = blk->prev;
	}
    }
    blk->inCache = cacheMask;
    if (blk != head) {
	if (blk == tail){
	    assert(blk->next == NULL);
	    tail = blk->prev;
	    tail->next = NULL;
	} else {
	    blk->prev->next = blk->next;
	    blk->next->prev = blk->prev;
	}
	blk->next = head;
	blk->prev = NULL;
	head->prev = blk;
	head = blk;
    }
}

bool
FALRU::check()
{
    FALRUBlk* blk = head;
    int size = 0;
    int boundary = 1<<17;
    int j = 0; 
    int flags = cacheMask;
    while (blk) {
	size += blkSize;
	if (blk->inCache != flags) {
	    return false;
	}
	if (size == boundary && blk != tail) {
	    if (cacheBoundaries[j] != blk) {
		return false;
	    }
	    flags &=~(1 << j);
	    boundary = boundary<<1;
	    ++j;
	}
	blk = blk->next;
    }
    return true;
}
