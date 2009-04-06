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
 * Miss and writeback queue definitions.
 */

#include "cpu/exec_context.hh"
#include "cpu/smt.hh" //for maxThreadsPerCPU
#include "mem/cache/base_cache.hh"
#include "mem/cache/miss/miss_queue.hh"
#include "mem/cache/prefetch/base_prefetcher.hh"

//#define DO_REQUEST_TRACE

using namespace std;

// simple constructor
/**
 * @todo Remove the +16 from the write buffer constructor once we handle
 * stalling on writebacks do to compression writes.
 */
MissQueue::MissQueue(int numMSHRs, int numTargets, int write_buffers,
		     bool write_allocate, bool prefetch_miss)
    : mq(numMSHRs, 4), wb(write_buffers,numMSHRs+1000), numMSHR(numMSHRs),
      numTarget(numTargets), writeBuffers(write_buffers),
      writeAllocate(write_allocate), order(0), prefetchMiss(prefetch_miss)
{
    noTargetMSHR = NULL;

    mqWasPrevQueue = false;
    changeQueue = false;
    prevTime = 0;

    if(numTargets == 1){
        fatal("In an explicitly addressed MSHR, the number of targets must be 2 or larger.");
    }
}

void
MissQueue::regStats(const string &name)
{
    using namespace Stats;

    writebacks
	.init(maxThreadsPerCPU)
	.name(name + ".writebacks")
	.desc("number of writebacks")
	.flags(total)
	;

    // MSHR hit statistics
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
    	MemCmd cmd = (MemCmdEnum)access_idx;
    	const string &cstr = cmd.toString();

    	mshr_hits[access_idx]
    	          .init(maxThreadsPerCPU)
    	          .name(name + "." + cstr + "_mshr_hits")
    	          .desc("number of " + cstr + " MSHR hits")
    	          .flags(total | nozero | nonan)
    	          ;
    }

    demandMshrHits
	.name(name + ".demand_mshr_hits")
	.desc("number of demand (read+write) MSHR hits")
	.flags(total)
	;
    demandMshrHits = mshr_hits[Read] + mshr_hits[Write];

    overallMshrHits
	.name(name + ".overall_mshr_hits")
	.desc("number of overall MSHR hits")
	.flags(total)
	;
    overallMshrHits = demandMshrHits + mshr_hits[Soft_Prefetch] +
	mshr_hits[Hard_Prefetch];

    // MSHR miss statistics
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
    	MemCmd cmd = (MemCmdEnum)access_idx;
    	const string &cstr = cmd.toString();

    	mshr_misses[access_idx]
    	            .init(maxThreadsPerCPU)
    	            .name(name + "." + cstr + "_mshr_misses")
    	            .desc("number of " + cstr + " MSHR misses")
    	            .flags(total | nozero | nonan)
    	            ;
    }

    demandMshrMisses
	.name(name + ".demand_mshr_misses")
	.desc("number of demand (read+write) MSHR misses")
	.flags(total)
	;
    demandMshrMisses = mshr_misses[Read] + mshr_misses[Write];

    overallMshrMisses
	.name(name + ".overall_mshr_misses")
	.desc("number of overall MSHR misses")
	.flags(total)
	;
    overallMshrMisses = demandMshrMisses + mshr_misses[Soft_Prefetch] +
	mshr_misses[Hard_Prefetch];

    // MSHR miss latency statistics
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	mshr_miss_latency[access_idx]
	    .init(maxThreadsPerCPU)
	    .name(name + "." + cstr + "_mshr_miss_latency")
	    .desc("number of " + cstr + " MSHR miss cycles")
	    .flags(total | nozero | nonan)
	    ;
    }

    demandMshrMissLatency
	.name(name + ".demand_mshr_miss_latency")
	.desc("number of demand (read+write) MSHR miss cycles")
	.flags(total)
	;
    demandMshrMissLatency = mshr_miss_latency[Read] + mshr_miss_latency[Write];

    overallMshrMissLatency
	.name(name + ".overall_mshr_miss_latency")
	.desc("number of overall MSHR miss cycles")
	.flags(total)
	;
    overallMshrMissLatency = demandMshrMissLatency +
	mshr_miss_latency[Soft_Prefetch] + mshr_miss_latency[Hard_Prefetch];

    // MSHR uncacheable statistics
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	mshr_uncacheable[access_idx]
	    .init(maxThreadsPerCPU)
	    .name(name + "." + cstr + "_mshr_uncacheable")
	    .desc("number of " + cstr + " MSHR uncacheable")
	    .flags(total | nozero | nonan)
	    ;
    }

    overallMshrUncacheable
	.name(name + ".overall_mshr_uncacheable_misses")
	.desc("number of overall MSHR uncacheable misses")
	.flags(total)
	;
    overallMshrUncacheable = mshr_uncacheable[Read] + mshr_uncacheable[Write]
	+ mshr_uncacheable[Soft_Prefetch] + mshr_uncacheable[Hard_Prefetch];

    // MSHR miss latency statistics
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	mshr_uncacheable_lat[access_idx]
	    .init(maxThreadsPerCPU)
	    .name(name + "." + cstr + "_mshr_uncacheable_latency")
	    .desc("number of " + cstr + " MSHR uncacheable cycles")
	    .flags(total | nozero | nonan)
	    ;
    }

    overallMshrUncacheableLatency
	.name(name + ".overall_mshr_uncacheable_latency")
	.desc("number of overall MSHR uncacheable cycles")
	.flags(total)
	;
    overallMshrUncacheableLatency = mshr_uncacheable_lat[Read]
	+ mshr_uncacheable_lat[Write] + mshr_uncacheable_lat[Soft_Prefetch]
	+ mshr_uncacheable_lat[Hard_Prefetch];

