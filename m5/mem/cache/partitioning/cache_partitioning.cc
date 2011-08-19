/*
 * cache_partitioning.cc
 *
 *  Created on: May 2, 2010
 *      Author: jahre
 */

#include "mem/cache/partitioning/cache_partitioning.hh"

using namespace std;

CachePartitioning::CachePartitioning(std::string _name,
									 int _associativity,
									 Tick _epochSize,
									 int _np,
									 CacheInterference* ci)
: SimObject(_name){
	cacheBanks.resize(_np, NULL);
	associativity = _associativity;
	partitioningCpuCount = _np;

    epochSize = _epochSize;
    assert(epochSize > 0);

    cacheInterference = ci;
    shadowTags = ci->shadowTags;

    repartEvent = new CacheRepartitioningEvent(this);
    repartEvent->schedule(1);
}

CachePartitioning::~CachePartitioning(){
	assert(!repartEvent->scheduled());
	delete repartEvent;
}

void
CachePartitioning::registerCache(BaseCache* _cache, int bankID){
	assert(cacheBanks[bankID] == NULL);
	cacheBanks[bankID] = _cache;
}

void
CachePartitioning::debugPrintPartition(std::vector<int>& partition, const char* message){
	DPRINTF(CachePartitioning, message);
	for(int i=0;i<partition.size();i++) DPRINTFR(CachePartitioning, "%d:%d ", i , partition[i]);
	DPRINTFR(CachePartitioning, "\n");
}

void
CachePartitioning::schedulePartitionEvent(){
	// reset counters and reschedule event
	for(int i=0;i<shadowTags.size();i++) shadowTags[i]->resetHitCounters();
	assert(repartEvent != NULL);
	repartEvent->schedule(curTick + epochSize);

	DPRINTF(CachePartitioning, "Scheduling next event for %d\n", curTick+epochSize);
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("CachePartitioning", CachePartitioning);

#endif

