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
 * Defines a base class for main memory objects.
 */

#ifndef __BASE_MEMORY_HH__
#define __BASE_MEMORY_HH__

#include <vector>

#include "config/full_system.hh"
#include "mem/base_mem.hh"
#include "base/statistics.hh"
#include "mem/mem_req.hh" // For MemReqPtr

class Bus;
class FunctionalMemory;

typedef enum{
    DRAM_READ,
    DRAM_WRITE,
    DRAM_CMD_CNT
} DRAM_STATS_CMD;

/**
 * A common base class for main memory objects.
 */
class BaseMemory : public BaseMem
{
  protected:
#if FULL_SYSTEM
    /**
     * The functional memory that corresponds to this timing memory.
     */
    FunctionalMemory *funcMem;
#endif

    /** The latency of uncached accesses. */
    const int uncacheLatency;
    /**
     * True if this memory should get dirty data from cache to cache transfers.
     */
    const bool snarfUpdates;
    /**
     * True if this memory should copy data into functional memory.
     */
    const bool doWrites;

    int num_banks; // needs to be here for statistics allocation
    int bmCPUCount;

    // statistics
    /**
     * @addtogroup MemoryStatistics Memory Statistics
     * @{
     */

    /** The number of reads */
    Stats::Scalar<> number_of_reads;
    /** The number of writes */
    Stats::Scalar<> number_of_writes;
    /** The number of reads that hit open page */
    Stats::Scalar<> number_of_reads_hit;
    /** The number of writes that hit open page */
    Stats::Scalar<> number_of_writes_hit;

    /** Read hit rate */
    Stats::Formula read_hit_rate;
    /** Write hit rate */
    Stats::Formula write_hit_rate;

    Stats::Formula overall_hit_rate;

    /** Total latency */
    Stats::Scalar<> total_latency;
    /** Average latency */
    Stats::Formula average_latency;

    /* Slow read hits */
    Stats::Scalar<> number_of_slow_read_hits;
    /* Slow write hits */
    Stats::Scalar<> number_of_slow_write_hits;

    /* Non-overlapping activates */
    Stats::Scalar<> number_of_non_overlap_activate;

    Stats::Vector<> accessesPerBank;

    Stats::Vector<> pageConflicts;
    Stats::Vector<> pageMisses;
    Stats::Vector<> pageHits;

    Stats::Vector<> pageConflictLatency;
    Stats::Vector<> pageMissLatency;
    Stats::Vector<> pageHitLatency;

    Stats::Formula avgPageConflictLatency;
    Stats::Formula avgPageMissLatency;
    Stats::Formula avgPageHitLatency;

    Stats::VectorDistribution<> pageConflictLatencyDistribution;
    Stats::VectorDistribution<> pageMissLatencyDistribution;

    Stats::Vector<> perCPUPageConflicts;
    Stats::Vector<> perCPUPageMisses;
    Stats::Vector<> perCPUPageHits;
    Stats::Vector<> perCPURequests;

    Stats::Formula perCPUConflictRate;
    Stats::Formula perCPUMissRate;
    Stats::Formula perCPUHitRate;

  public:

    /**
     * Collection of parameters for a BaseMemory.
     */
    class Params
    {
      public:
	/** The input bus for this memory. */
	Bus *in;
#if FULL_SYSTEM
	/** The corresponding functional memory. */
	FunctionalMemory *funcMem;
#endif
	/** The hit latency of this memory. */
	int access_lat;
	/** The latency for uncacheable accesses to this memory. */
	int uncache_lat;
	/** Should we update on cache satisfied requests? */
	bool snarf_updates;
	/** Should we write data to memory? */
	bool do_writes;
	/** List of address ranges for this memory. */
	std::vector<Range<Addr> > addrRange;

    /* DDR2 params */
    int num_banks;
    int RAS_latency;
    int CAS_latency;
    int precharge_latency;
    int min_activate_to_precharge_latency;

    bool static_memory_latency;

    };

    /**
     * Creates and initializes this memory.
     * @param name The name of this memory.
     * @param hier Pointer to the hierarchy wide params.
     * @param params The parameters for this memory.
     */
    BaseMemory(const std::string &name, HierParams *hier, Params &params);

    /**
     * Register statistics
     */
    void regStats();

    /**
     * Dummy implementation.
     */
    int getBlockSize() const
    {
	return 0;
    }

    /**
     * Dummy implementation.
     */
    void squash(int thread_num)
    {
    }

    /**
     * Dummy implementation.
     */
    int outstandingMisses() const
    {
	return 0;
    }

    /**
     * Dummy implementation.
     */
    bool isBlocked() const
    {
	return false;
    }

    /**
     * Dummy implementation.
     */
    MemReqPtr getCoherenceReq()
    {
	return NULL;
    }

    /**
     * Dummy implementation.
     */
    bool doSlaveRequest()
    {
	return false;
    }

#ifdef CACHE_DEBUG
    virtual void removePendingRequest(Addr address, MemReqPtr& req){
        fatal("BaseMemory removePendingRequest() should never be called!");
    }
    virtual void addPendingRequest(Addr address, MemReqPtr& req){
        fatal("BaseMemory addPendingRequest() should never be called!");
    }
#endif
};

#endif //__BASE_MEMORY_HH__