#if 0
    // MSHR access formulas
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	mshrAccesses[access_idx]
	    .name(name + "." + cstr + "_mshr_accesses")
	    .desc("number of " + cstr + " mshr accesses(hits+misses)")
	    .flags(total | nozero | nonan)
	    ;
	mshrAccesses[access_idx] =
	    mshr_hits[access_idx] + mshr_misses[access_idx]
	    + mshr_uncacheable[access_idx];
    }

    demandMshrAccesses
	.name(name + ".demand_mshr_accesses")
	.desc("number of demand (read+write) mshr accesses")
	.flags(total | nozero | nonan)
	;
    demandMshrAccesses = demandMshrHits + demandMshrMisses;

    overallMshrAccesses
	.name(name + ".overall_mshr_accesses")
	.desc("number of overall (read+write) mshr accesses")
	.flags(total | nozero | nonan)
	;
    overallMshrAccesses = overallMshrHits + overallMshrMisses
	+ overallMshrUncacheable;
#endif

    // MSHR miss rate formulas
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
    	MemCmd cmd = (MemCmdEnum)access_idx;
    	const string &cstr = cmd.toString();

    	mshrMissRate[access_idx]
    	             .name(name + "." + cstr + "_mshr_miss_rate")
    	             .desc("mshr miss rate for " + cstr + " accesses")
    	             .flags(total | nozero | nonan)
    	             ;

    	mshrMissRate[access_idx] =
    		mshr_misses[access_idx] / cache->accesses[access_idx];
    }

    demandMshrMissRate
	.name(name + ".demand_mshr_miss_rate")
	.desc("mshr miss rate for demand accesses")
	.flags(total)
	;
    demandMshrMissRate = demandMshrMisses / cache->demandAccesses;

    overallMshrMissRate
	.name(name + ".overall_mshr_miss_rate")
	.desc("mshr miss rate for overall accesses")
	.flags(total)
	;
    overallMshrMissRate = overallMshrMisses / cache->overallAccesses;

    // mshrMiss latency formulas
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	avgMshrMissLatency[access_idx]
	    .name(name + "." + cstr + "_avg_mshr_miss_latency")
	    .desc("average " + cstr + " mshr miss latency")
	    .flags(total | nozero | nonan)
	    ;

	avgMshrMissLatency[access_idx] =
	    mshr_miss_latency[access_idx] / mshr_misses[access_idx];
    }

    demandAvgMshrMissLatency
	.name(name + ".demand_avg_mshr_miss_latency")
	.desc("average overall mshr miss latency")
	.flags(total)
	;
    demandAvgMshrMissLatency = demandMshrMissLatency / demandMshrMisses;

    overallAvgMshrMissLatency
	.name(name + ".overall_avg_mshr_miss_latency")
	.desc("average overall mshr miss latency")
	.flags(total)
	;
    overallAvgMshrMissLatency = overallMshrMissLatency / overallMshrMisses;

    // mshrUncacheable latency formulas
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	avgMshrUncacheableLatency[access_idx]
	    .name(name + "." + cstr + "_avg_mshr_uncacheable_latency")
	    .desc("average " + cstr + " mshr uncacheable latency")
	    .flags(total | nozero | nonan)
	    ;

	avgMshrUncacheableLatency[access_idx] =
	    mshr_uncacheable_lat[access_idx] / mshr_uncacheable[access_idx];
    }

    overallAvgMshrUncacheableLatency
	.name(name + ".overall_avg_mshr_uncacheable_latency")
	.desc("average overall mshr uncacheable latency")
	.flags(total)
	;
    overallAvgMshrUncacheableLatency = overallMshrUncacheableLatency / overallMshrUncacheable;

    mshr_cap_events
	.init(maxThreadsPerCPU)
	.name(name + ".mshr_cap_events")
	.desc("number of times MSHR cap was activated")
	.flags(total)
	;

    //software prefetching stats
    soft_prefetch_mshr_full
	.init(maxThreadsPerCPU)
	.name(name + ".soft_prefetch_mshr_full")
	.desc("number of mshr full events for SW prefetching instrutions")
	.flags(total)
	;

    mshr_no_allocate_misses
	.name(name +".no_allocate_misses")
	.desc("Number of misses that were no-allocate")
	;

    sum_roundtrip_latency
        .name(name +".sum_roundtrip_latency")
        .desc("Total latency experienced by requests sent from this cache")
        ;
    num_roundtrip_responses
        .name(name +".num_roundtrip_responses")
        .desc("Number of responses reflected in the roundtrip latency")
        ;

    avg_roundtrip_latency
        .name(name +".avg_roundtrip_latency")
        .desc("Average latency experienced by requests sent from this cache")
        ;

    avg_roundtrip_latency = sum_roundtrip_latency / num_roundtrip_responses;

    sum_roundtrip_interference
        .name(name +".sum_roundtrip_interference")
        .desc("Total amount of interference experienced by requests sent from this cache")
        ;

    avg_roundtrip_interference
        .name(name +".avg_roundtrip_interference")
        .desc("Average interference experienced by requests sent from this cache")
        ;

    avg_roundtrip_interference = sum_roundtrip_interference / num_roundtrip_responses;

    interconnect_entry_interference
      .name(name +".sum_ic_entry_interference")
      .desc("Aggregate interconnect entry interference")
      ;

    interconnect_transfer_interference
      .name(name +".sum_ic_transfer_interference")
      .desc("Aggregate interconnect transfer interference")
      ;

    interconnect_delivery_interference
      .name(name +".sum_ic_delivery_interference")
      .desc("Aggregate interconnect delivery interference")
      ;

    bus_entry_interference
      .name(name +".sum_bus_entry_interference")
      .desc("Aggregate bus entry interference")
      ;

    bus_queue_interference
      .name(name +".sum_bus_queue_interference")
      .desc("Aggregate bus queue interference")
      ;

    bus_service_interference
        .name(name +".sum_bus_service_interference")
        .desc("Aggregate bus service interference")
        ;

    cache_capacity_interference
		.name(name +".sum_cache_capacity_interference")
		.desc("Aggregate cache capacity interference")
		;

    avg_interconnect_entry_interference
      .name(name +".avg_ic_entry_interference")
      .desc("Average interconnect entry interference")
      ;

    avg_interconnect_transfer_interference
      .name(name +".avg_ic_transfer_interference")
      .desc("Average interconnect transfer interference")
      ;

    avg_interconnect_delivery_interference
      .name(name +".avg_ic_delivery_interference")
      .desc("Average interconnect delivery interference")
      ;

    avg_bus_entry_interference
      .name(name +".avg_bus_entry_interference")
      .desc("Average bus entry interference")
      ;

    avg_bus_queue_interference
      .name(name +".avg_bus_queue_interference")
      .desc("Average bus queue interference")
      ;

    avg_bus_service_interference
        .name(name +".avg_bus_service_interference")
        .desc("Average bus service interference")
        ;

    avg_cache_capacity_interference
		.name(name +".avg_cache_capacity_interference")
		.desc("Average cache capacity interference")
		;

    avg_interconnect_entry_interference = interconnect_entry_interference / num_roundtrip_responses;
    avg_interconnect_transfer_interference = interconnect_transfer_interference / num_roundtrip_responses;
    avg_interconnect_delivery_interference = interconnect_delivery_interference / num_roundtrip_responses;
    avg_bus_entry_interference = bus_entry_interference / num_roundtrip_responses;
    avg_bus_queue_interference = bus_queue_interference / num_roundtrip_responses;
    avg_bus_service_interference = bus_service_interference / num_roundtrip_responses;
    avg_cache_capacity_interference = cache_capacity_interference / num_roundtrip_responses;

    interconnect_entry_latency
      .name(name +".sum_ic_entry_latency")
      .desc("Aggregate interconnect entry latency")
      ;

    interconnect_transfer_latency
      .name(name +".sum_ic_transfer_latency")
      .desc("Aggregate interconnect transfer latency")
      ;

    interconnect_delivery_latency
      .name(name +".sum_ic_delivery_latency")
      .desc("Aggregate interconnect delivery latency")
      ;

    bus_entry_latency
      .name(name +".sum_bus_entry_latency")
      .desc("Aggregate bus entry latency")
      ;

    bus_queue_latency
      .name(name +".sum_bus_queue_latency")
      .desc("Aggregate bus queue latency")
      ;

    bus_service_latency
        .name(name +".sum_bus_service_latency")
        .desc("Aggregate bus service latency")
        ;

    avg_interconnect_entry_latency
      .name(name +".avg_ic_entry_latency")
      .desc("Average interconnect entry latency")
      ;

    avg_interconnect_transfer_latency
      .name(name +".avg_ic_transfer_latency")
      .desc("Average interconnect transfer latency")
      ;

    avg_interconnect_delivery_latency
      .name(name +".avg_ic_delivery_latency")
      .desc("Average interconnect delivery latency")
      ;

    avg_bus_entry_latency
      .name(name +".avg_bus_entry_latency")
      .desc("Average bus entry latency")
      ;

    avg_bus_queue_latency
      .name(name +".avg_bus_queue_latency")
      .desc("Average bus queue latency")
      ;

    avg_bus_service_latency
        .name(name +".avg_bus_service_latency")
        .desc("Average bus service latency")
        ;

    avg_interconnect_entry_latency = interconnect_entry_latency / num_roundtrip_responses;
    avg_interconnect_transfer_latency = interconnect_transfer_latency / num_roundtrip_responses;
    avg_interconnect_delivery_latency = interconnect_delivery_latency /num_roundtrip_responses;
    avg_bus_entry_latency = bus_entry_latency / num_roundtrip_responses;
    avg_bus_queue_latency = bus_queue_latency / num_roundtrip_responses;
    avg_bus_service_latency = bus_service_latency / num_roundtrip_responses;
}

