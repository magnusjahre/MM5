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
 * Definitions of LRU tag store.
 */

#include <string>

#include "mem/cache/base_cache.hh"
#include "base/intmath.hh"
#include "mem/cache/tags/lru.hh"
#include "sim/root.hh"

using namespace std;

LRUBlk*
CacheSet::findBlk(int asid, Addr tag) const
{
    for (int i = 0; i < assoc; ++i) {
	if (blks[i]->tag == tag && blks[i]->isValid()) {
	    return blks[i];
	}
    }
    return 0;
}


void
CacheSet::moveToHead(LRUBlk *blk)
{
    // nothing to do if blk is already head
    if (blks[0] == blk)
	return;

    // write 'next' block into blks[i], moving up from MRU toward LRU
    // until we overwrite the block we moved to head.

    // start by setting up to write 'blk' into blks[0]
    int i = 0;
    LRUBlk *next = blk;

    do {
	assert(i < assoc);
	// swap blks[i] and next
	LRUBlk *tmp = blks[i];
	blks[i] = next;
	next = tmp;
	++i;
    } while (next != blk);
}

/* New address layout with banked caches (Magnus):
 MSB                                                     LSB
|----------------------------------------------------------|
| Tag         | Set index        | Bank | Block offset     |
|----------------------------------------------------------|
*/

// create and initialize a LRU/MRU cache structure
//block size is configured in bytes
LRU::LRU(int _numSets, int _blkSize, int _assoc, int _hit_latency, int _bank_count) :
    numSets(_numSets), blkSize(_blkSize), assoc(_assoc), hitLatency(_hit_latency),numBanks(_bank_count)
{
    
    // block size is converted to bits in cache_builder.cc and can be used directly
    
    // Check parameters
    if (blkSize < 4 || ((blkSize & (blkSize - 1)) != 0)) {
	fatal("Block size must be at least 4 and a power of 2");
    }
    if (numSets <= 0 || ((numSets & (numSets - 1)) != 0)) {
	fatal("# of sets must be non-zero and a power of 2");
    }
    if (assoc <= 0) {
	fatal("associativity must be greater than zero");
    }
    if (hitLatency <= 0) {
	fatal("access latency must be greater than zero");
    }

    LRUBlk  *blk;
    int i, j, blkIndex;
    
    blkMask = (blkSize) - 1;
    
    if(numBanks != -1) setShift = FloorLog2(blkSize) + FloorLog2(numBanks);
    else setShift = FloorLog2(blkSize);
    setMask = numSets - 1;
    tagShift = setShift + FloorLog2(numSets);
    warmedUp = false;
    /** @todo Make warmup percentage a parameter. */
    warmupBound = numSets * assoc;
    
    sets = new CacheSet[numSets];
    blks = new LRUBlk[numSets * assoc];
    // allocate data storage in one big chunk
    dataBlks = new uint8_t[numSets*assoc*blkSize];

    blkIndex = 0;	// index into blks array
    for (i = 0; i < numSets; ++i) {
	sets[i].assoc = assoc;

	sets[i].blks = new LRUBlk*[assoc];

	// link in the data blocks
	for (j = 0; j < assoc; ++j) {
	    // locate next cache block
	    blk = &blks[blkIndex];
	    blk->data = &dataBlks[blkSize*blkIndex];
	    ++blkIndex;

	    // invalidate new cache block
	    blk->status = 0;

	    //EGH Fix Me : do we need to initialize blk?

	    // Setting the tag to j is just to prevent long chains in the hash
	    // table; won't matter because the block is invalid
	    blk->tag = j;
	    blk->whenReady = 0;
	    blk->asid = -1;
	    blk->isTouched = false;
	    blk->size = blkSize;
	    sets[i].blks[j]=blk;
	    blk->set = i;
	}
    }
}

LRU::~LRU()
{
    delete [] dataBlks;
    delete [] blks;
    delete [] sets;
}

// probe cache for presence of given block.
bool
LRU::probe(int asid, Addr addr) const
{
    //  return(findBlock(Read, addr, asid) != 0);
    Addr tag = extractTag(addr);
    unsigned myset = extractSet(addr);

    LRUBlk *blk = sets[myset].findBlk(asid, tag);

    return (blk != NULL);	// true if in cache
}

