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
 * Declares a basic cache interface BaseCache.
 */

#ifndef __BASE_CACHE_HH__
#define __BASE_CACHE_HH__

#include <vector>

#include "base/statistics.hh"
#include "base/trace.hh"
#include "mem/base_mem.hh"
#include "mem/bus/base_interface.hh"
#include "mem/mem_cmd.hh"
#include "mem/mem_req.hh" // For MemReqPtr

#include "mem/cache/miss/adaptive_mha.hh"
#include "mem/interference_manager.hh"

#include "mem/cache/coherence/directory.hh"
#include "mem/cache/cache_blk.hh"


// Forward declarations
class Bus;
class Crossbar;
class CacheAliveCheckEvent;
class CacheProfileEvent;

/**
 * Reasons for Caches to be Blocked.
 */
enum BlockedCause{
    Blocked_NoMSHRs,
    Blocked_NoTargets,
    Blocked_NoWBBuffers,
    Blocked_Coherence,
    Blocked_Copy,
    NUM_BLOCKED_CAUSES
};

/**
 * Reasons for cache to request a bus.
 */
enum RequestCause{
    Request_MSHR,
    Request_WB,
    Request_Coherence,
    Request_PF,
    Request_DirectoryCoherence
};

class AdaptiveMHA;

/**
 * A basic cache interface. Implements some common functions for speed.
 */
class BaseCache : public BaseMem {
  private:
    /**
     * Bit vector of the blocking reasons for the access path.
     * @sa #BlockedCause
     */
    uint8_t blocked;

    /**
     * Bit vector for the blocking reasons for the snoop path.
     * @sa #BlockedCause
     */
    uint8_t blockedSnoop;

    /**
     * Bit vector for the outstanding requests for the master interface.
     */
    uint8_t masterRequests;

    /**
     * Bit vector for the outstanding requests for the slave interface.
     */
    uint8_t slaveRequests;

    void printBlockedState();

    // deadlock check variables
    CacheAliveCheckEvent* checkEvent;
    Tick blockedAt;

    std::vector<std::vector<int> > interferenceEventsBW;
    std::vector<std::vector<int> > interferenceEventsCapacity;

  protected:

    /** The master interface, typically nearer to Main Memory */
    BaseInterface *mi;

    /** True if this cache is connected to the CPU. */
    bool topLevelCache;

    /** Stores time the cache blocked for statistics. */
    Tick blockedCycle;

    /** Block size of this cache */
    const int blkSize;

    /** The number of misses to trigger an exit event. */
    Counter missCount;

    // cache contention variables
    bool simulateContention;
    Tick nextFreeCache;

    struct cacheOccupancy{
        Tick startTick;
        Tick endTick;
        int occCPUID;
        Tick originalRequestTick;

        cacheOccupancy(Tick _start, Tick _end, int _id, Tick _origReq)
            : startTick(_start), endTick(_end), occCPUID(_id), originalRequestTick(_origReq)
        {
        }

        std::string toString(){
            std::stringstream retstr;
            retstr << "Occupied by CPU" << occCPUID << " from " << startTick << " to " << endTick << ", first req at " << originalRequestTick;
            return retstr.str();
        }
    };

    std::vector<cacheOccupancy> occupancy;

  public:

	enum WritebackOwnerPolicy{
		WB_POLICY_UNKNOWN,
		WB_POLICY_OWNER,
		WB_POLICY_REPLACER,
		NUM_WB_POLICIES
	};

	WritebackOwnerPolicy writebackOwnerPolicy;

    /** Bank addressing scheme */
    bool doModuloAddressing;
    int bankID;
    int bankCount;

    bool isShared;
    bool useDirectory;
    bool isReadOnly;
    bool useAdaptiveMHA;
    AdaptiveMHA* adaptiveMHA;
    bool useUniformPartitioning;
    Tick uniformPartitioningStartTick;
    bool useMTPPartitioning;
    bool useStaticPartInWarmup;

    InterferenceManager* interferenceManager;

    Tick detailedSimulationStartTick;

    CacheProfileEvent* profileEvent;

#ifdef CACHE_DEBUG
    std::map<Addr, std::pair<int, Tick> > pendingRequests;
#endif

    // Statistics
    /**
     * @addtogroup CacheStatistics
     * @{
     */

