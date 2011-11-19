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
 * Cache definitions.
 */

#include <assert.h>
#include <math.h>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#include "sim/host.hh"
#include "base/misc.hh"
#include "cpu/smt.hh"

#include "mem/cache/cache.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/miss/mshr.hh"
#include "mem/cache/prefetch/prefetcher.hh"

#include "mem/bus/bus.hh"

#include "mem/bus/slave_interface.hh"
#include "mem/memory_interface.hh"
#include "mem/bus/master_interface.hh"

#include "mem/mem_debug.hh"

#include "sim/sim_events.hh" // for SimExitEvent

#define CACHE_PROFILE_INTERVAL 100000

using namespace std;

template<class TagStore, class Buffering, class Coherence>
Cache<TagStore,Buffering,Coherence>::
Cache(const std::string &_name, HierParams *hier_params,
      Cache<TagStore,Buffering,Coherence>::Params &params)
    : BaseCache(_name, hier_params, params.baseParams, params.isShared, params.directoryCoherence != NULL, params.isReadOnly),
      prefetchAccess(params.prefetchAccess),
      tags(params.tags), missQueue(params.missQueue),
      coherence(params.coherence), prefetcher(params.prefetcher),
      doCopy(params.doCopy), blockOnCopy(params.blockOnCopy)
{

    // needs these for the memory addresing bug workaround
    cpuCount = params.cpu_count;
    assert(cpuCount > 0);
    isMultiprogWorkload = params.multiprog_workload;

    idIsSet = false;
    cacheCpuID = params.cpu_id;
    memoryAddressOffset = params.memoryAddressOffset;
    memoryAddressParts = params.memoryAddressParts;

    simulateContention = params.simulateContention;
//    useStaticPartInWarmup = params.useStaticPartInWarmup;

    doModuloAddressing = params.doModuloBankAddr;
    bankID = params.bankID;
    bankCount = params.bankCount;

    interferenceManager = params.interferenceManager;
    writebackOwnerPolicy = params.wbPolicy;

    associativity = tags->getAssoc();

    useAggregateMLPEstimator = params.useAggMLPEstimator;

    cacheInterference = params.cacheInterference;

//    if(params.useMTPPartitioning)
//    {
//        assert(isShared);
//        assert(params.useUniformPartitioning);
//        assert(!shadowTags.empty());
//
//        mtp = new MultipleTimeSharingParititions(this, tags->getAssoc(), params.mtpEpochSize, shadowTags, params.detailedSimStartTick);
//    }
//    else mtp = NULL;

    if(params.partitioning != NULL){
    	assert(isShared);
    	params.partitioning->registerCache(this, bankID);
    }

    if(params.isShared){
        profileFileName = name() + "CapacityProfile.txt";
        ofstream file(profileFileName.c_str());
        file << "Tick";
        for(int i=0;i<params.cpu_count;i++) file << ";CPU " << i;
        file << ";Not touched\n";
        file.flush();
        file.close();

        profileEvent = new CacheProfileEvent(this);
        if(params.detailedSimStartTick > 0){
            profileEvent->schedule(params.detailedSimStartTick);
        }
        else{
            profileEvent->schedule(CACHE_PROFILE_INTERVAL);
        }
    }

    overlapEstimator = params.overlapEstimator;

    detailedSimulationStartTick = params.detailedSimStartTick;

    useAdaptiveMHA = false;
    if (params.in == NULL) {
    	topLevelCache = true;
    	if(params.adaptiveMHA != NULL && !params.isShared){
    		params.adaptiveMHA->registerCache(cacheCpuID, !params.isReadOnly, this, false);
    		useAdaptiveMHA = true;
    		adaptiveMHA = params.adaptiveMHA;
    	}
    	else if(params.adaptiveMHA != NULL && params.isShared){
    		//register for cache capacity measurements
    		params.adaptiveMHA->registerCache(-1, !params.isReadOnly, this, true);
    		useAdaptiveMHA = false;
    		adaptiveMHA = params.adaptiveMHA;
    	}
    	else{
    		adaptiveMHA = NULL;
    	}
    }

    directoryProtocol = NULL;
    if(params.directoryCoherence != NULL){
        directoryProtocol = params.directoryCoherence;
        directoryProtocol->setCpuCount(cpuCount, params.cpu_id);
    }

    /* CPU ID sanity checks */
    if(params.directoryCoherence != NULL){
        if(!params.isShared && !params.isReadOnly){
            // This is an L1 data cache
            if(cacheCpuID == -1){
                fatal("The CPU ID of this L1 cache must be set");
            }
        }
        else{
            if(cacheCpuID != -1){
                fatal("The CPU ID is set for a cache that shouldn't have a CPU ID");
            }
        }
    }

    if(params.out == NULL){
        /* no outgoing bus set, assume crossbar */
        tags->setCache(this, params.outInterconnect->width, params.outInterconnect->clock);
    }
    else{
        tags->setCache(this, params.out->width, params.out->clockRate);
    }

    tags->setPrefetcher(prefetcher);
    missQueue->setCache(this);
    missQueue->setPrefetcher(prefetcher);
    coherence->setCache(this);
    prefetcher->setCache(this);
    prefetcher->setTags(tags);
    prefetcher->setBuffer(missQueue);
    invalidateReq = new MemReq;
    invalidateReq->cmd = Invalidate;

    localName = _name;

    // TODO: if cache hit stats are needed, impl exit callback
    // callback should call dumpHitStats()

    if(adaptiveMHA != NULL && !isShared && !params.isReadOnly){
    	interferenceManager->registerLastLevelPrivateCache(this, cacheCpuID, missQueue->getNumMSHRs());
    	if(params.missBandwidthPolicy != NULL){
    		params.missBandwidthPolicy->registerCache(this, cacheCpuID, missQueue->getNumMSHRs());
    	}
    }

    if(isShared && interferenceManager != NULL){
    	interferenceManager->registerSharedCache(this);
    }

    if(isShared && params.missBandwidthPolicy != NULL){
    	params.missBandwidthPolicy->registerSharedCache(this);
    }

    accessSample = 0;
    missSample = 0;

    if(!params.staticQuotas.empty()){
    	if(!isShared) fatal("Cache partitioning only makes sense for shared caches");
    	tags->setCachePartition(params.staticQuotas);
    	tags->enablePartitioning();
    }
}

