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

}

void
MissQueue::setCache(BaseCache *_cache)
{
    cache = _cache;
    blkSize = cache->getBlockSize();
    mq.setCache(cache);
    wb.setCache(cache);
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
    
//    if(req->paddr == 13835058060651984384ull){
//        cout << curTick << " " << cache->name() << " allocating with new MSHR\n";
//    }
    
//     if(curTick >= 25100000 && cache->name() == "L1icaches3"){
//         cout << curTick << "After:\n";
//         mq.printShortStatus();
//     }
    
    
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
//         if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//             cout << curTick << ": SetBlocked called from allocateMiss\n";
//         }
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
        
//         if(curTick == 1086049 && cache->name() == "L2Bank2"){
//             cout << "Sweet breakpoint\n";
//         }
        
        
//         if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//             cout << curTick << ": SetBlocked called from allocateWrite\n";
//         }
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
    
//     if(cache->name() == "L1dcaches3" && req->oldAddr == 4831407968){
//         cout << curTick << "Block seen in handle miss, status is ";
//         mq.printShortStatus();
//     }
    
//    if (!cache->isTopLevel())
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
                
//                 if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//                     cout << curTick << ": SetBlocked called from handleMiss\n";
//                 }
                
                cache->setBlocked(Blocked_NoTargets);
                mq.moveToFront(mshr);
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
        
//         if(cache->name() == "L2Bank2" && curTick >= 1086000){
//             cout << "calling allocateWrite from handleMiss\n";
//         }
        
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
        
//         if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//             cout << curTick << ": SetBlocked called from fetchBlock\n";
//         }
	cache->setBlocked(Blocked_NoMSHRs);
    }
    
//     if(cache->getMasterInterfaceType() == CROSSBAR){
//         cache->setMasterRequestAddr(blkAddr);
//     }
    cache->setMasterRequest(Request_MSHR, time);
    return mshr;
}

