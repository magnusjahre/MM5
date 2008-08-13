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
#define CONTENTION_DELAY 2

using namespace std;

BaseCache::BaseCache(const std::string &name, 
                     HierParams *hier_params, 
                     Params &params, 
                     bool _isShared, 
                     bool _useDirectory, 
                     bool _isReadOnly,
                     bool _useUniformPartitioning,
                     Tick _uniformPartitioningStart,
                     bool _useMTPPartitioning)
    : BaseMem(name, hier_params, params.hitLatency, params.addrRange), 
              blocked(0), blockedSnoop(0), masterRequests(0), slaveRequests(0),
              topLevelCache(false),  blkSize(params.blkSize), 
              missCount(params.maxMisses), isShared(_isShared),
              useDirectory(_useDirectory), isReadOnly(_isReadOnly),
              useUniformPartitioning(_useUniformPartitioning),
              uniformPartitioningStartTick(_uniformPartitioningStart),
              useMTPPartitioning(_useMTPPartitioning)
{
    if(_useUniformPartitioning){
        if(!_isShared) panic("The cache must be shared to use static uniform partitioning!");
        if(_uniformPartitioningStart == -1) panic("An uniform partitioning start tick must be provided");
    }
    
    checkEvent = new CacheAliveCheckEvent(this);
    checkEvent->schedule(CACHE_CHECK_INTERVAL);
    blockedAt = 0;
    
    if(_isShared){
        interferenceEventsBW = vector<vector<int> >(params.baseCacheCPUCount, vector<int>(params.baseCacheCPUCount,0));
        interferenceEventsCapacity = vector<vector<int> >(params.baseCacheCPUCount, vector<int>(params.baseCacheCPUCount,0));
    }
    
    nextFreeCache = 0;
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

    missesPerCPU.init(cpuCount);
    missesPerCPU
        .name(name() + ".misses_per_cpu")
        .desc("number of misses for each CPU")
        .flags(total)
        ;
    
    accessesPerCPU.init(cpuCount);
    accessesPerCPU
        .name(name() + ".accesses_per_cpu")
        .desc("number of accesses for each CPU")
        .flags(total)
        ;
    
    delayDueToCongestion
        .name(name() + ".congestion_delay")
        .desc("Additional delay due to congestion")
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
    DPRINTF(Blocking, "Blocking for cause %s\n", cause);
    si->setBlocked();
    
}

void
BaseCache::clearBlocked(BlockedCause cause)
{
    
    uint8_t flag = 1 << cause;
    
    assert((blocked & flag) > 0);
    
    blocked &= ~flag;
    blockedSnoop &= ~flag;
    DPRINTF(Cache,"Unblocking for cause %s, causes left=%i\n", 
            cause, blocked);
    DPRINTF(Blocking, "Unblocking for cause %s, causes left=%i\n", cause, blocked);
    if (!isBlocked()) {
        blocked_cycles[cause] += curTick - blockedCycle;
        DPRINTF(Cache,"Unblocking from all causes\n");
        si->clearBlocked();
    }
    if (!isBlockedForSnoop()) {
        mi->clearBlocked();
    }
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
    
    if(isBlocked()){
        if(blockedAt < (curTick - CACHE_CHECK_INTERVAL)){
            cout << "CACHE DEADLOCK!, " << name() << " blocked at " << blockedAt << "\n";
            printBlockedState();
            panic("%s has been blocked for 100000 clock cycles", name());
        }
    }
    
    checkEvent->schedule(curTick + CACHE_CHECK_INTERVAL);
}

void
BaseCache::respondToMiss(MemReqPtr &req, Tick time, bool moreTargetsToService)
{
    
    Tick originalReqTime = time;
    
    // assumes that the targets are delivered to the interconnect in parallel
    if(simulateContention && !moreTargetsToService){
        time = updateAndStoreInterference(req, time);
    }
    
    if(!isShared && adaptiveMHA != NULL && !moreTargetsToService){
        adaptiveMHA->addTotalDelay(req->adaptiveMHASenderID, time - req->time, req->paddr, true);
    }
    
    if (!req->isUncacheable()) {
        missLatency[req->cmd.toIndex()][req->thread_num] += time - req->time;
    }
    
    delayDueToCongestion += originalReqTime - time;
    
    si->respond(req,time);
}