    /** Number of hits per thread for each type of command. @sa MemCmd */
    Stats::Vector<> hits[NUM_MEM_CMDS];
    /** Number of hits for demand accesses. */
    Stats::Formula demandHits;
    /** Number of hit for all accesses. */
    Stats::Formula overallHits;

    /** Number of misses per thread for each type of command. @sa MemCmd */
    Stats::Vector<> misses[NUM_MEM_CMDS];
    /** Number of misses for demand accesses. */
    Stats::Formula demandMisses;
    /** Number of misses for all accesses. */
    Stats::Formula overallMisses;

    /**
     * Total number of cycles per thread/command spent waiting for a miss.
     * Used to calculate the average miss latency.
     */
    Stats::Vector<> missLatency[NUM_MEM_CMDS];
    /** Total number of cycles spent waiting for demand misses. */
    Stats::Formula demandMissLatency;
    /** Total number of cycles spent waiting for all misses. */
    Stats::Formula overallMissLatency;

    /** The number of accesses per command and thread. */
    Stats::Formula accesses[NUM_MEM_CMDS];
    /** The number of demand accesses. */
    Stats::Formula demandAccesses;
    /** The number of overall accesses. */
    Stats::Formula overallAccesses;

    /** The miss rate per command and thread. */
    Stats::Formula missRate[NUM_MEM_CMDS];
    /** The miss rate of all demand accesses. */
    Stats::Formula demandMissRate;
    /** The miss rate for all accesses. */
    Stats::Formula overallMissRate;

    /** The average miss latency per command and thread. */
    Stats::Formula avgMissLatency[NUM_MEM_CMDS];
    /** The average miss latency for demand misses. */
    Stats::Formula demandAvgMissLatency;
    /** The average miss latency for all misses. */
    Stats::Formula overallAvgMissLatency;

    /** The total number of cycles blocked for each blocked cause. */
    Stats::Vector<> blocked_cycles;
    /** The number of times this cache blocked for each blocked cause. */
    Stats::Vector<> blocked_causes;

    /** The average number of cycles blocked for each blocked cause. */
    Stats::Formula avg_blocked;

    /** The number of fast writes (WH64) performed. */
    Stats::Scalar<> fastWrites;

    /** The number of cache copies performed. */
    Stats::Scalar<> cacheCopies;

    /** The number of good prefetches */
    Stats::Scalar<> goodprefetches;

    /** Per cpu cache miss statistics */
    Stats::Vector<> missesPerCPU;
    /** Per cpu cache access statistics */
    Stats::Vector<> accessesPerCPU;

    Stats::Scalar<> delayDueToCongestion;

    Stats::Vector<> cpuInterferenceCycles;
    Stats::Vector<> cpuCapacityInterference;

    Stats::Vector<> extraMissLatency;
    Stats::Vector<> numExtraMisses;
    Stats::Vector<> privateMissSharedHit;

    Stats::Scalar<> recvMissResponses;

    /**
     * @}
     */

    /**
     * Register stats for this object.
     */
    virtual void regStats();

  public:

    class Params
    {
      public:
	/** List of address ranges of this cache. */
	std::vector<Range<Addr> > addrRange;
	/** The hit latency for this cache. */
	int hitLatency;
	/** The block size of this cache. */
	int blkSize;
	/**
	 * The maximum number of misses this cache should handle before
	 * ending the simulation.
	 */
	Counter maxMisses;

        int baseCacheCPUCount;

	/**
	 * Construct an instance of this parameter class.
	 */
	Params(std::vector<Range<Addr> > addr_range,
               int hit_latency, int _blkSize, Counter max_misses, int _baseCacheCPUCount)
	    : addrRange(addr_range), hitLatency(hit_latency), blkSize(_blkSize),
	      maxMisses(max_misses), baseCacheCPUCount(_baseCacheCPUCount)
	{
	}
    };

    /**
     * Create and initialize a basic cache object.
     * @param name The name of this cache.
     * @param hier_params Pointer to the HierParams object for this hierarchy
     * of this cache.
     * @param params The parameter object for this BaseCache.
     */
    BaseCache(const std::string &name, HierParams *hier_params, Params &params, bool _isShared, bool _useDirectory, bool _isReadOnly, bool _useUniformPartitioning, Tick _uniformPartitioningStart, bool _useMTPPartitioning);