void
MissQueue::setCache(BaseCache *_cache)
{
    cache = _cache;
    blkSize = cache->getBlockSize();
    mq.setCache(cache);
    wb.setCache(cache);

#ifdef DO_REQUEST_TRACE
    if(!cache->isShared && cache->adaptiveMHA != NULL){

        latencyTrace = RequestTrace(_cache->name(), "LatencyTrace");

        vector<string> params;
        params.push_back("Address");
        params.push_back("PC");
        params.push_back("IC Entry");
        params.push_back("IC Latency");
        params.push_back("IC Delivery");
        params.push_back("Mem Bus Blocking");
        params.push_back("Mem Bus Queue");
        params.push_back("Mem Bus Service");

        latencyTrace.initalizeTrace(params);

        interferenceTrace = RequestTrace(_cache->name(), "InterferenceTrace");

        vector<string> params2;
        params2.push_back("Address");
        params2.push_back("PC");
        params2.push_back("IC Entry");
        params2.push_back("IC Latency");
        params2.push_back("IC Delivery");
        params2.push_back("Mem Bus Blocking");
        params2.push_back("Mem Bus Queue");
        params2.push_back("Mem Bus Service");

        interferenceTrace.initalizeTrace(params2);
    }
#endif
}

void
MissQueue::setPrefetcher(BasePrefetcher *_prefetcher)
{
    prefetcher = _prefetcher;
}

