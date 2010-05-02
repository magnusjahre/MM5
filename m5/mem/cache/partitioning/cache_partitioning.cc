/*
 * cache_partitioning.cc
 *
 *  Created on: May 2, 2010
 *      Author: jahre
 */

#include "mem/cache/partitioning/cache_partitioning.hh"

CachePartitioning::CachePartitioning(std::string _name,
									 int _associativity,
									 Tick _epochSize,
									 int _np)
: SimObject(_name){
	cache = NULL;
	associativity = _associativity;
	partitioningCpuCount = _np;

    epochSize = _epochSize;
    assert(epochSize > 0);


    repartEvent = new CacheRepartitioningEvent(this);
    repartEvent->schedule(1);
}

CachePartitioning::~CachePartitioning(){
	assert(!repartEvent->scheduled());
	delete repartEvent;
}

void
CachePartitioning::registerCache(BaseCache* _cache, std::vector<LRU*> _shadowTags){
	assert(cache == NULL);
	cache = _cache;

	assert(shadowTags.empty());
	shadowTags = _shadowTags;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("CachePartitioning", CachePartitioning);

#endif