template<class TagStore, class Buffering, class Coherence>
Cache<TagStore,Buffering,Coherence>::~Cache(){

}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::regStats()
{
    BaseCache::regStats();
    tags->regStats(name());
    missQueue->regStats(name());
    coherence->regStats(name());
    prefetcher->regStats(name());
}

template<class TagStore, class Buffering, class Coherence>
MemAccessResult
Cache<TagStore,Buffering,Coherence>::access(MemReqPtr &req)
{

	MemDebug::cacheAccess(req);
	BlkType *blk = NULL;
	MemReqList writebacks;
	int size = blkSize;
	int lat = hitLatency;

	if(overlapEstimator != NULL) overlapEstimator->issuedMemoryRequest(req);

	if(useDirectory && doData()){
		fatal("Directory protocol does not handle data transfers");
	}

	if(!isShared){
		setSenderID(req);
	}
	else{
		if(cpuCount > 1) assert(req->adaptiveMHASenderID >= 0 && req->adaptiveMHASenderID < cpuCount);
	}

	if(req->cmd == Soft_Prefetch){
		req->isSWPrefetch = true;
	}

	// update hit statistics
	// NOTE: this must be done here to avoid errors from waiting til after the block is moved to the MRU position
	tags->updateSetHitStats(req);

	if (req->cmd == Copy) {
		if(useDirectory) fatal("directory copy not implemented");
		startCopy(req);

		/**
		 * @todo What return value makes sense on a copy.
		 */
		return MA_CACHE_MISS;
	}
	if (prefetchAccess) {
		//We are determining prefetches on access stream, call prefetcher
		prefetcher->handleMiss(req, curTick);
	}
	if (!req->isUncacheable()) {
		if (req->cmd.isInvalidate() && !req->cmd.isRead()
				&& !req->cmd.isWrite()) {
			//Upgrade or Invalidate
			//Look into what happens if two slave caches on bus
			DPRINTF(Cache, "%s %d %x ? blk_addr: %x\n", req->cmd.toString(),
					req->asid, req->paddr & (((ULL(1))<<48)-1),
					req->paddr & ~((Addr)blkSize - 1));

			//@todo Should this return latency have the hit latency in it?
			respond(req,curTick+lat);
			req->flags |= SATISFIED;
			return MA_HIT;
		}
		blk = tags->handleAccess(req, lat, writebacks);
	} else {
		size = req->size;
	}
	// If this is a block size write/hint (WH64) allocate the block here
	// if the coherence protocol allows it.
	/** @todo make the fast write alloc (wh64) work with coherence. */
	/** @todo Do we want to do fast writes for writebacks as well? */
	if (!blk && req->size >= blkSize && coherence->allowFastWrites() && !useDirectory &&
			(req->cmd == Write || req->cmd == WriteInvalidate) ) {

		if(useDirectory) fatal("Doing fast allocate with directory protocol");

		// not outstanding misses, can do this
		MSHR* outstanding_miss = missQueue->findMSHR(req->paddr, req->asid);
		if (req->cmd ==WriteInvalidate || !outstanding_miss) {
			if (outstanding_miss) {
				warn("WriteInv doing a fastallocate"
						"with an outstanding miss to the same address\n");
			}

			assert(!isShared);
			blk = tags->handleFill(NULL, req, BlkValid | BlkWritable,
					writebacks);
			++fastWrites;
		}
	}
	while (!writebacks.empty()) {

		if(useDirectory && !isShared){
			fatal("directory write back handling not implemented (Cache access() ) ");
		}

		missQueue->doWriteback(writebacks.front());
		writebacks.pop_front();
	}
	DPRINTF(Cache, "%s %d %x %s blk_addr: %x pc %x\n", req->cmd.toString(),
			req->asid, req->paddr & (((ULL(1))<<48)-1), (blk) ? "hit" : "miss",
					req->paddr & ~((Addr)blkSize - 1), req->pc);

	if(isDirectoryAndL2Cache()){

		if(directoryProtocol->doDirectoryAccess(req)){
			// the return value does not matter
			return MA_CACHE_MISS;
		}
	}

	if(isShared){
		assert(req->adaptiveMHASenderID != -1);
		accessesPerCPU[req->adaptiveMHASenderID]++;
	}
	else{
		accessesPerCPU[cacheCpuID]++;
	}

	if(isShared && req->cmd == Read){
		interferenceManager->addLatency(InterferenceManager::CacheCapacity, req, hitLatency);
	}

	//shadow tag access
	if(cacheInterference != NULL){
		cacheInterference->access(req, !blk, hitLatency, detailedSimulationStartTick);
	}

	accessSample++;
	if (blk) {

		if(isDirectoryAndL2Cache()){
			// Make sure no state has been allocated by mistake
			assert(blk->presentFlags == NULL);
			assert(blk->owner == -1);
		}

		//**************** L1 Cache Hit **********************//
		if(isDirectoryAndL1DataCache()){
			if(directoryProtocol->doL1DirectoryAccess(req, blk)){
				return MA_CACHE_MISS;
			}
		}

		// Hit
		hits[req->cmd.toIndex()][req->thread_num]++;
		// clear dirty bit if write through
		if (!req->cmd.isNoResponse()){
			respond(req, curTick+lat);
		}
		else if(simulateContention && curTick >= detailedSimulationStartTick) updateInterference(req);

		return MA_HIT;
	}

	missSample++;

	// Miss
	if (!req->isUncacheable()) {
		misses[req->cmd.toIndex()][req->thread_num]++;
		/** @todo Move miss count code into BaseCache */
		if (missCount) {
			--missCount;
			if (missCount == 0)
				new SimExitEvent("A cache reached the maximum miss count");
		}
	}

	if(isDirectoryAndL1DataCache()){
		MemAccessResult res = directoryProtocol->handleL1DirectoryMiss(req);
		if(res != BA_NO_RESULT){
			return res;
		}
	}

	if(isShared){
		assert(req->adaptiveMHASenderID != -1);
		assert(req->adaptiveMHASenderID >= 0 && req->adaptiveMHASenderID < cpuCount);
		missesPerCPU[req->adaptiveMHASenderID]++;

		interferenceManager->addCacheResult(req);
	}
	else{
		missesPerCPU[cacheCpuID]++;
	}

	if(simulateContention && curTick >= detailedSimulationStartTick){
		Tick issueAt = updateAndStoreInterference(req, curTick + hitLatency);
		req->finishedInCacheAt = issueAt;
		missQueue->handleMiss(req, size, issueAt);
	}
	else{
		req->finishedInCacheAt = curTick + hitLatency;
		missQueue->handleMiss(req, size, curTick + hitLatency);
	}

	return MA_CACHE_MISS;
}