LRUBlk*
LRU::findBlock(Addr addr, int asid, int &lat)
{
    Addr tag = extractTag(addr);
    unsigned set = extractSet(addr);
    LRUBlk *blk = sets[set].findBlk(asid, tag);
    lat = hitLatency;
    if (blk != NULL) {
	// move this block to head of the MRU list
	sets[set].moveToHead(blk);
        
        if(cache->useUniformPartitioning){
            DPRINTF(UniformPartitioning, "Set %d: Hit in block (1), retrieved by processor %d, replaced block addr is %x\n",
                    set,
                    blk->origRequestingCpuID,
                    regenerateBlkAddr(blk->tag,blk->set));
        }
        
	if (blk->whenReady > curTick
	    && blk->whenReady - curTick > hitLatency) {
	    lat = blk->whenReady - curTick;
	}
	blk->refCount += 1;
    }

    return blk;
}

LRUBlk*
LRU::findBlock(MemReqPtr &req, int &lat)
{
    Addr addr = req->paddr;
    int asid = req->asid;

    Addr tag = extractTag(addr);
    unsigned set = extractSet(addr);
    LRUBlk *blk = sets[set].findBlk(asid, tag);
    lat = hitLatency;
    if (blk != NULL) {
	// move this block to head of the MRU list
	sets[set].moveToHead(blk);
        
        if(cache->useUniformPartitioning){
            DPRINTF(UniformPartitioning, "Set %d: Hit in block (2), retrieved by processor %d, replaced block addr is %x\n",
                    set,
                    blk->origRequestingCpuID,
                    regenerateBlkAddr(blk->tag,blk->set));
        }
        
	if (blk->whenReady > curTick
	    && blk->whenReady - curTick > hitLatency) {
	    lat = blk->whenReady - curTick;
	}
	blk->refCount += 1;
    }

    return blk;
}

LRUBlk*
LRU::findBlock(Addr addr, int asid) const
{
    Addr tag = extractTag(addr);
    unsigned set = extractSet(addr);
    LRUBlk *blk = sets[set].findBlk(asid, tag);
    return blk;
}

LRUBlk*
LRU::findReplacement(MemReqPtr &req, MemReqList &writebacks,
		     BlkList &compress_blocks)
{
    unsigned set = extractSet(req->paddr);
    
    // grab a replacement candidate
    LRUBlk *blk;
    if(cache->useUniformPartitioning){
        
        int fromProc = req->adaptiveMHASenderID;
        
        // we know that assoc is a power of two, checked in the constructor
        int maxBlks = (int) ((double) assoc / (double) cache->cpuCount);
        assert(maxBlks > 1);
        
        int* blkCnt = new int[cache->cpuCount];
        for(int i=0;i<cache->cpuCount;i++) blkCnt[i] = 0;
        for(int i=0;i<assoc;i++){
            int tmpID = sets[set].blks[i]->origRequestingCpuID;
            if(tmpID >= 0) blkCnt[tmpID]++;
        }
        
        ostringstream tmp;
        tmp << "Set " << set <<":";
        for(int i=0;i<cache->cpuCount;i++){
            tmp << " p" << i << "=" << blkCnt[i];
            assert(blkCnt[i] <= maxBlks);
        }
        DPRINTF(UniformPartitioning, "%s\n", tmp.str().c_str());
        
        
        bool found = false;
        if(blkCnt[fromProc] < maxBlks){
            
            // not using all blocks, LRU block is not touched
            DPRINTF(UniformPartitioning, "Set %d: Choosing block that has not been touched for replacement, request addr is %x, req from proc %d\n", 
                    set,
                    req->paddr, 
                    fromProc);
            blk = sets[set].blks[assoc-1];
            found = true;
            
            // if the block is touched, one processor has more than its share of cache blocks
            if(blk->isTouched){
                // evict the lru block for the processor with the most cache blocks
                fatal("not implemented");
            }
            
        }
        else{
            // replace the LRU block belonging to this cache
            for(int i = assoc-1;i>=0;i--){
                blk = sets[set].blks[i];
                if(blk->origRequestingCpuID == fromProc){
                    DPRINTF(UniformPartitioning, "Set %d: Replacing block belonging to processor %d, req by proc %d, request addr is %x, replaced block addr is %x\n",
                            set,
                            blk->origRequestingCpuID,
                            fromProc,
                            req->paddr,
                            regenerateBlkAddr(blk->tag,blk->set));
                    found = true;
                    break;
                }
            }
        }
        assert(found);
        
        delete blkCnt;
    }
    else{
        blk = sets[set].blks[assoc-1];
    }
    assert(blk != NULL);
    
    sets[set].moveToHead(blk);
    if (blk->isValid()) {
	int thread_num = (blk->xc) ? blk->xc->thread_num : 0;
	replacements[thread_num]++;
	totalRefs += blk->refCount;
	++sampledRefs;
	blk->refCount = 0;
    } else if (!blk->isTouched) {
	tagsInUse++;
	blk->isTouched = true;
	if (!warmedUp && tagsInUse.value() >= warmupBound) {
	    warmedUp = true;
	    warmupCycle = curTick;
	}
    }

    return blk;
}