MSHR*
MissQueue::allocateMiss(MemReqPtr &req, int size, Tick time)
{

    MSHR* mshr = mq.allocate(req, size);

    if(cache->isDirectoryAndL1DataCache()){
        assert(mshr->directoryOriginalCmd == InvalidCmd);
        mshr->directoryOriginalCmd = req->cmd;
    }

    mshr->order = order++;
    if (!req->isUncacheable() ){//&& !req->isNoAllocate()) {
	// Mark this as a cache line fill
	mshr->req->flags |= CACHE_LINE_FILL;
    }
    if (mq.isFull()) {
	cache->setBlocked(Blocked_NoMSHRs);
    }
    if (req->cmd != Hard_Prefetch) {
	//If we need to request the bus (not on HW prefetch), do so
	cache->setMasterRequest(Request_MSHR, time);
    }

    if(mq.isFull()) assert(cache->isBlocked());

    return mshr;
}


MSHR*
MissQueue::allocateWrite(MemReqPtr &req, int size, Tick time)
{
    req->finishedInCacheAt = curTick;
    MSHR* mshr = wb.allocate(req,req->size);
    mshr->order = order++;
    if (cache->doData()){
	if (req->isCompressed()) {
	    delete [] mshr->req->data;
	    mshr->req->actualSize = req->actualSize;
	    mshr->req->data = new uint8_t[req->actualSize];
	    memcpy(mshr->req->data, req->data, req->actualSize);
	} else {
	    memcpy(mshr->req->data, req->data, req->size);
	}
    }

    // set blocked if it is not already
    // FIXME: a response can cause a writeback while we are blocked, not ideal
    if (wb.isFull() && !cache->isBlockedNoWBBuffers()) {
	cache->setBlocked(Blocked_NoWBBuffers);
    }

    cache->setMasterRequest(Request_WB, time);

    return mshr;
}


