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
 * Miss and writeback queue declarations.
 */

#ifndef __MISS_QUEUE_HH__
#define __MISS_QUEUE_HH__

#include <vector>

#include "mem/cache/miss/mshr.hh"
#include "mem/cache/miss/mshr_queue.hh"
#include "base/statistics.hh"
#include "mem/requesttrace.hh"
#include "mem/cache/miss/throttle_control.hh"

class BaseCache;
class BasePrefetcher;

/**
 * Manages cache misses and writebacks. Contains MSHRs to store miss data
 * and the writebuffer for writes/writebacks.
 * @todo need to handle data on writes better (encapsulate).
 * @todo need to make replacements/writebacks happen in Cache::access
 */
class MissQueue
{
  private:
    bool mqWasPrevQueue;
    bool changeQueue;
    Tick prevTime;
    Tick lastMissAt;

    RequestTrace latencyTrace;
    RequestTrace interferenceTrace;

    ThrottleControl* throttleControl;

    std::vector<Tick> lastBurstMissAt;
    std::vector<int> burstSizeAccumulator;

    int avgBurstSizeAccumulator;
    int numDetectedBursts;

  protected:
    /** The MSHRs. */
    MSHRQueue mq;
    /** Write Buffer. */
    MSHRQueue wb;

    // PARAMTERS

    /** The number of MSHRs in the miss queue. */
    const int numMSHR;
    /** The number of targets for each MSHR. */
    const int numTarget;
    /** The number of write buffers. */
    const int writeBuffers;
    /** True if the cache should allocate on a write miss. */
    const bool writeAllocate;
    /** Pointer to the parent cache. */
    BaseCache* cache;

    /** The Prefetcher */
    BasePrefetcher *prefetcher;

    /** The block size of the parent cache. */
    int blkSize;

    /** Increasing order number assigned to each incoming request. */
    uint64_t order;

    bool prefetchMiss;

    // Statistics
    /**
     * @addtogroup CacheStatistics
     * @{
     */
    /** Number of blocks written back per thread. */
    Stats::Vector<> writebacks;

    /** Number of misses that hit in the MSHRs per command and thread. */
    Stats::Vector<> mshr_hits[NUM_MEM_CMDS];
    /** Demand misses that hit in the MSHRs. */
    Stats::Formula demandMshrHits;
    /** Total number of misses that hit in the MSHRs. */
    Stats::Formula overallMshrHits;

    /** Number of misses that miss in the MSHRs, per command and thread. */
    Stats::Vector<> mshr_misses[NUM_MEM_CMDS];
    /** Demand misses that miss in the MSHRs. */
    Stats::Formula demandMshrMisses;
    /** Total number of misses that miss in the MSHRs. */
    Stats::Formula overallMshrMisses;

    /** Number of misses that miss in the MSHRs, per command and thread. */
    Stats::Vector<> mshr_uncacheable[NUM_MEM_CMDS];
    /** Total number of misses that miss in the MSHRs. */
    Stats::Formula overallMshrUncacheable;

    /** Total cycle latency of each MSHR miss, per command and thread. */
    Stats::Vector<> mshr_miss_latency[NUM_MEM_CMDS];
    /** Total cycle latency of demand MSHR misses. */
    Stats::Formula demandMshrMissLatency;
    /** Total cycle latency of overall MSHR misses. */
    Stats::Formula overallMshrMissLatency;

    /** Total cycle latency of each MSHR miss, per command and thread. */
    Stats::Vector<> mshr_uncacheable_lat[NUM_MEM_CMDS];
    /** Total cycle latency of overall MSHR misses. */
    Stats::Formula overallMshrUncacheableLatency;

    /** The total number of MSHR accesses per command and thread. */
    Stats::Formula mshrAccesses[NUM_MEM_CMDS];
    /** The total number of demand MSHR accesses. */
    Stats::Formula demandMshrAccesses;
    /** The total number of MSHR accesses. */
    Stats::Formula overallMshrAccesses;

