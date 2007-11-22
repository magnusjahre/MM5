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
 * Declaration of a fully associative LRU tag store.
 */

#ifndef __FA_LRU_HH__
#define __FA_LRU_HH__

#include <list>

#include "mem/cache/cache_blk.hh"
#include "mem/mem_req.hh"
#include "base/hashmap.hh"
#include "mem/cache/tags/base_tags.hh"

/**
 * A fully associative cache block.
 */
class FALRUBlk : public CacheBlk
{
public:
    /** The previous block in LRU order. */
    FALRUBlk *prev;
    /** The next block in LRU order. */
    FALRUBlk *next;
    /** Has this block been touched? */
    bool isTouched;

    /**
     * A bit mask of the sizes of cache that this block is resident in.
     * Each bit represents a power of 2 in MB size cache.
     * If bit 0 is set, this block is in a 1MB cache
     * If bit 2 is set, this block is in a 4MB cache, etc.
     * There is one bit for each cache smaller than the full size (default
     * 16MB).
     */
    int inCache;
};

/**
 * A fully associative LRU cache. Keeps statistics for accesses to a number of
 * cache sizes at once.
 */
class FALRU : public BaseTags 
{
  public:
    /** Typedef the block type used in this class. */
    typedef FALRUBlk BlkType;
    /** Typedef a list of pointers to the local block type. */
    typedef std::list<FALRUBlk*> BlkList;
  protected:
    /** The block size of the cache. */
    const int blkSize;
    /** The size of the cache. */
    const int size;
    /** The number of blocks in the cache. */
    const int numBlks; // calculated internally
    /** The hit latency of the cache. */
    const int hitLatency;

    /** Array of pointers to blocks at the cache size  boundaries. */
    FALRUBlk **cacheBoundaries;
    /** A mask for the FALRUBlk::inCache bits. */
    int cacheMask;
    /** The number of different size caches being tracked. */
    int numCaches;

    /** The cache blocks. */
    FALRUBlk *blks;

    /** The MRU block. */
    FALRUBlk *head;
    /** The LRU block. */
    FALRUBlk *tail;

    /** Hash table type mapping addresses to cache block pointers. */
    typedef m5::hash_map<Addr, FALRUBlk *, m5::hash<Addr> > hash_t;
    /** Iterator into the address hash table. */
    typedef hash_t::const_iterator tagIterator;

    /** The address hash table. */
    hash_t tagHash;

    /**
     * Find the cache block for the given address.
     * @param addr The address to find.
     * @return The cache block of the address, if any.
     */
    FALRUBlk * hashLookup(Addr addr) const;

    /**
     * Move a cache block to the MRU position.
     * @param blk The block to promote.
     */
    void moveToHead(FALRUBlk *blk);

    /**
     * Check to make sure all the cache boundaries are still where they should
     * be. Used for debugging.
     * @return True if everything is correct.
     */
    bool check();

    /**
     * @defgroup FALRUStats Fully Associative LRU specific statistics
     * The FA lru stack lets us track multiple cache sizes at once. These
     * statistics track the hits and misses for different cache sizes.
     * @{
     */
    
    /** Hits in each cache size >= 128K. */
    Stats::Vector<> hits;
    /** Misses in each cache size >= 128K. */
    Stats::Vector<> misses;
    /** Total number of accesses. */
    Stats::Scalar<> accesses;

    /**
     * @}
     */

public:
    /**
     * Construct and initialize this cache tagstore.
     * @param blkSize The block size of the cache.
     * @param size The size of the cache.
     * @param hit_latency The hit latency of the cache.
     */
    FALRU(int blkSize, int size, int hit_latency);

    /**
     * Register the stats for this object.
     * @param name The name to prepend to the stats name.
     */
    void regStats(const std::string &name);
    
    /**
     * Return true if the address is found in the cache.
     * @param asid The address space ID.
     * @param addr The address to look for.
     * @return True if the address is in the cache.
     */
    bool probe(int asid, Addr addr) const;

    /**
     * Invalidate the cache block that contains the given addr.
     * @param asid The address space ID.
     * @param addr The address to invalidate.
     */
    void invalidateBlk(int asid, Addr addr);