/**
 * @todo Remove SW prefetches on mshr hits.
 */
void
MissQueue::handleMiss(MemReqPtr &req, int blkSize, Tick time)
{

    if (prefetchMiss) prefetcher->handleMiss(req, time);

    int size = blkSize;
    Addr blkAddr = req->paddr & ~(Addr)(blkSize-1);

    MSHR* mshr = NULL;
    if (!req->isUncacheable()) {
        mshr = mq.findMatch(blkAddr, req->asid);

	if (mshr){

            //@todo remove hw_pf here
            mshr_hits[req->cmd.toIndex()][req->thread_num]++;
            if (mshr->threadNum != req->thread_num) {
                mshr->threadNum = -1;
            }

            mq.allocateTarget(mshr, req);

            if (mshr->req->isNoAllocate() && !req->isNoAllocate()) {
                //We are adding an allocate after a no-allocate
                mshr->req->flags &= ~NO_ALLOCATE;
            }

            assert(mshr->getNumTargets() <= numTarget);
            if (mshr->getNumTargets() == numTarget) {
                noTargetMSHR = mshr;
                cache->setBlocked(Blocked_NoTargets);

                // this call creates deadlocks, not neccessary with new getMemReq impl
                //mq.moveToFront(mshr);
            }
            return;

	}
	if (req->isNoAllocate()) {
	    //Count no-allocate requests differently
	    mshr_no_allocate_misses++;
	}
	else {
	    mshr_misses[req->cmd.toIndex()][req->thread_num]++;
	}
    } else {
	//Count uncacheable accesses
	mshr_uncacheable[req->cmd.toIndex()][req->thread_num]++;
	size = req->size;
    }
    if (req->cmd.isWrite() && (req->isUncacheable() || !writeAllocate ||
			       req->cmd.isNoResponse())) {
	/**
	 * @todo Add write merging here.
	 */

	mshr = allocateWrite(req, req->size, time);
	return;
    }

    mshr = allocateMiss(req, size, time);
}

MSHR*
MissQueue::fetchBlock(Addr addr, int asid, int blk_size, Tick time,
		      MemReqPtr &target)
{

    Addr blkAddr = addr & ~(Addr)(blk_size - 1);
    assert(mq.findMatch(addr, asid) == NULL);
    MSHR *mshr = mq.allocateFetch(blkAddr, asid, blk_size, target);
    mshr->order = order++;
    mshr->req->flags |= CACHE_LINE_FILL;
    if (mq.isFull()) {

	cache->setBlocked(Blocked_NoMSHRs);
    }

    cache->setMasterRequest(Request_MSHR, time);
    return mshr;
}