    /** The miss rate in the MSHRs pre command and thread. */
    Stats::Formula mshrMissRate[NUM_MEM_CMDS];
    /** The demand miss rate in the MSHRs. */
    Stats::Formula demandMshrMissRate;
    /** The overall miss rate in the MSHRs. */
    Stats::Formula overallMshrMissRate;

    /** The average latency of an MSHR miss, per command and thread. */
    Stats::Formula avgMshrMissLatency[NUM_MEM_CMDS];
    /** The average latency of a demand MSHR miss. */
    Stats::Formula demandAvgMshrMissLatency;
    /** The average overall latency of an MSHR miss. */
    Stats::Formula overallAvgMshrMissLatency;

    /** The average latency of an MSHR miss, per command and thread. */
    Stats::Formula avgMshrUncacheableLatency[NUM_MEM_CMDS];
    /** The average overall latency of an MSHR miss. */
    Stats::Formula overallAvgMshrUncacheableLatency;

    /** The number of times a thread hit its MSHR cap. */
    Stats::Vector<> mshr_cap_events;
    /** The number of times software prefetches caused the MSHR to block. */
    Stats::Vector<> soft_prefetch_mshr_full;

    Stats::Scalar<> mshr_no_allocate_misses;

    Stats::Scalar<> sum_roundtrip_latency;
    Stats::Scalar<> num_roundtrip_responses;
    Stats::Formula avg_roundtrip_latency;

    Stats::Scalar<> sum_roundtrip_interference;
    Stats::Formula avg_roundtrip_interference;

    Stats::Scalar<> interconnect_entry_interference;
    Stats::Scalar<> interconnect_transfer_interference;
    Stats::Scalar<> interconnect_delivery_interference;
    Stats::Scalar<> bus_entry_interference;
    Stats::Scalar<> bus_queue_interference;
    Stats::Scalar<> bus_service_interference;
    Stats::Scalar<> cache_capacity_interference;

    Stats::Formula avg_interconnect_entry_interference;
    Stats::Formula avg_interconnect_transfer_interference;
    Stats::Formula avg_interconnect_delivery_interference;
    Stats::Formula avg_bus_entry_interference;
    Stats::Formula avg_bus_queue_interference;
    Stats::Formula avg_bus_service_interference;
    Stats::Formula avg_cache_capacity_interference;

    Stats::Scalar<> interconnect_entry_latency;
    Stats::Scalar<> interconnect_transfer_latency;
    Stats::Scalar<> interconnect_delivery_latency;
    Stats::Scalar<> bus_entry_latency;
    Stats::Scalar<> bus_queue_latency;
    Stats::Scalar<> bus_service_latency;

    Stats::Formula avg_interconnect_entry_latency;
    Stats::Formula avg_interconnect_transfer_latency;
    Stats::Formula avg_interconnect_delivery_latency;
    Stats::Formula avg_bus_entry_latency;
    Stats::Formula avg_bus_queue_latency;
    Stats::Formula avg_bus_service_latency;

    Stats::Scalar<> cycles_between_misses;
    Stats::Formula avg_cycles_between_misses;

    Stats::Distribution<> cycles_between_misses_distribution;

    Stats::VectorDistribution<> burstSizeDistribution;

  private:
    /** Pointer to the MSHR that has no targets. */
    MSHR* noTargetMSHR;

    /**
     * Allocate a new MSHR to handle the provided miss.
     * @param req The miss to buffer.
     * @param size The number of bytes to fetch.
     * @param time The time the miss occurs.
     * @return A pointer to the new MSHR.
     */
    MSHR* allocateMiss(MemReqPtr &req, int size, Tick time);

    /**
     * Allocate a new WriteBuffer to handle the provided write.
     * @param req The write to handle.
     * @param size The number of bytes to write.
     * @param time The time the write occurs.
     * @return A pointer to the new write buffer.
     */
    MSHR* allocateWrite(MemReqPtr &req, int size, Tick time);