Tick
BaseCache::updateAndStoreInterference(MemReqPtr &req, Tick time){
    
    assert(curTick == (time - hitLatency));
        
    for(int i=0;i<occupancy.size();i++){
        if(occupancy[i].endTick < curTick) occupancy.erase(occupancy.begin()+i);
    }
    
    assert(req->adaptiveMHASenderID >= 0);
    if(nextFreeCache < curTick){
        occupancy.push_back(cacheOccupancy(curTick, curTick + CONTENTION_DELAY, req->adaptiveMHASenderID, curTick));
        nextFreeCache = curTick + CONTENTION_DELAY;
    }
    else{
        assert(occupancy.back().originalRequestTick <= curTick);
        
        // search to discover which part(s) of the delay is actually due to interference with a different processor
        vector<bool> waitingFor(cpuCount, false);
        vector<int> waitingPosition(cpuCount, -1);
        for(int i=0;i<occupancy.size();i++){
          if(occupancy[i].occCPUID != req->adaptiveMHASenderID){
              waitingFor[occupancy[i].occCPUID] = true;
              waitingPosition[occupancy[i].occCPUID] = i;
          }
        }
        
        vector<vector<Tick> > interference(cpuCount, vector<Tick>(cpuCount, 0));
        vector<vector<bool> > delayedIsRead(cpuCount, vector<bool>(cpuCount, false));
        for(int i=0;i<waitingFor.size();i++){
            if(waitingFor[i]){
              Tick occFromTick = nextFreeCache - ((occupancy.size() - waitingPosition[i])*CONTENTION_DELAY);
              Tick occToTick = occFromTick + CONTENTION_DELAY;

              Tick curIP = 0;
              if(occFromTick < curTick) curIP = occToTick - curTick;
              else curIP = CONTENTION_DELAY;

              interference[req->adaptiveMHASenderID][i] = curIP;
              delayedIsRead[req->adaptiveMHASenderID][i] = (req->cmd == Read);
              interferenceEventsBW[req->adaptiveMHASenderID][i] += curIP;
            }
        }
                
        if(adaptiveMHA != NULL){
            adaptiveMHA->addInterferenceDelay(interference,
                                            req->paddr,
                                            req->cmd,
                                            req->adaptiveMHASenderID,
                                            L2_INTERFERENCE,
                                            delayedIsRead);
        }
        
        time = nextFreeCache + hitLatency;
        nextFreeCache += CONTENTION_DELAY;
        occupancy.push_back(cacheOccupancy(nextFreeCache - CONTENTION_DELAY,nextFreeCache, req->adaptiveMHASenderID, curTick));
    }
    
    return time;
}

void
BaseCache::updateInterference(MemReqPtr &req){
    
    if(nextFreeCache < curTick) nextFreeCache = curTick + CONTENTION_DELAY;
    else nextFreeCache += CONTENTION_DELAY;
    
    assert(req->adaptiveMHASenderID != -1);
    occupancy.push_back(cacheOccupancy(nextFreeCache - CONTENTION_DELAY, nextFreeCache, req->adaptiveMHASenderID, curTick));
    
    for(int i=0;i<occupancy.size();i++){
        if(occupancy[i].endTick < curTick) occupancy.erase(occupancy.begin()+i);
    }
}

std::vector<std::vector<int> > 
BaseCache::retrieveBWInterferenceStats(){
    return interferenceEventsBW;
}
        
void
BaseCache::resetBWInterferenceStats(){
    for(int i=0;i<interferenceEventsBW.size();i++){
        for(int j=0;j<interferenceEventsBW[0].size();j++){
            interferenceEventsBW[i][j] = 0;
        }
    }
}

std::vector<std::vector<int> >
BaseCache::retrieveCapacityInterferenceStats(){
    return interferenceEventsCapacity;
}


void 
BaseCache::resetCapacityInterferenceStats(){
    for(int i=0;i<interferenceEventsCapacity.size();i++){
        for(int j=0;j<interferenceEventsCapacity[0].size();j++){
            interferenceEventsCapacity[i][j] = 0;
        }
    }
}

void
BaseCache::addCapacityInterference(int victimID, int interfererID){
    interferenceEventsCapacity[victimID][interfererID]++;
}