MemReqPtr
MissQueue::getMemReq()
{
    MemReqPtr req = NULL;
    MemReqPtr mqReq = mq.getReq();
    MemReqPtr wbReq = wb.getReq();

    if(mqReq) assert(mqReq->finishedInCacheAt != 0);
    if(wbReq) assert(wbReq->finishedInCacheAt != 0);

    if(mqReq && mqReq->finishedInCacheAt <= curTick
       && wbReq && wbReq->finishedInCacheAt <= curTick){

        // POLICY: in allocation order in each queue, oldest first intra queue
        if(mqReq->finishedInCacheAt <= wbReq->finishedInCacheAt) req = mqReq;
        else req = wbReq;

        assert(prevTime <= req->finishedInCacheAt);
        prevTime = req->finishedInCacheAt;
    }
    else if(mqReq && mqReq->finishedInCacheAt <= curTick){
        req = mqReq;
    }
    else if(wbReq && wbReq->finishedInCacheAt <= curTick){
        req = wbReq;
    }

    if (!mq.isFull()){

      if (!req) {
        req = prefetcher->getMemReq();
        if (req) {
          //Update statistic on number of prefetches issued (hwpf_mshr_misses)
          mshr_misses[req->cmd.toIndex()][req->thread_num]++;
          //It will request the bus for the future, but should clear that immedieatley
          allocateMiss(req, req->size, curTick);
          req = mq.getReq();
          assert(req); //We should get back a req b/c we just put one in
        }
      }
    }

    return req;
}

void
MissQueue::setBusCmd(MemReqPtr &req, MemCmd cmd)
{

    assert(req->mshr != 0);
    MSHR * mshr = req->mshr;
    mshr->originalCmd = req->cmd;
    if (req->isCacheFill() || req->isNoAllocate())
	req->cmd = cmd;
}

void
MissQueue::restoreOrigCmd(MemReqPtr &req)
{
    if(changeQueue) mqWasPrevQueue = !mqWasPrevQueue;
    req->cmd = req->mshr->originalCmd;
}

void
MissQueue::markInService(MemReqPtr &req)
{
    if(changeQueue) mqWasPrevQueue = !mqWasPrevQueue;

    assert(req->mshr != 0);
    bool unblock = false;
    BlockedCause cause = NUM_BLOCKED_CAUSES;

    /**
     * @todo Should include MSHRQueue pointer in MSHR to select the correct
     * one.
     */
    if ((!req->isCacheFill() && req->cmd.isWrite()) || req->cmd == Copy) {
	// Forwarding a write/ writeback, don't need to change
	// the command
	unblock = wb.isFull();

	wb.markInService(req->mshr);
	if (!wb.havePending()){
	    cache->clearMasterRequest(Request_WB);
	}
	if (unblock) {
	    // Do we really unblock?
	    unblock = !wb.isFull();
	    cause = Blocked_NoWBBuffers;
	}
    } else {
	unblock = mq.isFull();
	mq.markInService(req->mshr);
	if (!mq.havePending()){
	    cache->clearMasterRequest(Request_MSHR);
	}
	if (req->mshr->originalCmd == Hard_Prefetch) {
	    DPRINTF(HWPrefetch, "%s:Marking a HW_PF in service\n",
		    cache->name());
	    //Also clear pending if need be
	    if (!prefetcher->havePending())
	    {
                cache->clearMasterRequest(Request_PF);
	    }
	}
	if (unblock) {
	    unblock = !mq.isFull();
            if(req->mshr != noTargetMSHR){
                cause = Blocked_NoMSHRs;
            }
            else{
                cause = Blocked_NoTargets;
            }
	}
    }
    if (unblock) {
	cache->clearBlocked(cause);
    }
}