    /**
     * Set the master interface for this cache to the one provided.
     * @param i The new master interface.
     */
    void setMasterInterface(BaseInterface *i)
    {
	mi = i;
	std::list<Range<Addr> > ranges;
	si->getRange(ranges);
	mi->setAddrRange(ranges);
    }

    /**
     * Query block size of a cache.
     * @return  The block size
     */
    int getBlockSize() const
    {
	return blkSize;
    }

    /**
     * Returns true if this cache is connect to the CPU.
     * @return True if this is a L1 cache.
     */
    bool isTopLevel()
    {
	return topLevelCache;
    }

    /**
     * Returns true if the cache is blocked for accesses.
     */
    bool isBlocked()
    {
	return blocked != 0;
    }

    bool isBlockedNoMSHRs()
    {
        uint8_t flag = 1 << Blocked_NoMSHRs;
        return (blocked & flag);
    }

    bool isBlockedNoWBBuffers(){
        uint8_t flag = 1 << Blocked_NoWBBuffers;
        return (blocked & flag);
    }

    bool isBlockedNoTargets(){
        uint8_t flag = 1 << Blocked_NoTargets;
        return (blocked & flag);
    }

    /**
     * Returns true if the cache is blocked for snoops.
     */
    bool isBlockedForSnoop()
    {
	return blockedSnoop != 0;
    }

    /**
     * Marks the access path of the cache as blocked for the given cause. This
     * also sets the blocked flag in the slave interface.
     * @param cause The reason for the cache blocking.
     */
    void setBlocked(BlockedCause cause);

    /**
     * Marks the snoop path of the cache as blocked for the given cause. This
     * also sets the blocked flag in the master interface.
     * @param cause The reason to block the snoop path.
     */
    void setBlockedForSnoop(BlockedCause cause)
    {
	uint8_t flag = 1 << cause;
	blockedSnoop |= flag;
	mi->setBlocked();
    }

    /**
     * Marks the cache as unblocked for the given cause. This also clears the
     * blocked flags in the appropriate interfaces.
     * @param cause The newly unblocked cause.
     * @warning Calling this function can cause a blocked request on the bus to
     * access the cache. The cache must be in a state to handle that request.
     */
    void clearBlocked(BlockedCause cause);

    /**
     * True if the master bus should be requested.
     * @return True if there are outstanding requests for the master bus.
     */
    bool doMasterRequest()
    {
	return masterRequests != 0;
    }

    /**
     * Request the master bus for the given cause and time.
     * @param cause The reason for the request.
     * @param time The time to make the request.
     */
    void setMasterRequest(RequestCause cause, Tick time)
    {
	uint8_t flag = 1<<cause;
	masterRequests |= flag;
	mi->request(time);
    }


    /**
     * Clear the master bus request for the given cause.
     * @param cause The request reason to clear.
     */
    void clearMasterRequest(RequestCause cause)
    {
        uint8_t flag = 1<<cause;
	masterRequests &= ~flag;
    }

    /**
     * Return true if the slave bus should be requested.
     * @return True if there are outstanding requests for the slave bus.
     */
    bool doSlaveRequest()
    {
	return slaveRequests != 0;
    }

    /**
     * Request the slave bus for the given reason and time.
     * @param cause The reason for the request.
     * @param time The time to make the request.
     */
    void setSlaveRequest(RequestCause cause, Tick time)
    {
	uint8_t flag = 1<<cause;
	slaveRequests |= flag;
	si->request(time);
    }

    /**
     * Clear the slave bus request for the given reason.
     * @param cause The request reason to clear.
     */
    void clearSlaveRequest(RequestCause cause)
    {
	uint8_t flag = 1<<cause;
	slaveRequests &= ~flag;
    }

    /**
     * Send a response to the slave interface.
     * @param req The request being responded to.
     * @param time The time the response is ready.
     */
    void respond(MemReqPtr &req, Tick time)
    {
	si->respond(req,time);
    }

    /**
     * Send a reponse to the slave interface and calculate miss latency.
     * @param req The request to respond to.
     * @param time The time the response is ready.
     */
    void respondToMiss(MemReqPtr &req, Tick time, bool moreTargetsToService);