    /**
     * Find the block in the cache and update the replacement data. Returns
     * the access latency and the in cache flags as a side effect
     * @param addr The address to look for.
     * @param asid The address space ID.
     * @param lat The latency of the access.
     * @param inCache The FALRUBlk::inCache flags.
     * @return Pointer to the cache block.
     */
    FALRUBlk* findBlock(Addr addr, int asid, int &lat, int *inCache = 0);

    /**
     * Find the block in the cache and update the replacement data. Returns
     * the access latency and the in cache flags as a side effect
     * @param req The req whose block to find
     * @param lat The latency of the access.
     * @param inCache The FALRUBlk::inCache flags.
     * @return Pointer to the cache block.
     */
    FALRUBlk* findBlock(MemReqPtr &req, int &lat, int *inCache = 0);

    /**
     * Find the block in the cache, do not update the replacement data.
     * @param addr The address to look for.
     * @param asid The address space ID.
     * @return Pointer to the cache block.
     */
    FALRUBlk* findBlock(Addr addr, int asid) const;

    /**
     * Find a replacement block for the address provided.
     * @param req The request to a find a replacement candidate for.
     * @param writebacks List for any writebacks to be performed.
     * @param compress_blocks List of blocks to compress, for adaptive comp.
     * @return The block to place the replacement in.
     */
    FALRUBlk* findReplacement(MemReqPtr &req, MemReqList & writebacks,
			      BlkList &compress_blocks);

    /**
     * Return the hit latency of this cache.
     * @return The hit latency.
     */
    int getHitLatency() const
    {
	return hitLatency;
    }

    /**
     * Return the block size of this cache.
     * @return The block size.
     */
    int getBlockSize()
    {
	return blkSize;
    }

    /**
     * Return the subblock size of this cache, always the block size.
     * @return The block size.
     */
    int getSubBlockSize()
    {
	return blkSize;
    }

    /**
     * Align an address to the block size.
     * @param addr the address to align.
     * @return The aligned address.
     */
    Addr blkAlign(Addr addr) const
    {
	return (addr & ~(Addr)(blkSize-1));
    }

    /**
     * Generate the tag from the addres. For fully associative this is just the
     * block address.
     * @param addr The address to get the tag from.
     * @param blk ignored here
     * @return The tag.
     */
    Addr extractTag(Addr addr, FALRUBlk *blk) const
    {
	return blkAlign(addr);
    }

    /**
     * Return the set of an address. Only one set in a fully associative cache.
     * @param addr The address to get the set from.
     * @return 0.
     */
    int extractSet(Addr addr) const
    {
	return 0;
    }

    /**
     * Calculate the block offset of an address.
     * @param addr the address to get the offset of.
     * @return the block offset.
     */
    int extractBlkOffset(Addr addr) const
    {
	return (addr & (Addr)(blkSize-1));
    }

    /**
     * Regenerate the block address from the tag and the set.
     * @param tag The tag of the block.
     * @param set The set the block belongs to.
     * @return the block address.
     */
    Addr regenerateBlkAddr(Addr tag, int set) const
    {
	return (tag);
    }

    /**
     * Read the data out of the internal storage of a cache block. FALRU
     * currently doesn't support data storage.
     * @param blk The cache block to read.
     * @param data The buffer to read the data into.
     * @return The data from the cache block.
     */
    void readData(FALRUBlk *blk, uint8_t *data)
    {
    }

    /**
     * Write data into the internal storage of a cache block. FALRU
     * currently doesn't support data storage.
     * @param blk The cache block to be written.
     * @param data The data to write.
     * @param size The number of bytes to write.
     * @param writebacks A list for any writebacks to be performed. May be
     * needed when writing to a compressed block.
     */
    void writeData(FALRUBlk *blk, uint8_t *data, int size,
		   MemReqList &writebacks)
    {
    }

    /**
     * Unimplemented. Perform a cache block copy from block aligned addresses.
     * @param source The block aligned source address.
     * @param dest The block aligned destination adddress.
     * @param asid The address space ID.
     * @param writebacks List for any generated writeback requests.
     */
    void doCopy(Addr source, Addr dest, int asid, MemReqList &writebacks)
    {
    }
    
    /**
     * Unimplemented.
     */
    void fixCopy(MemReqPtr &req, MemReqList &writebacks)
    {
    }
	
};

#endif
