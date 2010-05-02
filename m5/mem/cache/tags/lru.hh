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
 * Declaration of a LRU tag store.
 */

#ifndef __LRU_HH__
#define __LRU_HH__

#include <list>

#include "mem/cache/cache_blk.hh" // base class
#include "mem/mem_req.hh" // for inlined functions
#include <assert.h>
#include "mem/cache/tags/base_tags.hh"

class BaseCache;
class LRU;

/**
 * LRU cache block.
 */
class LRUBlk : public CacheBlk {
public:
	/** Has this block been touched? Used to aid calculation of warmup time. */
	bool isTouched;
};

/**
 * An associative set of cache blocks.
 */
class CacheSet
{
public:
	/** The associativity of this set. */
	int assoc;

	/** Cache blocks in this set, maintained in LRU order 0 = MRU. */
	LRUBlk **blks;

	LRU* lruTags;

	/**
	 * Find a block matching the tag in this set.
	 * @param asid The address space ID.
	 * @param tag The Tag to find.
	 * @return Pointer to the block if found.
	 */
	LRUBlk* findBlk(int asid, Addr tag, int maxUseSets);

	LRUBlk* findBlk(int asid, Addr tag, int* hitIndex, int maxUseSets);

	/**
	 * Move the given block to the head of the list.
	 * @param blk The block to move.
	 */
	void moveToHead(LRUBlk *blk);
};

/**
 * A LRU cache tag store.
 */
class LRU : public BaseTags
{
public:
	/** Typedef the block type used in this tag store. */
	typedef LRUBlk BlkType;
	/** Typedef for a list of pointers to the local block class. */
	typedef std::list<LRUBlk*> BlkList;
protected:
	/** The number of sets in the cache. */
	const int numSets;
	/** The number of bytes in a block. */
	const int blkSize;
	/** The associativity of the cache. */
	const int assoc;
	/** The hit latency. */
	const int hitLatency;

	const int numBanks;

	const bool isShadow;

	int maxUseWays;

	/** The cache sets. */
	CacheSet *sets;

	/** The cache blocks. */
	LRUBlk *blks;
	/** The data blocks, 1 per cache block. */
	uint8_t *dataBlks;

	/** The amount to shift the address to get the set. */
	int setShift;
	/** The amount to shift the address to get the tag. */
	int tagShift;
	/** Mask out all bits that aren't part of the set index. */
	unsigned setMask;
	/** Mask out all bits that aren't part of the block offset. */
	unsigned blkMask;

	int bankShift;

	std::vector<std::vector<int> > perSetHitCounters;
	int accesses;
	std::vector<int> currentPartition;
	bool doPartitioning;

	std::vector<std::vector<std::vector<int> > > perCPUperSetHitCounters;

	int divFactor;
	int shadowID;

	std::vector<int> getUsedBlocksPerCore(unsigned int set);
	LRUBlk* findLRUBlkForCPU(int cpuID, unsigned int set);
	LRUBlk* findLRUOverQuotaBlk(std::vector<int> blocksInUse, unsigned int set);

public:
	/**
	 * Construct and initialize this tag store.
	 * @param _numSets The number of sets in the cache.
	 * @param _blkSize The number of bytes in a block.
	 * @param _assoc The associativity of the cache.
	 * @param _hit_latency The latency in cycles for a hit.
	 */
	LRU(int _numSets,
		int _blkSize,
		int _assoc,
		int _hit_latency,
		int _bank_count,
		bool _isShadow,
		int _divFactor,
		int _maxUseWays,
		int _shadowID = -1);

	/**
	 * Destructor
	 */
	virtual ~LRU();

	/**
	 * Return the block size.
	 * @return the block size.
	 */
	int getBlockSize()
	{
		return blkSize;
	}

	/**
	 * Return the subblock size. In the case of LRU it is always the block
	 * size.
	 * @return The block size.
	 */
	int getSubBlockSize()
	{
		return blkSize;
	}

	int getNumSets(){
		return numSets;
	}

	int getAssoc(){
		return assoc;
	}

	/**
	 * Search for the address in the cache.
	 * @param asid The address space ID.
	 * @param addr The address to find.
	 * @return True if the address is in the cache.
	 */
	bool probe(int asid, Addr addr) const;

	/**
	 * Invalidate the block containing the given address.
	 * @param asid The address space ID.
	 * @param addr The address to invalidate.
	 */
	void invalidateBlk(int asid, Addr addr);

	/**
	 * Finds the given address in the cache and update replacement data.
	 * Returns the access latency as a side effect.
	 * @param req The request whose block to find.
	 * @param lat The access latency.
	 * @return Pointer to the cache block if found.
	 */
	LRUBlk* findBlock(MemReqPtr &req, int &lat);

