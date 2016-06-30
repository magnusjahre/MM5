/*
 * Copyright (c) 2002, 2003, 2004, 2005
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
 * Describes a cache based on template policies.
 */

#ifndef __CACHE_HH__
#define __CACHE_HH__

#include "base/misc.hh" // fatal, panic, and warn
#include "cpu/smt.hh" // SMT_MAX_THREADS

#include "mem/cache/base_cache.hh"
#include "mem/cache/prefetch/prefetcher.hh"

#include "mem/interconnect/interconnect.hh"
#include "mem/cache/coherence/directory.hh"

#include "mem/cache/tags/lru.hh"
#include "mem/config/cache.hh"

#include "mem/cache/partitioning/cache_partitioning.hh"

// forward declarations
class Bus;
class ExecContext;

/**
 * A template-policy based cache. The behavior of the cache can be altered by
 * supplying different template policies. TagStore handles all tag and data
 * storage @sa TagStore. Buffering handles all misses and writes/writebacks
 * @sa MissQueue. Coherence handles all coherence policy details @sa
 * UniCoherence, SimpleMultiCoherence.
 */
template <class TagStore, class Buffering, class Coherence>
class Cache : public BaseCache
{
  private:
    bool idIsSet;
    RequestTrace capacityProfileTrace;

//    MultipleTimeSharingParititions* mtp;

    int accessSample;
    int missSample;

  public:
    /** Define the type of cache block to use. */
    typedef typename TagStore::BlkType BlkType;

    bool prefetchAccess;

    std::string localName;

  protected:

    /** Tag and data Storage */
    TagStore *tags;

    /** Miss and Writeback handler */
    Buffering *missQueue;

    /** Coherence protocol. */
    Coherence *coherence;
    DirectoryProtocol<TagStore> *directoryProtocol;
//     int interfaceID;

    /** Prefetcher */
    Prefetcher<TagStore, Buffering> *prefetcher;

    /** Do fast copies in this cache. */
    bool doCopy;

    /** Block on a delayed copy. */
    bool blockOnCopy;

    /**
     * The clock ratio of the outgoing bus.
     * Used for calculating critical word first.
     */
    int busRatio;

     /**
      * The bus width in bytes of the outgoing bus.
      * Used for calculating critical word first.
      */
    int busWidth;

     /**
      * A permanent mem req to always be used to cause invalidations.
      * Used to append to target list, to cause an invalidation.
      */
    MemReqPtr invalidateReq;

    /**
     * Temporarily move a block into a MSHR.
     * @todo Remove this when LSQ/SB are fixed and implemented in memtest.
     */
    void pseudoFill(Addr addr, int asid);

    /**
     * Temporarily move a block into an existing MSHR.
     * @todo Remove this when LSQ/SB are fixed and implemented in memtest.
     */
    void pseudoFill(MSHR *mshr);

    bool calculatePartitions();

  public:

	  class Params
	  {
	  public:
		  TagStore *tags;
		  Buffering *missQueue;
		  Coherence *coherence;
		  DirectoryProtocol<TagStore> *directoryCoherence;
		  bool doCopy;
		  bool blockOnCopy;
		  BaseCache::Params baseParams;
		  Bus *in;
		  Bus *out;
		  //Crossbar *inCrossbar;
		  //Crossbar *outCrossbar;
		  Interconnect *inInterconnect;
		  Interconnect *outInterconnect;
		  Prefetcher<TagStore, Buffering> *prefetcher;
		  bool prefetchAccess;
		  int cpu_count;
		  int cpu_id;
		  bool multiprog_workload;
		  bool isShared;
		  bool isReadOnly;
		  bool doModuloBankAddr;
		  int bankID;
		  int bankCount;
		  AdaptiveMHA* adaptiveMHA;
//		  bool useUniformPartitioning;
//		  bool useMTPPartitioning;
//		  Tick uniformPartitioningStart;
		  Tick detailedSimStartTick;
//		  Tick mtpEpochSize;
		  bool simulateContention;
//		  bool useStaticPartInWarmup;
		  int memoryAddressOffset;
		  int memoryAddressParts;
		  InterferenceManager* interferenceManager;
		  BasePolicy* missBandwidthPolicy;
		  WritebackOwnerPolicy wbPolicy;
		  int shadowTagLeaderSets;
		  bool useAggMLPEstimator;
		  std::vector<int> staticQuotas;
		  CachePartitioning* partitioning;
		  CacheInterference* cacheInterference;
		  MemoryOverlapEstimator* overlapEstimator;