    void addBurstStats(MemReqPtr &req);

  public:
    /**
     * Simple Constructor. Initializes all needed internal storage and sets
     * parameters.
     * @param numMSHRs The number of outstanding misses to handle.
     * @param numTargets The number of outstanding targets to each miss.
     * @param write_buffers The number of outstanding writes to handle.
     * @param write_allocate If true, treat write misses the same as reads.
     */
    MissQueue(int numMSHRs, int numTargets, int write_buffers,
	      bool write_allocate, bool prefetch_miss, bool _doMSHRTrace, ThrottleControl* _tc);

    /**
     * Deletes all allocated internal storage.
     */
    ~MissQueue();

    int getNumMSHRs(){
    	return numMSHR;
    }

    /**
     * Register statistics for this object.
     * @param name The name of the parent cache.
     */
    void regStats(const std::string &name);

    /**
     * Called by the parent cache to set the back pointer.
     * @param _cache A pointer to the parent cache.
     */
    void setCache(BaseCache *_cache);

    void setPrefetcher(BasePrefetcher *_prefetcher);

    /**
     * Handle a cache miss properly. Either allocate an MSHR for the request,
     * or forward it through the write buffer.
     * @param req The request that missed in the cache.
     * @param blk_size The block size of the cache.
     * @param time The time the miss is detected.
     */
    void handleMiss(MemReqPtr &req, int blk_size, Tick time);

    /**
     * Fetch the block for the given address and buffer the given target.
     * @param addr The address to fetch.
     * @param asid The address space of the address.
     * @param blk_size The block size of the cache.
     * @param time The time the miss is detected.
     * @param target The target for the fetch.
     */
    MSHR* fetchBlock(Addr addr, int asid, int blk_size, Tick time,
		     MemReqPtr &target);

    /**
     * Selects a outstanding request to service.
     * @return The request to service, NULL if none found.
     */
    MemReqPtr getMemReq();

    /**
     * Set the command to the given bus command.
     * @param req The request to update.
     * @param cmd The bus command to use.
     */
    void setBusCmd(MemReqPtr &req, MemCmd cmd);

    /**
     * Restore the original command in case of a bus transmission error.
     * @param req The request to reset.
     */
    void restoreOrigCmd(MemReqPtr &req);

    /**
     * Marks a request as in service (sent on the bus). This can have side
     * effect since storage for no response commands is deallocated once they
     * are successfully sent.
     * @param req The request that was sent on the bus.
     */
    void markInService(MemReqPtr &req);

    /**
     * Collect statistics and free resources of a satisfied request.
     * @param req The request that has been satisfied.
     * @param time The time when the request is satisfied.
     */
    void handleResponse(MemReqPtr &req, Tick time);

    /**
     * Removes all outstanding requests for a given thread number. If a request
     * has been sent to the bus, this function removes all of its targets.
     * @param thread_number The thread number of the requests to squash.
     */
    void squash(int thread_number);

    /**
     * Return the current number of outstanding misses.
     * @return the number of outstanding misses.
     */
    int getMisses()
    {
	return mq.getAllocatedTargets();
    }

    /**
     * Searches for the supplied address in the miss queue.
     * @param addr The address to look for.
     * @param asid The address space id.
     * @return The MSHR that contains the address, NULL if not found.
     * @warning Currently only searches the miss queue. If non write allocate
     * might need to search the write buffer for coherence.
     */
    MSHR* findMSHR(Addr addr, int asid) const;

    /**
     * Searches for the supplied address in the write buffer.
     * @param addr The address to look for.
     * @param asid The address space id.
     * @param writes The list of writes that match the address.
     * @return True if any writes are found
     */
    bool findWrites(Addr addr, int asid, std::vector<MSHR*>& writes) const;

