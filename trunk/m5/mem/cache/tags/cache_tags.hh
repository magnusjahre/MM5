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

/** @file
 * Declarations of the Cache TagStore template policy.
 */

#ifndef __CACHE_TAGS_HH__
#define __CACHE_TAGS_HH__

#include "base/statistics.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/mem_req.hh"
#include "cpu/base.hh"

class BaseCache;
class MSHR;
class BasePrefetcher;

/**
 * A TagStore Cache policy to handle tag and data storage and access. This is
 * a template class with Tags and Compression policies. Tags handles the
 * storage of tags and data. Compression compresses/uncompresses data as
 * necessary.
 */
template <class Tags, class Compression>
class CacheTags
{
    
  public:
    /** A typedef for the block type to use. */
    typedef typename Tags::BlkType BlkType;
    /** A typedef for a list of BlkType pointers. */
    typedef typename Tags::BlkList BlkList;
  protected:
    // Params
    /**
     * Policy class for managing tags and data.
     */
    Tags *ct;
    /**
     * Policy class for performing compression.
     */
    Compression compress;

    BasePrefetcher *prefetcher;

    /**
     * The latency of a compression operation.
     */
    const int compLatency;
    /**
     * Can this cache should allocate a block on a line-sized write miss.
     */
    const bool doFastWrites;
    /**
     * Can the data can be stored in a compressed form.
     */
    const bool storeCompressed;

    /**
     * Should we use an adaptive compression scheme.
     */
    const bool adaptiveCompression;

    const bool prefetchMiss;

    /**
     * The block size of this cache. Set to value in the Tags object.
     */
    const int blkSize;

    /**
     * Pointer to the parent cache.
     */
    BaseCache *cache;
    /**
     * The width in bytes of the master bus. Used for calculating critical
     * word first delays.
     */
    int busWidth;
    /**
     * The clock ratio of bus cycles to processor cycles. Used for calculating
     * critical word first delays.
     */
    int busRatio;

    /**
     * Local copy of the cache name for DPRINTFs
     */
    std::string objName;

    /**
     * Compare the internal block data to the fast access block data.
     * @param blk The cache block to check.
     * @return True if the data is the same.
     */
    bool verifyData(BlkType *blk);

    /**
     * Update the internal data of the block. The data to write is assumed to
     * be in the fast access data.
     * @param blk The block with the data to update.
     * @param writebacks A list to store any generated writebacks.
     * @param compress_block True if we should compress this block
     */
    void updateData(BlkType *blk, MemReqList &writebacks, bool compress_block);

    /**
     * Handle a replacement for the given request.
     * @param blk A pointer to the block, usually NULL
     * @param req The memory request to satisfy.
     * @param new_state The new state of the block.
     * @param writebacks A list to store any generated writebacks.
     */
    BlkType* doReplacement(BlkType *blk,
                           MemReqPtr &req, 
                           CacheBlk::State new_state,
                           MemReqList &writebacks);

  public:
    /**
     * Constructor.
     * @param _ct The Tags object to use.
     * @param comp_latency The number of cycles to compress a cache line.
     * @param do_fast_writes Perform fast writes?
     * @param store_compressed Store data in a compressed form.
     * @param adaptive_compression Use an apative compression scheme.
     */
    CacheTags(Tags *_ct, int comp_latency, bool do_fast_writes,
	      bool store_compressed, bool adaptive_compression,
	      bool prefeetch_miss);

    /**
     * Register stats for this object and the Tags policy.
     * @param name The name of the cache.
     */
    void regStats(const std::string &name);

    /**
     * Set the parent cache and the master bus parameters.
     * @param _cache A pointer to the parent cache.
     * @param bus_width The width in bytes of the master bus.
     * @param bus_ratio The number of cycles in a bus cycle.
     */
    void setCache(BaseCache *_cache, int bus_width, int bus_ratio);

    void setPrefetcher(BasePrefetcher *_prefetcher);

    /**
     * The name of the parent cache. Used by DPRINTF.
     * @return The name of the cache.
     */
    const std::string &name() const
    {
	return objName;
    }

    /**
     * Finds the cache block for the given request.
     * @param addr The address to find.
     * @param asid The address space id.
     * @return A pointer to the cache block, NULL if not found.
     */
    BlkType* findBlock(Addr addr, int asid)
    {
	return ct->findBlock(addr, asid);
    }
    
    /**
     * Finds the cache block for the given request.
     * @param req The request.
     * @return A pointer to the cache block, NULL if not found.
     */
    BlkType* findBlock(MemReqPtr &req)
    {
	return ct->findBlock(req->paddr, req->asid);
    }