		  Params(TagStore *_tags, Buffering *mq, Coherence *coh, DirectoryProtocol<TagStore> *_directoryCoherence,
				  bool do_copy, BaseCache::Params params,
				  Bus * in_bus, Bus * out_bus,
				  // Crossbar* _inCrossbar, Crossbar * _outCrossbar,
				  Interconnect* _inInterconnect, Interconnect* _outInterconnect,
				  Prefetcher<TagStore, Buffering> *_prefetcher,
				  bool prefetch_access, int _cpu_count, int _cpu_id, bool _multiprog_workload,
				  bool _isShared, bool _isReadOnly, bool _doModAddr, int _bankID, int _bankCount,
				  AdaptiveMHA* _adaptiveMHA, Tick _detailedSimStartTick, bool _simulateContention,
				  int _memoryAddressOffset, int _memoryAddressParts, InterferenceManager* intman,
				  BasePolicy* mbp, WritebackOwnerPolicy _wbPolicy, int _shadowLeaderSets,
				  bool _useAggMLPEstimator,
				  std::vector<int> _staticQuotas, CachePartitioning* _partitioning, CacheInterference* _cacheInterference, MemoryOverlapEstimator* _oe)
		  : tags(_tags), missQueue(mq), coherence(coh), directoryCoherence(_directoryCoherence)
		  ,doCopy(do_copy), blockOnCopy(false), baseParams(params), in(in_bus), out(out_bus),
		  inInterconnect(_inInterconnect), outInterconnect(_outInterconnect),
		  prefetcher(_prefetcher), prefetchAccess(prefetch_access),
		  cpu_count(_cpu_count), cpu_id(_cpu_id), multiprog_workload(_multiprog_workload),
		  isShared(_isShared), isReadOnly(_isReadOnly),
		  doModuloBankAddr(_doModAddr), bankID(_bankID), bankCount(_bankCount),
		  adaptiveMHA(_adaptiveMHA), detailedSimStartTick(_detailedSimStartTick), simulateContention(_simulateContention),
		  memoryAddressOffset(_memoryAddressOffset), memoryAddressParts(_memoryAddressParts),
		  interferenceManager(intman), missBandwidthPolicy(mbp), wbPolicy(_wbPolicy),
		  shadowTagLeaderSets(_shadowLeaderSets),
		  useAggMLPEstimator(_useAggMLPEstimator), staticQuotas(_staticQuotas), partitioning(_partitioning), cacheInterference(_cacheInterference), overlapEstimator(_oe)
		  {
		  }
	  };

    /** Instantiates a basic cache object. */
    Cache(const std::string &_name, HierParams *hier_params, Params &params);

    ~Cache();

    void regStats();

    /**
     * Performs the access specified by the request.
     * @param req The request to perform.
     * @return The result of the access.
     */
    virtual MemAccessResult access(MemReqPtr &req);

    /**
     * Selects a request to send on the bus.
     * @return The memory request to service.
     */
    MemReqPtr getMemReq();

    /**
     * Was the request was sent successfully?
     * @param req The request.
     * @param success True if the request was sent successfully.
     */
    void sendResult(MemReqPtr &req, bool success);

    /**
     * Handles a response (cache line fill/write ack) from the bus.
     * @param req The request being responded to.
     */
    void handleResponse(MemReqPtr &req);

    /**
     * Start handling a copy transaction.
     * @param req The copy request to perform.
     */
    void startCopy(MemReqPtr &req);

    /**
     * Handle a delayed copy transaction.
     * @param req The delayed copy request to continue.
     * @param addr The address being responded to.
     * @param blk The block of the current response.
     * @param mshr The mshr being handled.
     */
    void handleCopy(MemReqPtr &req, Addr addr, BlkType *blk, MSHR *mshr);

    /**
     * Selects a coherence message to forward to lower levels of the hierarchy.
     * @return The coherence message to forward.
     */
    MemReqPtr getCoherenceReq();

    /**
     * Snoops bus transactions to maintain coherence.
     * @param req The current bus transaction.
     */
    void snoop(MemReqPtr &req);

    void snoopResponse(MemReqPtr &req);

    /**
     * Invalidates the block containing address if found.
     * @param addr The address to look for.
     * @param asid The address space ID of the address.
     * @todo Is this function necessary?
     */
    void invalidateBlk(Addr addr, int asid);

    /**
     * Aquash all requests associated with specified thread.
     * intended for use by I-cache.
     * @param thread_number The thread to squash.
     */
    void squash(int thread_number)
    {
	missQueue->squash(thread_number);
    }

    /**
     * Return the number of outstanding misses in a Cache.
     * Default returns 0.
     *
     * @retval unsigned The number of missing still outstanding.
     */
    unsigned outstandingMisses() const
    {
	return missQueue->getMisses();
    }

