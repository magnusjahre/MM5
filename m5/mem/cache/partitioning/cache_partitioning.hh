/*
 * cache_partitioning.hh
 *
 *  Created on: May 2, 2010
 *      Author: jahre
 */

#include "sim/sim_object.hh"
#include "mem/cache/base_cache.hh"
#include "mem/cache/tags/lru.hh"

#ifndef CACHE_PARTITIONING_HH_
#define CACHE_PARTITIONING_HH_

class CacheRepartitioningEvent;

class CachePartitioning : public SimObject{

protected:
    BaseCache* cache;
    int associativity;
    std::vector<LRU*> shadowTags;
	Tick epochSize;
	int partitioningCpuCount;

	CacheRepartitioningEvent* repartEvent;

	void debugPrintPartition(std::vector<int>& partitions, const char* message);
public:

	CachePartitioning(std::string _name,
					  int _associativity,
					  Tick _epochSize,
					  int _np);

	~CachePartitioning();

	virtual void handleRepartitioningEvent() = 0;

	virtual bool calculatePartitions() = 0;

	void registerCache(BaseCache* _cache, std::vector<LRU*> _shadowTags);
};

class CacheRepartitioningEvent: public Event {

public:

	CachePartitioning* partitioning;

	CacheRepartitioningEvent(CachePartitioning* _partitioning) :
		Event(&mainEventQueue), partitioning(_partitioning) {
	}

	void process() {
		partitioning->handleRepartitioningEvent();
	}

	virtual const char *description() {
		return "Cache Repartitioning Event";
	}
};

#endif /* CACHE_PARTITIONING_HH_ */