    /**
     * Invalidates the cache block for the provided address.
     * @param addr The address to look for.
     * @param asid The address space id.
     * @todo Do I need this anymore?
     */
    void invalidateBlk(Addr addr, int asid)
    {
	ct->invalidateBlk(asid, addr);
    }

    void invalidateBlk(CacheBlk *blk)
    {
	ct->invalidateBlk(blk->asid, ct->regenerateBlkAddr(blk->tag, blk->set));
    }

    /**
     * Does all the processing necessary to perform the provided request.
     * @param req The memory request to perform.
     * @param lat The latency of the access.
     * @param writebacks List for any writebacks that need to be performed.
     * @param update True if the replacement data should be updated.
     * @return Pointer to the cache block touched by the request. NULL if it
     * was a miss.
     */
    BlkType* handleAccess(MemReqPtr &req, int & lat, 
			   MemReqList & writebacks, bool update = true);

    /**
     * Populates a cache block and handles all outstanding requests for the
     * satisfied fill request. This version takes an MSHR pointer and uses its
     * request to fill the cache block, while repsonding to its targets.
     * @param blk The cache block if it already exists.
     * @param mshr The MSHR that contains the fill data and targets to satisfy.
     * @param new_state The state of the new cache block.
     * @param writebacks List for any writebacks that need to be performed.
     * @return Pointer to the new cache block.
     */
    BlkType* handleFill(BlkType *blk,
                        MSHR * mshr,
                        CacheBlk::State new_state,
                        MemReqList & writebacks);

    /**
     * Populates a cache block and handles all outstanding requests for the
     * satisfied fill request. This version takes two memory requests. One 
     * contains the fill data, the other is an optional target to satisfy.
     * Used for Cache::probe.
     * @param blk The cache block if it already exists.
     * @param req The memory request with the fill data.
     * @param new_state The state of the new cache block.
     * @param writebacks List for any writebacks that need to be performed.
     * @param target The memory request to perform after the fill.
     * @return Pointer to the new cache block.
     */
    BlkType* handleFill(BlkType *blk, MemReqPtr &req, 
			CacheBlk::State new_state,
			MemReqList & writebacks, MemReqPtr target = NULL);

    /**
     * Populates a cache block and instantly handles all outstanding requests 
     * for the satisfied fill request. This version takes an MSHR pointer and
     * uses its request to fill the cache block, while repsonding to its
     * targets.
     * @param mshr The MSHR that contains the fill data and targets to satisfy.
     * @param new_state The state of the new cache block.
     * @param writebacks List for any writebacks that need to be performed.
     * @return Pointer to the new cache block.
     */
    BlkType* pseudoFill(MSHR *mshr,
                        CacheBlk::State new_state,
			MemReqList &writebacks);

    /**
     * Handle any outstanding requests contained in the the given MSHR.
     * @param mshr The mshr that contains the targets.
     * @param writebacks A list for any writeback requests.
     * @return A pointer to the block used to respond to the requests.
     */
    BlkType* handleTargets(MSHR *mshr, MemReqList &writebacks);

    /**
     * Sets the blk to the new state and handles the given request.
     * @param blk The cache block being snooped.
     * @param new_state The new coherence state for the block.
     * @param req The request to satisfy
     */
    void handleSnoop(BlkType *blk, CacheBlk::State new_state, 
		     MemReqPtr &req);
    
    /**
     * Sets the blk to the new state.
     * @param blk The cache block being snooped.
     * @param new_state The new coherence state for the block.
     */
    void handleSnoop(BlkType *blk, CacheBlk::State new_state);

    /**
     * Perform a aligned cache block copy from the source to the destination.
     * @param source The block aligned source address.
     * @param dest The block aligned destination address.
     * @param asid The address space ID.
     * @param writebacks A list for any writeback requests.
     */
    void doCopy(Addr source, Addr dest, int asid, MemReqList &writebacks)
    {
	ct->doCopy(source, dest, asid, writebacks);
    }
    
    /**
     * Perform a cache block copy when at least one address is not aligned.
     * @param source The source address.
     * @param dest The destination address.
     * @param asid The address space ID.
     * @param writebacks A list for any writeback requests.
     */
    void doUnalignedCopy(Addr source, Addr dest, int asid,
			 MemReqList &writebacks);

    /**
     * Create a writeback request for the given block.
     * @param blk The block to writeback.
     * @return The writeback request for the block.
     */
    MemReqPtr writebackBlk(BlkType *blk);  
};


#endif //__CACHE_TAGS_HH__