    /**
     * Perform a writeback of dirty data to the given address.
     * @param addr The address to write to.
     * @param asid The address space id.
     * @param xc The execution context of the address space.
     * @param size The number of bytes to write.
     * @param data The data to write, can be NULL.
     * @param compressed True if the data is compressed.
     */
    void doWriteback(Addr addr, int asid, ExecContext *xc,
		     int size, uint8_t *data, bool compressed);

    /**
     * Perform the given writeback request.
     * @param req The writeback request.
     */
    void doWriteback(MemReqPtr &req);

    /**
     * Returns true if there are outstanding requests.
     * @return True if there are outstanding requests.
     */
    bool havePending();

    /**
     * Add a target to the given MSHR. This assumes it is in the miss queue.
     * @param mshr The mshr to add a target to.
     * @param req The target to add.
     */
    void addTarget(MSHR *mshr, MemReqPtr &req)
    {
	mq.allocateTarget(mshr, req);
    }

    /**
     * Allocate a MSHR to hold a list of targets to a block involved in a copy.
     * If the block is marked done then the MSHR already holds the data to
     * fill the block. Otherwise the block needs to be fetched.
     * @param addr The address to buffer.
     * @param asid The address space ID.
     * @return A pointer to the allocated MSHR.
     */
    MSHR* allocateTargetList(Addr addr, int asid);

    // Adaptive MHA methods
    void incrementNumMSHRs(bool onMSHRs){
        if(onMSHRs) mq.incrementNumMSHRs();
        else wb.incrementNumMSHRs();
    }

    void decrementNumMSHRs(bool onMSHRs){
        if(onMSHRs){
            mq.decrementNumMSHRs();
            if(mq.isFull() && !cache->isBlockedNoMSHRs()){
                cache->setBlocked(Blocked_NoMSHRs);
            }
        }
        else{
            wb.decrementNumMSHRs();
            if(wb.isFull() && !cache->isBlockedNoWBBuffers()){
                cache->setBlocked(Blocked_NoWBBuffers);
            }
        }
    }

    void incrementNumMSHRsByOne(bool onMSHRs){
        assert(onMSHRs);
        mq.incrementNumMSHRsByOne();
    }

    void decrementNumMSHRsByOne(bool onMSHRs){
        assert(onMSHRs);

        mq.decrementNumMSHRsByOne();
        if(mq.isFull() && !cache->isBlockedNoMSHRs()){
            cache->setBlocked(Blocked_NoMSHRs);
        }
    }

    void setNumMSHRs(int newMSHRCount);

    int getCurrentMSHRCount(bool onMSHRs){
        if(onMSHRs) return mq.getCurrentMSHRCount();
        else return wb.getCurrentMSHRCount();
    }

    std::map<int,int> assignBlockingBlame(bool blockedForMiss, bool blockedForTargets, double threshold);

    void measureInterference(MemReqPtr& req);

    void coreCommittedInstruction();

    std::vector<double> getMLPEstimate(){
    	return mq.getMLPEstimate();
    }

    std::vector<double> getServicedMissesWhileStalledEstimate(){
    	return mq.getServicedMissesWhileStalledEstimate();
    }

    double getAvgBurstSize();

    double getInstTraceMWS(){
    	return mq.getInstTraceMWS();
    }

    double getInstTraceMLP(){
    	return mq.getInstTraceMLP();
    }

    int getResponsesWhileStalled(){
    	return mq.getResponsesWhileStalled();
    }

    int getInstTraceRespWhileStalled(){
    	return mq.getInstTraceRespWhileStalled();
    }

    std::vector<MSHROccupancy>* getOccupancyList(){
    	return mq.getOccupancyList();
    }

    void clearOccupancyList(){
    	mq.clearOccupancyList();
    }

    void enableOccupancyList(){
    	mq.enableOccupancyList();
    }

};


#endif //__MISS_QUEUE_HH__
