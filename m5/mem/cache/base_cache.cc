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
 * Definition of BaseCache functions.
 */

#include "mem/cache/base_cache.hh"
#include "cpu/smt.hh"
#include "cpu/base.hh"

#define CACHE_CHECK_INTERVAL 100000

using namespace std;

BaseCache::BaseCache(const std::string &name, 
                     HierParams *hier_params, 
                     Params &params, 
                     bool _isShared, 
                     bool _useDirectory, 
                     bool _isReadOnly)
    : BaseMem(name, hier_params, params.hitLatency, params.addrRange), 
              blocked(0), blockedSnoop(0), masterRequests(0), slaveRequests(0),
              topLevelCache(false),  blkSize(params.blkSize), 
              missCount(params.maxMisses), isShared(_isShared),
              useDirectory(_useDirectory), isReadOnly(_isReadOnly)
{
    checkEvent = new CacheAliveCheckEvent(this);
    checkEvent->schedule(CACHE_CHECK_INTERVAL);
    blockedAt = 0;
}

void
BaseCache::regStats()
{
    using namespace Stats;

    // Hit statistics
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	hits[access_idx]
	    .init(maxThreadsPerCPU)
	    .name(name() + "." + cstr + "_hits")
	    .desc("number of " + cstr + " hits")
	    .flags(total | nozero | nonan)
	    ;
    }

    demandHits
	.name(name() + ".demand_hits")
	.desc("number of demand (read+write) hits")
	.flags(total)
	;
    demandHits = hits[Read] + hits[Write];

    overallHits
	.name(name() + ".overall_hits")
	.desc("number of overall hits")
	.flags(total)
	;
    overallHits = demandHits + hits[Soft_Prefetch] + hits[Hard_Prefetch]
	+ hits[Writeback];

    // Miss statistics
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	misses[access_idx]
	    .init(maxThreadsPerCPU)
	    .name(name() + "." + cstr + "_misses")
	    .desc("number of " + cstr + " misses")
	    .flags(total | nozero | nonan)
	    ;
    }

    demandMisses
	.name(name() + ".demand_misses")
	.desc("number of demand (read+write) misses")
	.flags(total)
	;
    demandMisses = misses[Read] + misses[Write];

    overallMisses
	.name(name() + ".overall_misses")
	.desc("number of overall misses")
	.flags(total)
	;
    overallMisses = demandMisses + misses[Soft_Prefetch] +
	misses[Hard_Prefetch] + misses[Writeback];

    // Miss latency statistics
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	missLatency[access_idx]
	    .init(maxThreadsPerCPU)
	    .name(name() + "." + cstr + "_miss_latency")
	    .desc("number of " + cstr + " miss cycles")
	    .flags(total | nozero | nonan)
	    ;
    }

    demandMissLatency
	.name(name() + ".demand_miss_latency")
	.desc("number of demand (read+write) miss cycles")
	.flags(total)
	;
    demandMissLatency = missLatency[Read] + missLatency[Write];

    overallMissLatency
	.name(name() + ".overall_miss_latency")
	.desc("number of overall miss cycles")
	.flags(total)
	;
    overallMissLatency = demandMissLatency + missLatency[Soft_Prefetch] +
	missLatency[Hard_Prefetch];

    // access formulas
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	accesses[access_idx]
	    .name(name() + "." + cstr + "_accesses")
	    .desc("number of " + cstr + " accesses(hits+misses)")
	    .flags(total | nozero | nonan)
	    ;

	accesses[access_idx] = hits[access_idx] + misses[access_idx];
    }

    demandAccesses
	.name(name() + ".demand_accesses")
	.desc("number of demand (read+write) accesses")
	.flags(total)
	;
    demandAccesses = demandHits + demandMisses;

    overallAccesses
	.name(name() + ".overall_accesses")
	.desc("number of overall (read+write) accesses")
	.flags(total)
	;
    overallAccesses = overallHits + overallMisses;

    // miss rate formulas
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	missRate[access_idx]
	    .name(name() + "." + cstr + "_miss_rate")
	    .desc("miss rate for " + cstr + " accesses")
	    .flags(total | nozero | nonan)
	    ;

	missRate[access_idx] = misses[access_idx] / accesses[access_idx];
    }

    demandMissRate
	.name(name() + ".demand_miss_rate")
	.desc("miss rate for demand accesses")
	.flags(total)
	;
    demandMissRate = demandMisses / demandAccesses;

    overallMissRate
	.name(name() + ".overall_miss_rate")
	.desc("miss rate for overall accesses")
	.flags(total)
	;
    overallMissRate = overallMisses / overallAccesses;

    // miss latency formulas
    for (int access_idx = 0; access_idx < NUM_MEM_CMDS; ++access_idx) {
	MemCmd cmd = (MemCmdEnum)access_idx;
	const string &cstr = cmd.toString();

	avgMissLatency[access_idx]
	    .name(name() + "." + cstr + "_avg_miss_latency")
	    .desc("average " + cstr + " miss latency")
	    .flags(total | nozero | nonan)
	    ;

	avgMissLatency[access_idx] =
	    missLatency[access_idx] / misses[access_idx];
    }

    demandAvgMissLatency
	.name(name() + ".demand_avg_miss_latency")
	.desc("average overall miss latency")
	.flags(total)
	;
    demandAvgMissLatency = demandMissLatency / demandMisses;

    overallAvgMissLatency
	.name(name() + ".overall_avg_miss_latency")
	.desc("average overall miss latency")
	.flags(total)
	;
    overallAvgMissLatency = overallMissLatency / overallMisses;

    blocked_cycles.init(NUM_BLOCKED_CAUSES);
    blocked_cycles
	.name(name() + ".blocked_cycles")
	.desc("number of cycles access was blocked")
	.subname(Blocked_NoMSHRs, "no_mshrs")
	.subname(Blocked_NoTargets, "no_targets")
	;


    blocked_causes.init(NUM_BLOCKED_CAUSES);
    blocked_causes
	.name(name() + ".blocked")
	.desc("number of cycles access was blocked")
	.subname(Blocked_NoMSHRs, "no_mshrs")
	.subname(Blocked_NoTargets, "no_targets")
	;

    avg_blocked
	.name(name() + ".avg_blocked_cycles")
	.desc("average number of cycles each access was blocked")
	.subname(Blocked_NoMSHRs, "no_mshrs")
	.subname(Blocked_NoTargets, "no_targets")
	;

    avg_blocked = blocked_cycles / blocked_causes;

    fastWrites
	.name(name() + ".fast_writes")
	.desc("number of fast writes performed")
	;
    
    cacheCopies
	.name(name() + ".cache_copies")
	.desc("number of cache copies performed")
	;

    goodprefetches
      .name(name() + ".good_prefetches")
      .desc("Number of good prefetches")
      ;

}