	/**
	 * Finds the given address in the cache and update replacement data.
	 * Returns the access latency as a side effect.
	 * @param addr The address to find.
	 * @param asid The address space ID.
	 * @param lat The access latency.
	 * @return Pointer to the cache block if found.
	 */
	LRUBlk* findBlock(Addr addr, int asid, int &lat);

	/**
	 * Finds the given address in the cache, do not update replacement data.
	 * @param addr The address to find.
	 * @param asid The address space ID.
	 * @return Pointer to the cache block if found.
	 */
	LRUBlk* findBlock(Addr addr, int asid) const;

	/**
	 * Find a replacement block for the address provided.
	 * @param req The request to a find a replacement candidate for.
	 * @param writebacks List for any writebacks to be performed.
	 * @param compress_blocks List of blocks to compress, for adaptive comp.
	 * @return The block to place the replacement in.
	 */
	LRUBlk* findReplacement(MemReqPtr &req, MemReqList &writebacks,
			BlkList &compress_blocks);

	/**
	 * Generate the tag from the given address.
	 * @param addr The address to get the tag from.
	 * @return The tag of the address.
	 */
	Addr extractTag(Addr addr) const
	{
		return (addr >> tagShift);
	}

	/**
	 * Generate the tag from the given address.
	 * @param addr The address to get the tag from.
	 * @param blk Ignored.
	 * @return The tag of the address.
	 */
	Addr extractTag(Addr addr, LRUBlk *blk) const
	{
		return (addr >> tagShift);
	}

	/**
	 * Calculate the set index from the address.
	 * @param addr The address to get the set from.
	 * @return The set index of the address.
	 */
	int extractSet(Addr addr) const
	{
		return ((addr >> setShift) & setMask);
	}

	/**
	 * Get the block offset from an address.
	 * @param addr The address to get the offset of.
	 * @return The block offset.
	 */
	int extractBlkOffset(Addr addr) const
	{
		return (addr & blkMask);
	}

	/**
	 * Align an address to the block size.
	 * @param addr the address to align.
	 * @return The block address.
	 */
	Addr blkAlign(Addr addr) const
	{
		return (addr & ~(Addr)blkMask);
	}

	/**
	 * Regenerate the block address from the tag.
	 * @param tag The tag of the block.
	 * @param set The set of the block.
	 * @return The block address.
	 */
	Addr regenerateBlkAddr(Addr tag, unsigned set) const
	{
		if(bankShift == -1){
			return ((tag << tagShift) | ((Addr)set << setShift));
		}
		return ((tag << tagShift) | ((Addr)set << setShift) | ((Addr) bankID << bankShift ));
	}

	/**
	 * Return the hit latency.
	 * @return the hit latency.
	 */
	int getHitLatency() const
	{
		return hitLatency;
	}

	/**
	 * Read the data out of the internal storage of the given cache block.
	 * @param blk The cache block to read.
	 * @param data The buffer to read the data into.
	 * @return The cache block's data.
	 */
	void readData(LRUBlk *blk, uint8_t *data)
	{
		memcpy(data, blk->data, blk->size);
	}

	/**
	 * Write data into the internal storage of the given cache block. Since in
	 * LRU does not store data differently this just needs to update the size.
	 * @param blk The cache block to write.
	 * @param data The data to write.
	 * @param size The number of bytes to write.
	 * @param writebacks A list for any writebacks to be performed. May be
	 * needed when writing to a compressed block.
	 */
	void writeData(LRUBlk *blk, uint8_t *data, int size,
			MemReqList & writebacks)
	{
		assert(size <= blkSize);
		blk->size = size;
	}

	/**
	 * Perform a block aligned copy from the source address to the destination.
	 * @param source The block-aligned source address.
	 * @param dest The block-aligned destination address.
	 * @param asid The address space DI.
	 * @param writebacks List for any generated writeback requests.
	 */
	void doCopy(Addr source, Addr dest, int asid, MemReqList &writebacks);

	/**
	 * No impl.
	 */
	void fixCopy(MemReqPtr &req, MemReqList &writebacks)
	{
	}

	/**
	 * Called at end of simulation to complete average block reference stats.
	 */
	virtual void cleanupRefs();

	virtual std::vector<int> perCoreOccupancy();

//	virtual void handleSwitchEvent();

	void resetHitCounters();

	void dumpHitCounters();

	std::vector<double> getMissRates();

	double getTouchedRatio();

	void enablePartitioning();
	virtual void setCachePartition(std::vector<int> setQuotas);

	virtual void updateSetHitStats(MemReqPtr& req);

	virtual void dumpHitStats();

	virtual void initializeCounters(int cpuCount);

	std::string generateIniName(std::string cachename, int set, int pos);

	virtual void serialize(std::ostream &os);
	virtual void unserialize(Checkpoint *cp, const std::string &section);
};

#endif