MemReqPtr
MissQueue::getMemReq()
{
    MemReqPtr req = NULL;
    MemReqPtr mqReq = mq.getReq();
    MemReqPtr wbReq = wb.getReq();
    
    changeQueue = false;
    
//     cout << curTick << " " << cache->name() << ": Get MemReq called, " << req << " mqreq "<< mqReq << " wbReq "<< wbReq << "\n";
    
    if(mqReq && mqReq->time < curTick 
       && wbReq && wbReq->time < curTick){
        
//         if(cache->name() == "L1dcaches1") cout << curTick << " Round Robin\n";
        
        if(mqWasPrevQueue){
//             if(cache->name() == "L1dcaches1") cout << curTick << " Choosing writeback\n";
            req = wbReq;
        }
        else{
//             if(cache->name() == "L1dcaches1") cout << curTick << " Choosing miss queue\n";
            req = mqReq;
        }
        changeQueue = true;
    }
    else if(mqReq && mqReq->time < curTick){
//         if(cache->name() == "L1dcaches1") cout << curTick << " Choosing miss queue, rr not needed\n";
        req = mqReq;
    }
    else if(wbReq && wbReq->time < curTick){
//         if(cache->name() == "L1dcaches1") cout << curTick << " Choosing writeback, rr not needed\n";
        req = wbReq;
    }
    
//     cout << "returning " << req << "\n";
    
    //TODO: Removed call to prefetcher if req is NULL, might break prefetch code
    
    return req;
    
    
// Old Code (M5 standard follows)
    
//     if(cache->name() == "L2Bank2" && curTick >= 1086000){
//         cout << curTick << ": calling mq getMemReq, checking mq first\n";
//     }
    
//     MemReqPtr req = mq.getReq();
//     
//     if (((wb.isFull() && wb.inServiceMSHRs == 0) || !req || 
// 	 req->time > curTick) && wb.havePending()) {
// 	
// //         if(cache->name() == "L2Bank2" && curTick >= 1086000){
// //             if(!req) cout << "wb is full, selecting from wb\n";
// //         }
//         
// //         if(cache->name() == "L2Bank0" && curTick >= 1127305){
// //             cout << curTick << " " << cache->name() << " retrieving from writeback\n";
// //         }
//         req = wb.getReq();
// 	// Need to search for earlier miss.
// 	MSHR *mshr = mq.findPending(req);
// 	if (mshr && mshr->order < req->mshr->order) {
// 	    // Service misses in order until conflict is cleared.
//             
// //             if(cache->name() == "L1dcaches2" && curTick >= 1086000){
// //                 if(!req) cout << "wb got canceled, sending from mq instead\n";
// //             }
//             
//             if(cache->name() == "L2Bank0" && curTick >= 1127305){
//                 cout << curTick << " " << cache->name() << " retrieved from miss queue on second go\n";
//             }
//             
// 	    return mq.getReq();
// 	}
//     }
//     else if (req) { //HACK by Magnus, was: if(req)
// 	MSHR* mshr = wb.findPending(req);
// 	if (mshr /*&& mshr->order < req->mshr->order*/) {
// 	    // The only way this happens is if we are
// 	    // doing a write and we didn't have permissions
// 	    // then subsequently saw a writeback(owned got evicted)
// 	    // We need to make sure to perform the writeback first
// 	    // To preserve the dirty data, then we can issue the write
//             
//             if(cache->name() == "L2Bank0" && curTick >= 1127305){
//                 cout << curTick << " " << cache->name() << " wierd code (1)\n";
//             }
//             
// 	    return wb.getReq();
// 	}
//     }
//     else if (!mq.isFull()){
// 	//If we have a miss queue slot, we can try a prefetch
//         if(cache->name() == "L2Bank0" && curTick >= 1127305){
//             cout << curTick << " " << cache->name() << " wierd code (2)\n";
//         }
// 	req = prefetcher->getMemReq();
// 	if (req) {
// 	    //Update statistic on number of prefetches issued (hwpf_mshr_misses)
// 	    mshr_misses[req->cmd.toIndex()][req->thread_num]++;
// 	    //It will request the bus for the future, but should clear that immedieatley
// 	    allocateMiss(req, req->size, curTick);
// 	    req = mq.getReq();
// 	    assert(req); //We should get back a req b/c we just put one in
// 	}
//     }
//     
// //     if(cache->name() == "L1dcaches2" && curTick >= 1086000){
// //         if(!req) cout << "Request is NULL!\n";
// //     }
//     
// //     if(cache->name() == "L2Bank0" && curTick >= 1127305){
// //         cout << curTick << " " << cache->name() << " fell through, unclear which queue got serviced\n";
// //     }
//     
//     return req;
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
//     if(cache->name() == "L1dcaches2" && curTick >= 1086000){
//         cout << curTick << "Mark in service is called\n";
//     }
    
//     if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//         std::cout << curTick << ": markInService is called, miss queue is " << (mq.isFull() ? "full" : "not full") << "\n";
//     }
    
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
        
//         if(unblock && cache->name() == "L1dcaches2" && curTick >= 1086000){
//             cout << curTick << "Attempting to unblock in markInService\n";
//         }
        
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
//         if(cache->name() == "L1dcaches0" && curTick > 1049000000){
//             cout << curTick << ": Unblocking in markInService\n";
//         }
        
        
//         if(unblock && cache->name() == "L1dcaches2" && curTick >= 1086000){
//             cout << curTick << "Actually unblocking!\n";
//         }
//         
//         if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//             cout << curTick << ": ClearBlocked called from markInService\n";
//         }
	cache->clearBlocked(cause);
    }
    
//     cout << curTick << " " << cache->name() << ": Mark in service is called:\n";
//     mq.printShortStatus();
}