void
MissQueue::measureInterference(MemReqPtr& req){

    //NOTE: finishedInCacheAt is written in the L2 and cannot be used
    sum_roundtrip_latency += curTick - (req->time + cache->getHitLatency());
    num_roundtrip_responses++;

    interconnect_entry_latency +=  req->latencyBreakdown[INTERCONNECT_ENTRY_LAT];
    interconnect_transfer_latency +=  req->latencyBreakdown[INTERCONNECT_TRANSFER_LAT];
    interconnect_delivery_latency +=  req->latencyBreakdown[INTERCONNECT_DELIVERY_LAT];
    bus_entry_latency +=  req->latencyBreakdown[MEM_BUS_ENTRY_LAT];
    bus_queue_latency +=  req->latencyBreakdown[MEM_BUS_QUEUE_LAT];
    bus_service_latency +=  req->latencyBreakdown[MEM_BUS_SERVICE_LAT];

    if(cache->cpuCount > 1){
        for(int i=0;i<req->interferenceBreakdown.size();i++){
            sum_roundtrip_interference += req->interferenceBreakdown[i];
        }
        sum_roundtrip_interference += req->cacheCapacityInterference;

        interconnect_entry_interference += req->interferenceBreakdown[INTERCONNECT_ENTRY_LAT];
        interconnect_transfer_interference += req->interferenceBreakdown[INTERCONNECT_TRANSFER_LAT];
        interconnect_delivery_interference += req->interferenceBreakdown[INTERCONNECT_DELIVERY_LAT];
        bus_entry_interference += req->interferenceBreakdown[MEM_BUS_ENTRY_LAT];
        bus_queue_interference += req->interferenceBreakdown[MEM_BUS_QUEUE_LAT];
        bus_service_interference += req->interferenceBreakdown[MEM_BUS_SERVICE_LAT];

        cache_capacity_interference += req->cacheCapacityInterference;
    }

#ifdef DO_REQUEST_TRACE

    if(curTick >= cache->detailedSimulationStartTick){

        // Latency trace
        vector<RequestTraceEntry> lats;
        lats.push_back(RequestTraceEntry(req->paddr));
        lats.push_back(RequestTraceEntry(req->pc));
        for(int i=0;i<req->latencyBreakdown.size();i++){
	  lats.push_back(RequestTraceEntry(req->latencyBreakdown[i]));
        }

        assert(latencyTrace.isInitialized());
        latencyTrace.addTrace(lats);

        // Interference trace
        if(cache->cpuCount > 1){
            vector<RequestTraceEntry> interference;
            interference.push_back(RequestTraceEntry(req->paddr));
            interference.push_back(RequestTraceEntry(req->pc));
            for(int i=0;i<req->latencyBreakdown.size();i++){
                interference.push_back(RequestTraceEntry(req->interferenceBreakdown[i]));
            }

            assert(interferenceTrace.isInitialized());
            interferenceTrace.addTrace(interference);
        }
    }
#endif
}

void
MissQueue::handleResponse(MemReqPtr &req, Tick time)
{
	MSHR* mshr = req->mshr;
	if (req->mshr->originalCmd == Hard_Prefetch) {
		DPRINTF(HWPrefetch, "%s:Handling the response to a HW_PF\n",
				cache->name());
	}

#ifndef NDEBUG
	int num_targets = mshr->getNumTargets();
#endif

	bool unblock = false;
	bool unblock_target = false;
	BlockedCause cause = NUM_BLOCKED_CAUSES;

	if (req->isCacheFill() && !req->isNoAllocate()) {


		mshr_miss_latency[mshr->originalCmd][req->thread_num] += curTick - req->time;

		if(!cache->isShared && cache->adaptiveMHA != NULL){
			measureInterference(req);
		}

		if(!cache->isShared && cache->interferenceManager != NULL){
			cache->interferenceManager->incrementTotalReqCount(req, curTick - (req->time + cache->getHitLatency()));
		}

		// targets were handled in the cache tags
		if (mshr == noTargetMSHR) {
			// we always clear at least one target
			unblock_target = true;
			cause = Blocked_NoTargets;
			noTargetMSHR = NULL;
		}

		if (mshr->hasTargets()) {

			// Didn't satisfy all the targets, need to resend
			MemCmd cmd = mshr->getTarget()->cmd;
			mq.markPending(mshr, cmd);
			mshr->order = order++;

			if(cache->isDirectoryAndL1DataCache()){
				// reset the addressing from the previous request
				req->toProcessorID = -1;
				req->fromProcessorID = -1;
				req->toInterfaceID = -1;
				req->fromInterfaceID = -1;
			}

			cache->setMasterRequest(Request_MSHR, time);
		}
		else {

			unblock = mq.isFull();
			mq.deallocate(mshr);

			if (unblock) {
				unblock = !mq.isFull();
				cause = Blocked_NoMSHRs;
			}
		}

	} else {

		assert(mshr != noTargetMSHR);

		if (req->isUncacheable()) {
			mshr_uncacheable_lat[req->cmd][req->thread_num] +=
				curTick - req->time;
		}
		if (mshr->hasTargets() && req->isUncacheable()) {
			// Should only have 1 target if we had any
			assert(num_targets == 1);
			MemReqPtr target = mshr->getTarget();
			mshr->popTarget();
			if (cache->doData() && req->cmd.isRead()) {
				memcpy(target->data, req->data, target->size);
			}
			cache->respond(target, time);
			assert(!mshr->hasTargets());
		}
		else if (mshr->hasTargets()) {
			//Must be a no_allocate with possibly more than one target
			assert(mshr->req->isNoAllocate());

			while (mshr->hasTargets()) {
				MemReqPtr target = mshr->getTarget();
				mshr->popTarget();
				if (cache->doData() && req->cmd.isRead()) {
					memcpy(target->data, req->data, target->size);
				}
				cache->respond(target, time);
			}
		}

		if (req->cmd.isWrite()) {
			// If the wrtie buffer is full, we might unblock now
			unblock = wb.isFull();

			wb.deallocate(mshr);
			if (unblock) {
				// Did we really unblock?
				unblock = !wb.isFull();
				cause = Blocked_NoWBBuffers;
			}
		} else {
			unblock = mq.isFull();
			mq.deallocate(mshr);
			if (unblock) {
				unblock = !mq.isFull();
				cause = Blocked_NoMSHRs;
			}
		}
	}

	if(unblock_target){
		// if both are set, we have recently changed the number of MSHRs
		cache->clearBlocked(Blocked_NoTargets);

		if(unblock && cache->isBlockedNoMSHRs()){
			// we are blocked for both targets and MSHRs at the same time (due to AMHA)
			cache->clearBlocked(Blocked_NoMSHRs);
		}

	}
	else if(unblock){
		cache->clearBlocked(cause);
	}
}