template<class TagStore, class Buffering, class Coherence>
RateMeasurement
Cache<TagStore,Buffering,Coherence>::getMissRate(){
	RateMeasurement rateMeasurement(missSample, accessSample);
	missSample = 0;
	accessSample = 0;
	return rateMeasurement;
}

template<class TagStore, class Buffering, class Coherence>
MemReqPtr
Cache<TagStore,Buffering,Coherence>::getMemReq()
{

	if(directoryProtocol != NULL && !directoryProtocol->directoryRequests.empty()){

		// the request is removed in sendResult()
		MemReqPtr dirReq = directoryProtocol->directoryRequests.front();
		assert(dirReq->time <= curTick);
		return dirReq;
	}


	MemReqPtr req = missQueue->getMemReq();

	if (req) {

		if (!req->isUncacheable()) {

			if (req->cmd == Hard_Prefetch) misses[Hard_Prefetch][req->thread_num]++;
			BlkType *blk = tags->findBlock(req);
			MemCmd cmd = coherence->getBusCmd(req->cmd,
					(blk)? blk->status : 0);

			// Inform L2 if the cache intends to write this line or not
			bool setWrite = false;
			req->oldCmd = req->cmd; //remember the old command
			if(isDirectoryAndL1DataCache() && req->cmd == Write){
				setWrite = true;
			}


			missQueue->setBusCmd(req, cmd);

			if(setWrite){
				req->writeMiss = true;
			}
		}
	}


	assert(!doMasterRequest() || missQueue->havePending());
	assert(!req || req->time <= curTick);
	if(req && !isShared) assert(req->adaptiveMHASenderID != -1);


	return req;
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::sendResult(MemReqPtr &req, bool success)
{

    if(req && (req->cmd.isDirectoryMessage() || req->ownerWroteBack)){
        if(!success){
            fatal("Unsuccsessfull directory writeback not implemented");
        }
        directoryProtocol->directoryRequests.pop_front();
        return;
    }

    if (success) {

	missQueue->markInService(req);
	  //Temp Hack for UPGRADES
	  if (req->cmd == Upgrade) {
	      handleResponse(req);
	  }
    } else if (req && !req->isUncacheable()) {
	missQueue->restoreOrigCmd(req);
    }
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::handleResponse(MemReqPtr &req)
{

	DPRINTF(Cache, "Response recieved: %s %x blk_addr: %x pc %x\n",
			req->cmd.toString(),
			req->paddr & (((ULL(1))<<48)-1),
			req->paddr & ~((Addr)blkSize - 1),
			req->pc);

	if(isDirectoryAndL1DataCache()){

		if(directoryProtocol->handleDirectoryResponse(req, tags)){
			return;
		}
	}

	if(isShared && req->cmd == Read){
		interferenceManager->addLatency(InterferenceManager::CacheCapacity, req, hitLatency);
	}

	MemReqPtr copy_request;
	BlkType *blk = NULL;

	if (req->mshr || req->cmd == DirOwnerTransfer || req->cmd == DirRedirectRead) {

		MemDebug::cacheResponse(req);
		DPRINTF(Cache, "Handling reponse to %x, blk addr: %x\n",req->paddr,
				req->paddr & (((ULL(1))<<48)-1));


		if ((req->isCacheFill() && !req->isNoAllocate()) || req->cmd == DirOwnerTransfer || req->cmd == DirRedirectRead) {
			blk = tags->findBlock(req);

			MemReqList writebacks;

			CacheBlk::State old_state = (blk) ? blk->status : 0;

			if(req->cmd != DirOwnerTransfer && req->mshr != NULL){
				blk = tags->handleFill(blk, req->mshr,
						coherence->getNewState(req,old_state),
						writebacks,
						req);
			}
			else{
				assert(req->cmd == DirOwnerTransfer || req->cmd == DirRedirectRead);
			}


			// check that we don't write to a block we don't own
			if(blk != NULL){
				if (req->prefetched) {
					blk->status = blk->status | 0x20;
				}
				if(req->writeMiss){
					assert(blk->dirState == DirOwnedExGR
							|| blk->dirState == DirOwnedNonExGR
							|| req->owner == cacheCpuID);
				}
			}
			// The shared cache case is handled cache tags, because we need to know
			// which cache should become the new owner

			bool doNotHandleResponse = false;
			if(isDirectoryAndL1DataCache()){
				doNotHandleResponse = directoryProtocol->handleDirectoryFill(req, blk, writebacks, tags);
			}

			//shadow replacement
			if(cacheInterference != NULL){
				cacheInterference->handleResponse(req, writebacks, this);
			}

			while (!writebacks.empty()) {

				if (writebacks.front()->cmd == Copy) {
					copy_request = writebacks.front();
				}
				else if(directoryProtocol ? directoryProtocol->doDirectoryWriteback(writebacks.front()) : false){
					// actions are handled in the check
				}
				else {

					if(!isShared){
						setSenderID(writebacks.front());
					}
					else{


							switch(writebackOwnerPolicy){
							case BaseCache::WB_POLICY_OWNER:
								assert(blk->prevOrigRequestingCpuID != -1);
								writebacks.front()->adaptiveMHASenderID = blk->prevOrigRequestingCpuID;
								writebacks.front()->nfqWBID = blk->prevOrigRequestingCpuID;
								break;
							case BaseCache::WB_POLICY_REPLACER:
								writebacks.front()->adaptiveMHASenderID = req->adaptiveMHASenderID;
								writebacks.front()->nfqWBID = req->adaptiveMHASenderID;
								break;
							default:
								writebacks.front()->adaptiveMHASenderID = -1;
								writebacks.front()->nfqWBID = -1;
								break;
							}

							writebacks.front()->memCtrlGeneratingReadSeqNum = req->memCtrlPrivateSeqNum;
							writebacks.front()->memCtrlGenReadInterference = req->interferenceBreakdown[MEM_BUS_QUEUE_LAT] + req->interferenceBreakdown[MEM_BUS_SERVICE_LAT] + req->interferenceBreakdown[MEM_BUS_ENTRY_LAT];
							writebacks.front()->memCtrlWbGenBy = req->paddr;

					}


					missQueue->doWriteback(writebacks.front());
				}
				writebacks.pop_front();
			}

			if(doNotHandleResponse) return;
		}
		else{
			if(cacheInterference != NULL) fatal("A response may not be represented in the shadow tags! (1)");
		}

		if (copy_request) {
			// The mshr is handled in handleCopy
			handleCopy(copy_request, req->paddr, blk, req->mshr);
		} else {
			assert(req->mshr != NULL);
			missQueue->handleResponse(req, curTick + hitLatency);
		}
	}
	else{
		if(cacheInterference != NULL) fatal("A response may not be represented in the shadow tags! (2)");
	}

	if(overlapEstimator != NULL) overlapEstimator->completedMemoryRequest(req, curTick+hitLatency);

}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::pseudoFill(Addr addr, int asid)
{
    // Need to temporarily move this blk into MSHRs
    MSHR *mshr = missQueue->allocateTargetList(addr, asid);
    int lat;
    MemReqList dummy;
    // Read the data into the mshr
    BlkType *blk = tags->handleAccess(mshr->req, lat, dummy, false);
    assert(dummy.empty());
    assert(mshr->req->isSatisfied());
    // can overload order since it isn't used on non pending blocks
    mshr->order = blk->status;
    // temporarily remove the block from the cache.
    tags->invalidateBlk(addr, asid);
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::pseudoFill(MSHR *mshr)
{
    // Need to temporarily move this blk into MSHRs
    assert(mshr->req->cmd == Read);
    int lat;
    MemReqList dummy;
    // Read the data into the mshr
    BlkType *blk = tags->handleAccess(mshr->req, lat, dummy, false);
    assert(dummy.empty());
    assert(mshr->req->isSatisfied());
    // can overload order since it isn't used on non pending blocks
    mshr->order = blk->status;
    // temporarily remove the block from the cache.
    tags->invalidateBlk(mshr->req->paddr, mshr->req->asid);
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::startCopy(MemReqPtr &req)
{
    MemDebug::cacheStartCopy(req);
    bool delayed = false;
    Addr source = req->paddr;
    // Fake wh64 for additional copies
    Addr dest = req->dest & ~(blkSize - 1);
    int asid = req->asid;

    cacheCopies++;

    bool source_unaligned = source & (blkSize - 1);
    bool dest_unaligned = dest & (blkSize - 1);

    // Block addresses for unaligned accesses.
    Addr source1 = source &  ~(Addr)(blkSize - 1);
    Addr source2 = source1 + blkSize;
    Addr dest1 = dest & ~(Addr)(blkSize - 1);
    Addr dest2 = dest1 + blkSize;

    MSHR *source1_mshr = missQueue->findMSHR(source1, asid);
    MSHR *source2_mshr = missQueue->findMSHR(source2, asid);
    MSHR *dest1_mshr = missQueue->findMSHR(dest1, asid);
    MSHR *dest2_mshr = missQueue->findMSHR(dest2, asid);

    if (source1_mshr) {
	req->flags |= COPY_SOURCE1;
	missQueue->addTarget(source1_mshr, req);
    }
    if (source2_mshr && source_unaligned) {
	req->flags |= COPY_SOURCE2;
	missQueue->addTarget(source2_mshr, req);
    }
    if (dest1_mshr) {
	req->flags |= COPY_DEST1;
	missQueue->addTarget(dest1_mshr, req);
    }
    if (dest2_mshr && dest_unaligned) {
	req->flags |= COPY_DEST2;
	missQueue->addTarget(dest2_mshr, req);
    }

    BlkType *source1_blk = tags->findBlock(source1, asid);
    BlkType *source2_blk = tags->findBlock(source2, asid);
    BlkType *dest1_blk = tags->findBlock(dest1, asid);
    BlkType *dest2_blk = tags->findBlock(dest2, asid);

    if (!doCopy) {
	// Writeback sources if dirty.
	if (source1_blk && source1_blk->isModified()) {
	    MemReqPtr writeback = tags->writebackBlk(source1_blk);
	    missQueue->doWriteback(writeback);
	}
	if (source_unaligned && source2_blk && source2_blk->isModified()) {
	    MemReqPtr writeback = tags->writebackBlk(source2_blk);
	    missQueue->doWriteback(writeback);
	}

	if (dest_unaligned) {
	    // Need to writeback dirty destinations
	    if (dest1_blk && dest1_blk->isModified()) {
		MemReqPtr writeback = tags->writebackBlk(dest1_blk);
		missQueue->doWriteback(writeback);
	    }
	    if (dest2_blk && dest2_blk->isModified()) {
		MemReqPtr writeback = tags->writebackBlk(dest2_blk);
		missQueue->doWriteback(writeback);
	    }
	    // Need to invalidate both dests
	    tags->invalidateBlk(dest2, asid);
	}
	// always need to invalidate this block.
	tags->invalidateBlk(dest1, asid);

	if (source1_mshr || (source2_mshr && source_unaligned) ||
	    dest1_mshr || (dest2_mshr && dest_unaligned)) {

	    // Need to delay the copy until the outstanding requests
	    // finish.
	    delayed = true;
	    if (blockOnCopy) {
		setBlocked(Blocked_Copy);
	    }
	    if (!dest1_mshr) {
		dest1_mshr = missQueue->allocateTargetList(dest1, asid);
		dest1_mshr->req->xc = req->xc;
	    }
	    if (!dest2_mshr && dest_unaligned) {
		dest2_mshr = missQueue->allocateTargetList(dest2, asid);
		dest2_mshr->req->xc = req->xc;
	    }
	    if (!source1_mshr) {
		if (source1_blk) {
		    pseudoFill(source1, asid);
		} else {
		    source1_mshr =
			missQueue->allocateTargetList(source1, asid);
		    source1_mshr->req->xc = req->xc;
		}
	    }
	    if (!source2_mshr && source_unaligned) {
		if (source2_blk) {
		    pseudoFill(source2, asid);
		} else {
		    source2_mshr =
			missQueue->allocateTargetList(source2, asid);
		    source2_mshr->req->xc = req->xc;
		}
	    }
	} else {
	    // Forward the copy to the next level.
	    missQueue->doWriteback(req);
	}
    } else { // doCopy
	// Need to fetch sources if they aren't present.
	if (!source1_mshr && !source1_blk) {
	    req->flags |= COPY_SOURCE1;
	    source1_mshr = missQueue->fetchBlock(source1, asid, blkSize,
						 curTick, req);
	}
	if (source_unaligned && !source2_mshr && !source2_blk) {
	    req->flags |= COPY_SOURCE2;
	    source1_mshr = missQueue->fetchBlock(source2, asid, blkSize,
						 curTick, req);
	}

	if (dest_unaligned) {
	    if (!dest1_mshr && !dest1_blk) {
		req->flags |= COPY_DEST1;
		dest1_mshr = missQueue->fetchBlock(dest1, asid, blkSize,
						   curTick, req);
	    }
	    if (!dest2_mshr && !dest2_blk) {
		req->flags |= COPY_DEST2;
		dest2_mshr = missQueue->fetchBlock(dest2, asid, blkSize,
						   curTick, req);
	    }
	}

	if (source1_mshr || (source2_mshr && source_unaligned) ||
	    dest1_mshr || (dest2_mshr && dest_unaligned)) {

	    delayed = true;
	    if (blockOnCopy) {
		setBlocked(Blocked_Copy);
	    }
	    if (source1_blk) {
		pseudoFill(source1, asid);
	    }
	    if (source2_blk && source_unaligned) {
		pseudoFill(source2, asid);
	    }
	    if (dest_unaligned) {
		if (dest1_blk) {
		    pseudoFill(dest1, asid);
		}
		if (dest2_blk) {
		    pseudoFill(dest2, asid);
		}
	    } else {
		if (!dest1_mshr) {
		    tags->invalidateBlk(dest1, asid);
		    dest1_mshr = missQueue->allocateTargetList(dest1, asid);
		    dest1_mshr->req->xc = req->xc;
		}
	    }
	} else {
	    MemReqList writebacks;
	    if (source_unaligned || dest_unaligned) {
		tags->doUnalignedCopy(source, dest, asid, writebacks);
	    } else {
		tags->doCopy(source, dest, asid, writebacks);
	    }
	    while(!writebacks.empty()) {
		missQueue->doWriteback(writebacks.front());
		writebacks.pop_front();
	    }
	}
    }
    DPRINTF(Cache, "Starting copy from %x (%x)  to %x (%x) %s\n", source,
	    source1, dest, dest1,
	    (delayed)?"delayed":"successful");
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::handleCopy(MemReqPtr &req, Addr addr,
						BlkType *blk, MSHR *mshr)
{
    MemDebug::cacheHandleCopy(req);
    Addr source = req->paddr;
    // Fake wh64 for multiple copies
    Addr dest = req->dest & ~(blkSize - 1);
    int asid = req->asid;
    MemReqList writebacks;

    assert(mshr->getTarget()->cmd == Copy);

    bool source_unaligned = source & (blkSize - 1);
    bool dest_unaligned = dest & (blkSize - 1);

    // Block addresses for unaligned accesses.
    Addr source1 = source &  ~(Addr)(blkSize - 1);
    Addr source2 = source1 + blkSize;
    Addr dest1 = dest & ~(Addr)(blkSize - 1);
    Addr dest2 = dest1 + blkSize;

    DPRINTF(Cache,"Handling delayed copy from %x (%x) to %x (%x)\n\t"
	    "pending = %x, addr = %x\n", source, source1, dest, dest1,
	    req->flags & COPY_PENDING_MASK,
	    addr);

    if (!doCopy) {
	if (addr == source1) {
	    req->flags &= ~COPY_SOURCE1;
	    // pop the copy off.
	    mshr->popTarget();
	    if (blk) {
		if (blk->isModified()) {
		    // writeback block.
		    MemReqPtr writeback = tags->writebackBlk(blk);
		    missQueue->doWriteback(writeback);
		}
		pseudoFill(mshr);
	    }
	}
	if (addr == source2) {
	    req->flags &= ~COPY_SOURCE2;
	    // pop the copy off.
	    mshr->popTarget();
	    if (blk) {
		if (blk->isModified()) {
		    // writeback block.
		    MemReqPtr writeback = tags->writebackBlk(blk);
		    missQueue->doWriteback(writeback);
		}
		pseudoFill(mshr);
	    }
	}
	if (addr == dest1) {
	    req->flags &= ~COPY_DEST1;
	    if (dest_unaligned && blk && blk->isModified()) {
		// writeback block.
		MemReqPtr writeback = tags->writebackBlk(blk);
		missQueue->doWriteback(writeback);
	    }
	    tags->invalidateBlk(dest1, asid);
	    // Pop the copy request off of the list.
	    mshr->popTarget();
	    // Mark the mshr request as not statisfied to make it a targetlist
	    mshr->req->flags &= ~SATISFIED;
	}
	if (addr == dest2) {
	    assert(dest_unaligned);
	    req->flags &= ~COPY_DEST2;
	    if (blk && blk->isModified()) {
		// writeback block.
		MemReqPtr writeback = tags->writebackBlk(blk);
		missQueue->doWriteback(writeback);
	    }
	    tags->invalidateBlk(dest2, asid);
	    // Pop the copy request off of the list.
	    mshr->popTarget();
	    // Mark the mshr request as not statisfied to make it a targetlist
            mshr->req->flags &= ~SATISFIED;
	}
	if (!req->pendingCopy()) {
	    // Need to clear the satisfied flag before we forward
	    req->flags &= ~SATISFIED;
	    missQueue->doWriteback(req);
	    if (blockOnCopy) {
		clearBlocked(Blocked_Copy);
	    }

	    // handle the first source block.
	    MSHR* source1_mshr = missQueue->findMSHR(source1, asid);
	    assert(source1_mshr);
	    if (source1_mshr->req->isSatisfied()) {
		CacheBlk::State state = source1_mshr->order;
		assert(state < 32);
		assert(req->xc || !doData());
		source1_mshr->req->xc = req->xc;
		tags->pseudoFill(source1_mshr, state, writebacks);
	    }
	    if (source1_mshr->hasTargets() &&
		source1_mshr->getTarget()->cmd == Copy) {

		MemReqPtr copy_request = source1_mshr->getTarget();
		handleCopy(copy_request, source1,
			   tags->findBlock(source1, asid), source1_mshr);
	    } else {
		// Refetch if necessary, or free the MSHR.
		missQueue->handleResponse(source1_mshr->req,
					  curTick + hitLatency);
	    }

	    // Handle the second source if necessary
	    if (source_unaligned) {
		MSHR* source2_mshr = missQueue->findMSHR(source2, asid);
		assert(source2_mshr);
		if (source2_mshr->req->isSatisfied()) {
		    CacheBlk::State state = source2_mshr->order;
		    assert(state < 32);
		    assert(req->xc || !doData());
		    source2_mshr->req->xc = req->xc;
		    tags->pseudoFill(source2_mshr, state, writebacks);
		}
		if (source2_mshr->hasTargets() &&
		    source2_mshr->getTarget()->cmd == Copy) {

		    MemReqPtr copy_request = source2_mshr->getTarget();
		    handleCopy(copy_request, source2,
			       tags->findBlock(source2, asid), source2_mshr);
		} else {
		    // Refetch if necessary, or free the MSHR.
		    missQueue->handleResponse(source2_mshr->req,
					      curTick + hitLatency);
		}
	    }

	    // Restart the first destination
	    MSHR* dest1_mshr = missQueue->findMSHR(dest1, asid);
	    // Restart the target list.
	    missQueue->handleResponse(dest1_mshr->req,

				      curTick + hitLatency);

	    // Restart the second destination, if necessary
	    if (dest_unaligned) {
		MSHR* dest2_mshr = missQueue->findMSHR(dest2, asid);
		// Restart the target list.
		missQueue->handleResponse(dest2_mshr->req,
					  curTick + hitLatency);
	    }
	}
    } else { // doCopy
	if (addr == source1) {
	    req->flags &= ~COPY_SOURCE1;
	    mshr->popTarget();
	    pseudoFill(mshr);
	}
	if (addr == source2) {
	    req->flags &= ~COPY_SOURCE2;
	    mshr->popTarget();
	    pseudoFill(mshr);
	}
	if (addr == dest1) {
	    req->flags &= ~COPY_DEST1;
	    mshr->popTarget();
	    if (dest_unaligned) {
		// Store the block for later
		pseudoFill(mshr);
	    } else {
		// Mark the MSHR as a target list
		mshr->req->flags &= ~SATISFIED;
		tags->invalidateBlk(dest1, asid);
	    }
	}
	if (addr == dest2) {
	    req->flags &= ~COPY_DEST2;
	    mshr->popTarget();
	    // Store the block for later
	    pseudoFill(mshr);
	}

	if (!req->pendingCopy()) {
	    MSHR* source1_mshr = missQueue->findMSHR(source1, asid);
	    MSHR* source2_mshr = missQueue->findMSHR(source2, asid);
	    MSHR* dest1_mshr = missQueue->findMSHR(dest1, asid);
	    MSHR* dest2_mshr = missQueue->findMSHR(dest2, asid);

	    // Refill the first Source
	    CacheBlk::State state = source1_mshr->order;
	    source1_mshr->req->xc = req->xc;
	    tags->handleFill(NULL, source1_mshr->req, state, writebacks, NULL);

	    // Refill the second source, if necessary
	    if (source_unaligned) {
		state = source2_mshr->order;
		source2_mshr->req->xc = req->xc;
		tags->handleFill(NULL, source2_mshr->req, state, writebacks,
				 NULL);
	    }

	    // Refill destination blocks, if necessary
	    if (dest_unaligned) {
		state = dest1_mshr->order;
		dest1_mshr->req->xc = req->xc;
		tags->handleFill(NULL, dest1_mshr->req, state, writebacks,
				 NULL);
		state = dest2_mshr->order;
		dest2_mshr->req->xc = req->xc;
		tags->handleFill(NULL, dest2_mshr->req, state, writebacks,
				 NULL);
	    }

	    // Perform the copy
	    if (source_unaligned || dest_unaligned) {
		tags->doUnalignedCopy(source, dest, asid, writebacks);
	    } else {
		tags->doCopy(source, dest, asid, writebacks);
	    }

	    if (blockOnCopy) {
		clearBlocked(Blocked_Copy);
	    }

	    // Handle targets, if any
	    tags->handleTargets(source1_mshr, writebacks);
	    // If there is another outstanding copy, don't free
	    // the MSHR here.
	    if (source1_mshr->hasTargets() &&
		source1_mshr->getTarget()->cmd == Copy) {

		MemReqPtr copy_request = source1_mshr->getTarget();
		handleCopy(copy_request, source1,
			   tags->findBlock(source1, asid), source1_mshr);
	    } else {
		// Refetch if necessary, or free the MSHR
		missQueue->handleResponse(source1_mshr->req,
					  curTick + hitLatency);
	    }

	    if (source_unaligned) {
		tags->handleTargets(source2_mshr, writebacks);
		// If there is another outstanding copy, don't free
		// the MSHR here.
		if (source2_mshr->hasTargets() &&
		    source2_mshr->getTarget()->cmd == Copy) {

		    MemReqPtr copy_request = source2_mshr->getTarget();
		    handleCopy(copy_request, source2,
			       tags->findBlock(source2, asid), source2_mshr);
		} else {
		    // Refetch if necessary, or free the MSHR
		    missQueue->handleResponse(source2_mshr->req,
					      curTick + hitLatency);
		}
	    }

	    tags->handleTargets(dest1_mshr, writebacks);
	    // If there is another outstanding copy, don't free
	    // the MSHR here.
	    if (dest1_mshr->hasTargets() &&
		dest1_mshr->getTarget()->cmd == Copy) {

		MemReqPtr copy_request = dest1_mshr->getTarget();
		handleCopy(copy_request, dest1,
			   tags->findBlock(dest1, asid), dest1_mshr);
	    } else {
		// Refetch if necessary, or free the MSHR
		missQueue->handleResponse(dest1_mshr->req,
					  curTick + hitLatency);
	    }

	    if (dest_unaligned) {
		tags->handleTargets(dest2_mshr, writebacks);
		// If there is another outstanding copy, don't free
		// the MSHR here.
		if (dest2_mshr->hasTargets() &&
		    dest2_mshr->getTarget()->cmd == Copy) {

		    MemReqPtr copy_request = dest2_mshr->getTarget();
		    handleCopy(copy_request, dest2,
			       tags->findBlock(dest2, asid), dest2_mshr);
		} else {
		    // Refetch if necessary, or free the MSHR
		    missQueue->handleResponse(dest2_mshr->req,
					      curTick + hitLatency);
		}
	    }


	}
    }

    // Send off any writebacks
    while (!writebacks.empty()) {
	if (writebacks.front()->cmd != Copy) {
	    missQueue->doWriteback(writebacks.front());
	}
	writebacks.pop_front();
    }
}

template<class TagStore, class Buffering, class Coherence>
MemReqPtr
Cache<TagStore,Buffering,Coherence>::getCoherenceReq()
{
    return coherence->getMemReq();
}


template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::snoop(MemReqPtr &req)
{


    Addr blk_addr = req->paddr & ~(Addr(blkSize-1));
    BlkType *blk = tags->findBlock(req);
    MSHR *mshr = missQueue->findMSHR(blk_addr, req->asid);
    if (isTopLevel() && coherence->hasProtocol()) { //@todo Move this into handle bus req
	//If we find an mshr, and it is in service, we need to NACK or invalidate

	if (mshr) {
	    if (mshr->inService) {
		if ((mshr->req->cmd.isInvalidate() || !mshr->req->isCacheFill())
		    && (req->cmd != Invalidate && req->cmd != WriteInvalidate)) {
		    //If the outstanding request was an invalidate (upgrade,readex,..)
		    //Then we need to ACK the request until we get the data
		    //Also NACK if the outstanding request is not a cachefill (writeback)
		    req->flags |= NACKED_LINE;
		    return;
		}
		else {
		    //The supplier will be someone else, because we are waiting for
		    //the data.  This should cause this cache to be forced to go to
		    //the shared state, not the exclusive even though the shared line
		    //won't be asserted.  But for now we will just invlidate ourselves
		    //and allow the other cache to go into the exclusive state.
		    //@todo Make it so a read to a pending read doesn't invalidate.
		    //@todo Make it so that a read to a pending read can't be exclusive now.

		    //Set the address so find match works
		    invalidateReq->paddr = req->paddr;

		    //Append the invalidate on
		    missQueue->addTarget(mshr,invalidateReq);
		    DPRINTF(Cache, "Appending Invalidate to blk_addr: %x\n", req->paddr & (((ULL(1))<<48)-1));
		    return;
		}
	    }
	}
	//We also need to check the writeback buffers and handle those
	std::vector<MSHR *> writebacks;
	if (missQueue->findWrites(blk_addr, req->asid, writebacks)) {
	    DPRINTF(Cache, "Snoop hit in writeback to blk_addr: %x\n", req->paddr & (((ULL(1))<<48)-1));

	    //Look through writebacks for any non-uncachable writes, use that
	    for (int i=0; i<writebacks.size(); i++) {
		mshr = writebacks[i];

		if (!mshr->req->isUncacheable()) {
		    if (req->cmd.isRead()) {
			//Only Upgrades don't get here
			//Supply the data
			req->flags |= SATISFIED;

			//If we are in an exclusive protocol, make it ask again
			//to get write permissions (upgrade), signal shared
			req->flags |= SHARED_LINE;

			if (doData()) {
			    assert(req->cmd.isRead());

			    assert(req->offset < blkSize);
			    assert(req->size <= blkSize);
			    assert(req->offset + req->size <=blkSize);
			    memcpy(req->data, mshr->req->data + req->offset, req->size);
			}
			respondToSnoop(req);
		    }

		    if (req->cmd.isInvalidate()) {
			//This must be an upgrade or other cache will take ownership
			missQueue->markInService(mshr->req);
		    }
		    return;
		}
	    }
	}
    }
    CacheBlk::State new_state;
    bool satisfy = coherence->handleBusRequest(req,blk,mshr, new_state);
    if (satisfy) {
	tags->handleSnoop(blk, new_state, req);
	respondToSnoop(req);
	return;
    }
    tags->handleSnoop(blk, new_state);
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::snoopResponse(MemReqPtr &req)
{
    //Need to handle the response, if NACKED
    if (req->isNacked()) {
	//Need to mark it as not in service, and retry for bus
	assert(0); //Yeah, we saw a NACK come through

	//For now this should never get called, we return false when we see a NACK
	//instead, by doing this we allow the bus_blocked mechanism to handle the retry
	//For now it retrys in just 2 cycles, need to figure out how to change that
	//Eventually we will want to also have success come in as a parameter
	//Need to make sure that we handle the functionality that happens on successufl
	//return of the sendAddr function
    }
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::invalidateBlk(Addr addr, int asid)
{
    tags->invalidateBlk(addr,asid);
}


/**
 * @todo Fix to not assume write allocate
 */
template<class TagStore, class Buffering, class Coherence>
Tick
Cache<TagStore,Buffering,Coherence>::probe(MemReqPtr &req, bool update)
{

	fatal("cache probe got called");

	MemDebug::cacheProbe(req);
    if (!update && !doData()) {
	// Nothing to do here
	return mi->sendProbe(req,update);
    }

    MemReqList writebacks;
    int lat;
    BlkType *blk = tags->handleAccess(req, lat, writebacks, update);

    if (!blk) {
	// Need to check for outstanding misses and writes
	Addr blk_addr = req->paddr & ~(blkSize - 1);

	// There can only be one matching outstanding miss.
	MSHR* mshr = missQueue->findMSHR(blk_addr, req->asid);

	// There can be many matching outstanding writes.
	vector<MSHR*> writes;
	missQueue->findWrites(blk_addr, req->asid, writes);

	if (!update) {
	    mi->sendProbe(req, update);
	    // Check for data in MSHR and writebuffer.
	    if (mshr) {
		warn("Found outstanding miss on an non-update probe");
		MSHR::TargetList *targets = mshr->getTargetList();
		MSHR::TargetList::iterator i = targets->begin();
		MSHR::TargetList::iterator end = targets->end();
		for (; i != end; ++i) {
		    MemReqPtr target = *i;
		    // If the target contains data, and it overlaps the
		    // probed request, need to update data
		    if (target->cmd.isWrite() && target->overlaps(req)) {
			uint8_t* req_data;
			uint8_t* write_data;
			int data_size;
			if (target->paddr < req->paddr) {
			    int offset = req->paddr - target->paddr;
			    req_data = req->data;
			    write_data = target->data + offset;
			    data_size = target->size - offset;
			    assert(data_size > 0);
			    if (data_size > req->size)
				data_size = req->size;
			} else {
			    int offset = target->paddr - req->paddr;
			    req_data = req->data + offset;
			    write_data = target->data;
			    data_size = req->size - offset;
			    assert(data_size > req->size);
			    if (data_size > target->size)
				data_size = target->size;
			}

			if (req->cmd.isWrite()) {
			    memcpy(req_data, write_data, data_size);
			} else {
			    memcpy(write_data, req_data, data_size);
			}
		    }
		}
	    }
	    for (int i = 0; i < writes.size(); ++i) {
		MemReqPtr write = writes[i]->req;
		if (write->overlaps(req)) {
		    warn("Found outstanding write on an non-update probe");
		    uint8_t* req_data;
		    uint8_t* write_data;
		    int data_size;
		    if (write->paddr < req->paddr) {
			int offset = req->paddr - write->paddr;
			req_data = req->data;
			write_data = write->data + offset;
			data_size = write->size - offset;
			assert(data_size > 0);
			if (data_size > req->size)
			    data_size = req->size;
		    } else {
			int offset = write->paddr - req->paddr;
			req_data = req->data + offset;
			write_data = write->data;
			data_size = req->size - offset;
			assert(data_size > req->size);
			if (data_size > write->size)
			    data_size = write->size;
		    }

		    if (req->cmd.isWrite()) {
			memcpy(req_data, write_data, data_size);
		    } else {
			memcpy(write_data, req_data, data_size);
		    }

		}
	    }
	    return 0;
	} else {
	    // update the cache state and statistics
	    if (mshr || !writes.empty()){
		// Can't handle it, return request unsatisfied.
		return 0;
	    }
	    if (!req->isUncacheable()) {
		// Fetch the cache block to fill
		MemReqPtr busReq = new MemReq();
		busReq->paddr = blk_addr;
		busReq->size = blkSize;
		busReq->data = new uint8_t[blkSize];

		BlkType *blk = tags->findBlock(req);
		busReq->cmd = coherence->getBusCmd(req->cmd,
						   (blk)? blk->status : 0);

		busReq->asid = req->asid;
		busReq->xc = req->xc;
		busReq->thread_num = req->thread_num;
		busReq->time = curTick;

		lat = mi->sendProbe(busReq, update);

		if (!busReq->isSatisfied()) {
		    // blocked at a higher level, just return
		    return 0;
		}

		misses[req->cmd.toIndex()][req->thread_num]++;

		CacheBlk::State old_state = (blk) ? blk->status : 0;
		tags->handleFill(blk, busReq,
				 coherence->getNewState(busReq, old_state),
				 writebacks, req);
		// Handle writebacks if needed
		while (!writebacks.empty()){
		    mi->sendProbe(writebacks.front(), update);
		    writebacks.pop_front();
		}
		return lat + hitLatency;
	    } else {
		return mi->sendProbe(req,update);
	    }
	}
    } else {
	// There was a cache hit.
	// Handle writebacks if needed
	while (!writebacks.empty()){
	    mi->sendProbe(writebacks.front(), update);
	    writebacks.pop_front();
	}

	if (update) {
	    hits[req->cmd.toIndex()][req->thread_num]++;
	} else if (req->cmd.isWrite()) {
	    // Still need to change data in all locations.
	    return mi->sendProbe(req, update);
	}
	return curTick + lat;
    }
    fatal("Probe not handled.\n");
    return 0;
}

template<class TagStore, class Buffering, class Coherence>
Tick
Cache<TagStore,Buffering,Coherence>::snoopProbe(MemReqPtr &req, bool update)
{
    Addr blk_addr = req->paddr & ~(Addr(blkSize-1));
    BlkType *blk = tags->findBlock(req);
    MSHR *mshr = missQueue->findMSHR(blk_addr, req->asid);
    CacheBlk::State new_state = 0;
    bool satisfy = coherence->handleBusRequest(req,blk,mshr, new_state);
    if (satisfy) {
	tags->handleSnoop(blk, new_state, req);
	return hitLatency;
    }
    tags->handleSnoop(blk, new_state);
    return 0;
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::respond(MemReqPtr &req, Tick time)
{
    if(simulateContention && curTick >= detailedSimulationStartTick) time = updateAndStoreInterference(req, time);
    si->respond(req,time);
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::handleProfileEvent(){

    vector<int> ownedBlocks = tags->perCoreOccupancy();
    assert(ownedBlocks.size() == cpuCount + 2);

    ofstream file(profileFileName.c_str(), ofstream::app);
    file << curTick;
    for(int i=0;i<cpuCount+1;i++) file << ";" << (double) ((double) ownedBlocks[i] / (double) ownedBlocks[cpuCount+1]);
    file << "\n";
    file.flush();
    file.close();

    profileEvent->schedule(curTick + CACHE_PROFILE_INTERVAL);
}

//template<class TagStore, class Buffering, class Coherence>
//void
//Cache<TagStore,Buffering,Coherence>::handleRepartitioningEvent(){
//    assert(mtp != NULL);
//    mtp->handleRepartitioningEvent();
//}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::setCachePartition(std::vector<int> setQuotas){
    tags->setCachePartition(setQuotas);
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::enablePartitioning(){
    tags->enablePartitioning();
}

template<class TagStore, class Buffering, class Coherence>
std::map<int,int>
Cache<TagStore,Buffering,Coherence>::assignBlockingBlame(){
    assert(isShared);

    double threshold = 0.9;

    map<int,int> retmap;

    if(isBlockedNoMSHRs()){
        retmap = missQueue->assignBlockingBlame(true, false, threshold);
    }
    else if(isBlockedNoTargets()){
        retmap = missQueue->assignBlockingBlame(true, true, threshold);
    }
    else if(isBlockedNoWBBuffers()){
        retmap = missQueue->assignBlockingBlame(false, false, threshold);
    }
//     fatal("assigning blocking blame but we are not blocked");

    return retmap;
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::serialize(std::ostream &os){
	assert(cpuCount == 1);
	tags->serialize(os, name());
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::unserialize(Checkpoint *cp, const std::string &section){
	tags->unserialize(cp, section);
}

#ifdef CACHE_DEBUG
template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::removePendingRequest(Addr address, MemReqPtr& req){


    Addr blkAddr = address & ~((Addr)blkSize - 1);

    map<Addr, pair<int, Tick> >::iterator result = pendingRequests.find(blkAddr);
    assert(result != pendingRequests.end());
    if(pendingRequests[blkAddr].first <= 0){
        pendingRequests.erase(result);
        assert(pendingRequests.find(blkAddr) == pendingRequests.end());
    }
    else{
        pendingRequests[blkAddr].first--;
    }
}

template<class TagStore, class Buffering, class Coherence>
void
Cache<TagStore,Buffering,Coherence>::addPendingRequest(Addr address, MemReqPtr& req){

    Addr blkAddr = address & ~((Addr)blkSize - 1);

    if(!req->cmd.isNoResponse()){
        assert(req->cmd == Write || req->cmd == Read || req->cmd == Soft_Prefetch);
        if(pendingRequests.find(blkAddr) == pendingRequests.end()){
            pendingRequests[blkAddr] = make_pair(0, curTick);
        }
        else{
            pendingRequests[blkAddr].first++;
        }
    }
}


#endif