void
MissQueue::handleResponse(MemReqPtr &req, Tick time)
{
    
//     if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//         std::cout << curTick << ": handleResponse is called, miss queue is " << (mq.isFull() ? "full" : "not full") << "\n";
//     }
    
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
    
    
//     if(cache->name() == "L1dcaches0" && curTick > 1049000000){
//         cout << curTick << ": Miss queue handleResponse is called\n";
//     }
    
    if (req->isCacheFill() && !req->isNoAllocate()) {
        
        
//         if(cache->name() == "L1dcaches0" && curTick > 1049000000){
//             cout << curTick << ": Request is cache fill\n";
//         }
        
        mshr_miss_latency[mshr->originalCmd][req->thread_num] +=
	    curTick - req->time;
	// targets were handled in the cache tags
	if (mshr == noTargetMSHR) {
	    // we always clear at least one target
	    unblock_target = true;
	    cause = Blocked_NoTargets;
	    noTargetMSHR = NULL;
//             if(cache->name() == "L1dcaches0" && curTick > 1049000000){
//                 cout << curTick << ": Is noTargetMSHR\n";
//             }
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
                
//                 if(cache->name() == "L1dcaches0" && curTick > 1049000000){
//                     cout << curTick << ": Cache is full and we are " << (unblock ? "unblocking" : "not unblocking") << "\n";
//                 }
                
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
//         if(cache->name() == "L1dcaches0" && curTick > 1049000000){
//             cout << curTick << ": Unblocking due to targets\n";
//         }
//         if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//             cout << curTick << ": ClearBlocked called from handleResponse (1)\n";
//         }
        cache->clearBlocked(Blocked_NoTargets);
        
        if(unblock && cache->isBlockedNoMSHRs()){
            // we are blocked for both targets and MSHRs at the same time (due to AMHA)
//             if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//                 cout << curTick << ": ClearBlocked called from handleResponse, unblocking for MSHRs as well!!!\n";
//             }
            cache->clearBlocked(Blocked_NoMSHRs);
        }
        
    }
    else if(unblock){
//         if(cache->name() == "L1dcaches0" && curTick > 1390000){
//             cout << curTick << ": Unblocking on cause " << cause  << " from handle resp, cache is " << (cache->isBlocked() ? "" : "not") << " blocked\n";
//         }
//         if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//             cout << curTick << ": ClearBlocked called from handleResponse (2)\n";
//         }
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
//         if(cache->name() == "L1dcaches0" && curTick > 1049000000){
//             cout << curTick << ": Unblocking in squash (new code)\n";
//         }
        
//         if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//             cout << curTick << ": ClearBlocked called from squash (1)\n";
//         }
        cache->clearBlocked(Blocked_NoTargets);
    }
    else if (unblock && !mq.isFull()) {
//         if(cache->name() == "L1dcaches0" && curTick > 1049000000){
//             cout << curTick << ": Unblocking in squash (old code)\n";
//         }
        
//         if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//             cout << curTick << ": ClearBlocked called from squash (2)\n";
//         }
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
    
//     if(curTick >= 1086000){
//         cout << curTick << " " << cache->name() << "Calling doWriteback() (1)\n";
//     }
    
    // Generate request
    MemReqPtr req = buildWritebackReq(addr, asid, xc, size, data,
				      compressed);

    writebacks[req->thread_num]++;

//     if(cache->name() == "L2Bank2" && curTick >= 1086000){
//         cout << "calling allocateWrite from doWriteback (1)\n";
//     }
    
    allocateWrite(req, 0, curTick);
}


void
MissQueue::doWriteback(MemReqPtr &req)
{
    
//     if(curTick >= 1086000){
//         cout << curTick << " " << cache->name() << "Calling doWriteback() (2)\n";
//     }
    
    writebacks[req->thread_num]++;
    assert(req->xc || !cache->doData());
    
//     if(cache->name() == "L2Bank2" && curTick >= 1086000){
//         cout << "calling allocateWrite from doWriteback (2)\n";
//     }
    allocateWrite(req, 0, curTick);
}


MSHR* 
MissQueue::allocateTargetList(Addr addr, int asid)
{
   MSHR* mshr = mq.allocateTargetList(addr, asid, blkSize);
   mshr->req->flags |= CACHE_LINE_FILL;
   if (mq.isFull()) {
//        if(cache->name() == "L1dcaches3" && curTick >= 1082099355){
//            cout << curTick << ": SetBlocked called from allocateTargetList\n";
//        }
//        
       cache->setBlocked(Blocked_NoMSHRs);
   }
   return mshr;
}

bool
MissQueue::havePending()
{
    return mq.havePending() || wb.havePending() || prefetcher->havePending();
}