    /**
     * Send a response to the slave interface.
     * @param req The request being responded to.
     * @param time The time the response is ready.
     */
    void respond(MemReqPtr &req, Tick time);

    /**
     * Perform the access specified in the request and return the estimated
     * time of completion. This function can either update the hierarchy state
     * or just perform the access wherever the data is found depending on the
     * state of the update flag.
     * @param req The memory request to satisfy
     * @param update If true, update the hierarchy, otherwise just perform the
     * request.
     * @return The estimated completion time.
     */
    Tick probe(MemReqPtr &req, bool update);

    /**
     * Snoop for the provided request in the cache and return the estimated
     * time of completion.
     * @todo Can a snoop probe not change state?
     * @param req The memory request to satisfy
     * @param update If true, update the hierarchy, otherwise just perform the
     * request.
     * @return The estimated completion time.
     */
    Tick snoopProbe(MemReqPtr &req, bool update);

    bool isCache() { return true; }

//     void setInterfaceID(int id){
//         assert(!idIsSet);
//         interfaceID = id;
//         idIsSet = true;
//     }

    int getProcessorID(){
        return cacheCpuID;
    }

    bool isModuloAddressedBank(){
        return doModuloAddressing;
    }

    int getBankID(){
        return bankID;
    }

    int getBankCount(){
        return bankCount;
    }

    int getNumSets(){
        return tags->getNumSets();
    }

    std::map<int,int> assignBlockingBlame();

    virtual void setCachePartition(std::vector<int> setQuotas);
    virtual void enablePartitioning();

    virtual void handleProfileEvent();

//    virtual void handleRepartitioningEvent();

    virtual int getCacheCPUid(){
        return cacheCpuID;
    }

    virtual CacheBlk::State getNewCoherenceState(MemReqPtr &req, CacheBlk::State old_state){
        return coherence->getNewState(req,old_state);
    }

    virtual void missQueueHandleResponse(MemReqPtr &req, Tick time){
        missQueue->handleResponse(req, time);
    }

    // Adaptive MHA methods
    virtual void incrementNumMSHRs(bool onMSHRs){
        missQueue->incrementNumMSHRs(onMSHRs);
    }

    virtual void decrementNumMSHRs(bool onMSHRs){
        missQueue->decrementNumMSHRs(onMSHRs);
    }

    virtual void incrementNumMSHRsByOne(bool onMSHRs){
        missQueue->incrementNumMSHRsByOne(onMSHRs);
    }

    virtual void decrementNumMSHRsByOne(bool onMSHRs){
        missQueue->decrementNumMSHRsByOne(onMSHRs);
    }

    virtual int getCurrentMSHRCount(bool onMSHRs){
        return missQueue->getCurrentMSHRCount(onMSHRs);
    }

    virtual void coreCommittedInstruction(){
    	missQueue->coreCommittedInstruction();
    }

    virtual std::vector<int> perCoreOccupancy(){
        return tags->perCoreOccupancy();
    }

    virtual void dumpHitStats(){
        tags->dumpHitStats();
    }

    virtual std::vector<double> getMLPEstimate(){
    	return missQueue->getMLPEstimate();
    }

    virtual std::vector<double> getServicedMissesWhileStalledEstimate(){
    	return missQueue->getServicedMissesWhileStalledEstimate();
    }

    virtual double getInstTraceMWS(){
    	return missQueue->getInstTraceMWS();
    }

    virtual double getInstTraceMLP(){
    	return missQueue->getInstTraceMLP();
    }

    virtual double getAvgBurstSize(){
    	return missQueue->getAvgBurstSize();
    }

    virtual int getResponsesWhileStalled(){
    	return missQueue->getResponsesWhileStalled();
    }

    virtual int getInstTraceRespWhileStalled(){
    	return missQueue->getInstTraceRespWhileStalled();
    }

    virtual void setNumMSHRs(int newMSHRCount){
    	missQueue->setNumMSHRs(newMSHRCount);
    }

    virtual std::vector<MSHROccupancy>* getOccupancyList(){
    	return missQueue->getOccupancyList();
    }

    virtual void clearOccupancyList(){
    	missQueue->clearOccupancyList();
    }

    virtual void enableOccupancyList(){
    	missQueue->enableOccupancyList();
    }

    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);

    virtual RateMeasurement getMissRate();

	virtual std::vector<int> lookaheadCachePartitioning(std::vector<std::vector<double> > utilities);

#ifdef CACHE_DEBUG
    virtual void removePendingRequest(Addr address, MemReqPtr& req);
    virtual void addPendingRequest(Addr address, MemReqPtr& req);
#endif

};

#endif // __CACHE_HH__