void
BaseCache::setBlocked(BlockedCause cause)
{
    
    blockedAt = curTick;
    
    uint8_t flag = 1 << cause;
    if (blocked == 0) {
        blocked_causes[cause]++;
        blockedCycle = curTick;
    }
    
    assert((blocked & flag) == 0);
    
    blocked |= flag;
    DPRINTF(Cache,"Blocking for cause %s\n", cause);
    si->setBlocked();
    
//     if(name() == "L1dcaches3" && curTick >= 1082099355){
//         std::cout << curTick << ": The cache is blocking on cause " << cause << "\n";
//         printBlockedState();
//     }
    
}

void
BaseCache::clearBlocked(BlockedCause cause)
{
    
    uint8_t flag = 1 << cause;
    
    
//     if((blocked & flag) == 0){
//         cout << curTick << " " << name() << ": unblocking on cause " << cause << ", will fail\n";
//     }
    assert((blocked & flag) > 0);
    
    blocked &= ~flag;
    blockedSnoop &= ~flag;
    DPRINTF(Cache,"Unblocking for cause %s, causes left=%i\n", 
            cause, blocked);
    if (!isBlocked()) {
        blocked_cycles[cause] += curTick - blockedCycle;
        DPRINTF(Cache,"Unblocking from all causes\n");
        si->clearBlocked();
    }
    if (!isBlockedForSnoop()) {
        mi->clearBlocked();
    }
        
//     if(name() == "L1dcaches0" && curTick > 1049000000){
//         if(!isBlocked()){
//             std::cout << curTick << ": We actually unblocked\n";
//         }
//         else{
//             std::cout << curTick << ": We are still blocked\n";
//         }
//     }

//     if(name() == "L1dcaches3" && curTick >= 1082099355){
//         cout << curTick << ": The cache is unblocking on cause " << cause << "\n";
//         printBlockedState();
//     }
}

void
BaseCache::printBlockedState(){
    
    cout << curTick << " " << name() << ": Blocked state is ";
    
    for(int i=8;i>=0;i--){
        uint8_t tmpFlag = 1 << i;
        cout << ((tmpFlag & blocked) == 0 ? "0" : "1");
    }
    cout << "\n";
}

void
BaseCache::checkIfCacheAlive(){
//     cout << curTick << ": checking if cache " << name() << " is alive\n";
    
    if(isBlocked()){
        if(blockedAt < (curTick - CACHE_CHECK_INTERVAL)){
            cout << "CACHE DEADLOCK!, " << name() << " blocked at " << blockedAt << "\n";
            printBlockedState();
            panic("%s has been blocked for 100000 clock cycles", name());
        }
    }
    
    checkEvent->schedule(curTick + CACHE_CHECK_INTERVAL);
}