void
MissQueue::squash(int thread_number)
{

    bool unblock = false;
    bool unblock_target = false;
    BlockedCause cause = NUM_BLOCKED_CAUSES;

    if (noTargetMSHR && noTargetMSHR->threadNum == thread_number) {
	noTargetMSHR = NULL;
	unblock_target = true;
	cause = Blocked_NoTargets;
    }
    if (mq.isFull()) {
	unblock = true;
	cause = Blocked_NoMSHRs;
    }
    mq.squash(thread_number);
    if (!mq.havePending()) {
	cache->clearMasterRequest(Request_MSHR);
    }

    if(unblock && unblock_target){
        assert(cache->useAdaptiveMHA);
        cache->clearBlocked(Blocked_NoTargets);
    }
    else if (unblock && !mq.isFull()) {
	cache->clearBlocked(cause);
    }

}

MSHR*
MissQueue::findMSHR(Addr addr, int asid) const
{

    return mq.findMatch(addr,asid);
}

bool
MissQueue::findWrites(Addr addr, int asid, vector<MSHR*> &writes) const
{
    return wb.findMatches(addr,asid,writes);
}

void
MissQueue::doWriteback(Addr addr, int asid, ExecContext *xc,
		       int size, uint8_t *data, bool compressed)
{
    if(cache->useDirectory) fatal("Cache tags doWriteback() version not implemented with directory");

    // Generate request
    MemReqPtr req = buildWritebackReq(addr, asid, xc, size, data,
				      compressed);

    writebacks[req->thread_num]++;

    allocateWrite(req, 0, curTick);
}


void
MissQueue::doWriteback(MemReqPtr &req)
{

    writebacks[req->thread_num]++;
    assert(req->xc || !cache->doData());

    allocateWrite(req, 0, curTick);
}


MSHR*
MissQueue::allocateTargetList(Addr addr, int asid)
{
   MSHR* mshr = mq.allocateTargetList(addr, asid, blkSize);
   mshr->req->flags |= CACHE_LINE_FILL;
   if (mq.isFull()) {
       cache->setBlocked(Blocked_NoMSHRs);
   }
   return mshr;
}

bool
MissQueue::havePending()
{
    return mq.havePending() || wb.havePending() || prefetcher->havePending();
}

map<int,int>
MissQueue::assignBlockingBlame(bool blockedForMiss, bool blockedForTargets, double threshold){
    if(blockedForMiss) return mq.assignBlockingBlame(numTarget, !blockedForTargets, threshold);
    return wb.assignBlockingBlame(numTarget, !blockedForTargets, threshold);
}