void
LRU::invalidateBlk(int asid, Addr addr)
{
    LRUBlk *blk = findBlock(addr, asid);
    if (blk) {
	blk->status = 0;
	blk->isTouched = false;
	tagsInUse--;
    }
}

void
LRU::doCopy(Addr source, Addr dest, int asid, MemReqList &writebacks)
{
    assert(source == blkAlign(source));
    assert(dest == blkAlign(dest));
    LRUBlk *source_blk = findBlock(source, asid);
    assert(source_blk);
    LRUBlk *dest_blk = findBlock(dest, asid);
    if (dest_blk == NULL) {
	// Need to do a replacement
	MemReqPtr req = new MemReq();
	req->paddr = dest;
	BlkList dummy_list;
	dest_blk = findReplacement(req, writebacks, dummy_list);
	if (dest_blk->isValid() && dest_blk->isModified()) {
	    // Need to writeback data.
	    req = buildWritebackReq(regenerateBlkAddr(dest_blk->tag, 
						      dest_blk->set), 
				    dest_blk->asid,
				    dest_blk->xc,
				    blkSize,
				    (cache->doData())?dest_blk->data:0,
				    dest_blk->size);
	    writebacks.push_back(req);
	}
	dest_blk->tag = extractTag(dest);
	dest_blk->asid = asid;
	/**
	 * @todo Do we need to pass in the execution context, or can we 
	 * assume its the same?
	 */
	assert(source_blk->xc);
	dest_blk->xc = source_blk->xc;
    }
    /**
     * @todo Can't assume the status once we have coherence on copies.
     */
    
    // Set this block as readable, writeable, and dirty.
    dest_blk->status = 7;
    if (cache->doData()) {
	memcpy(dest_blk->data, source_blk->data, blkSize);
    }
}

void
LRU::cleanupRefs()
{
    for (int i = 0; i < numSets*assoc; ++i) {
	if (blks[i].isValid()) {
	    totalRefs += blks[i].refCount;
	    ++sampledRefs;
	}
    }
}


std::vector<int>
LRU::perCoreOccupancy(){
    vector<int> ret(cache->cpuCount, 0);
    int notTouched = 0;
    
    for(int i=0;i<numSets;i++){
        for(int j=0;j<assoc;j++){
            LRUBlk* blk = sets[i].blks[j];
            assert(blk->origRequestingCpuID < cache->cpuCount);
            if(blk->origRequestingCpuID != -1) ret[blk->origRequestingCpuID]++;
            else{
                assert(!blk->isTouched);
                notTouched++;
            }
        }
    }
    
    ret.push_back(notTouched);
    ret.push_back(numSets * assoc);
    
    int sum = 0;
    for(int i=0;i<cache->cpuCount+1;i++) sum += ret[i];
    assert(sum == numSets * assoc);
    
    return ret;
}