    Tick updateAndStoreInterference(MemReqPtr &req, Tick time);
    void updateInterference(MemReqPtr &req);

    /**
     * Suppliess the data if cache to cache transfers are enabled.
     * @param req The bus transaction to fulfill.
     */
    void respondToSnoop(MemReqPtr &req)
    {
	mi->respond(req,curTick + hitLatency);
    }

    /**
     * Notification from master interface that a address range changed. Nothing
     * to do for a cache.
     */
    void rangeChange() {}

//     InterfaceType getMasterInterfaceType(){
//         return mi->getInterfaceType();
//     }
//
//     void setMasterRequestAddr(Addr address){
//         /* This method only makes sense if the interconnect is a crossbar */
//         assert(mi->getInterfaceType() == CROSSBAR);
//         mi->setCurrentRequestAddr(address);
//     }

    bool isInstructionCache(){
        return isReadOnly;
    }

    bool isDirectoryAndL2Cache(){
        return isShared && useDirectory;
    }

    bool isDirectoryAndL1DataCache(){
        return !isReadOnly && !isShared && useDirectory;
    }

    void checkIfCacheAlive();

    void setSenderID(MemReqPtr& req);

    std::vector<std::vector<int> > retrieveBWInterferenceStats();
    void resetBWInterferenceStats();
    std::vector<std::vector<int> > retrieveCapacityInterferenceStats();
    void resetCapacityInterferenceStats();
    void addCapacityInterference(int victimID, int interfererID);

    virtual void setMTPPartition(std::vector<int> setQuotas) = 0;

    virtual void handleProfileEvent() = 0;

    virtual void handleRepartitioningEvent() = 0;

//     virtual DirectoryProtocol* getDirectoryProtocol() = 0;

    virtual int getCacheCPUid() = 0;

    virtual MemAccessResult access(MemReqPtr &req) = 0;

    virtual CacheBlk::State getNewCoherenceState(MemReqPtr &req, CacheBlk::State old_state) = 0;

    virtual void missQueueHandleResponse(MemReqPtr &req, Tick time) = 0;

    // Adaptive MHA methods
    virtual void incrementNumMSHRs(bool onMSHRs) = 0;
    virtual void decrementNumMSHRs(bool onMSHRs) = 0;
    virtual void incrementNumMSHRsByOne(bool onMSHRs) = 0;
    virtual void decrementNumMSHRsByOne(bool onMSHRs) = 0;
    virtual int getCurrentMSHRCount(bool onMSHRs) = 0;

    virtual std::vector<int> perCoreOccupancy() = 0;

    virtual void dumpHitStats() = 0;

#ifdef CACHE_DEBUG
    virtual void removePendingRequest(Addr address, MemReqPtr& req) = 0;
    virtual void addPendingRequest(Addr address, MemReqPtr& req) = 0;
#endif
};

class CacheAliveCheckEvent : public Event
{

    public:

        BaseCache* cache;

        CacheAliveCheckEvent(BaseCache* _cache)
            : Event(&mainEventQueue), cache(_cache)
        {
        }

        void process(){
            cache->checkIfCacheAlive();
        }

        virtual const char *description(){
            return "Cache Alive Check Event";
        }
};

class CacheProfileEvent : public Event
{

    public:

        BaseCache* cache;

        CacheProfileEvent(BaseCache* _cache)
        : Event(&mainEventQueue), cache(_cache)
        {
        }

        void process(){
            cache->handleProfileEvent();
        }

        virtual const char *description(){
            return "Cache Profiling Event";
        }
};

class CacheRepartitioningEvent: public Event
{

    public:

        BaseCache* cache;

        CacheRepartitioningEvent(BaseCache* _cache)
            : Event(&mainEventQueue), cache(_cache)
        {
        }

        void process(){
            cache->handleRepartitioningEvent();
        }

        virtual const char *description(){
            return "Cache Repartitioning Event";
        }
};

class CacheDumpStatsEvent: public Event
{

    public:

        BaseCache* cache;

        CacheDumpStatsEvent(BaseCache* _cache)
    : Event(&mainEventQueue), cache(_cache)
        {
        }

        void process(){
            cache->dumpHitStats();
            delete this;
        }

        virtual const char *description(){
            return "Cache Dump Hit Stats Event";
        }
};


#endif //__BASE_CACHE_HH__
