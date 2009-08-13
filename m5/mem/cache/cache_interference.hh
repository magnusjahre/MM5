/*
 * cache_interference.hh
 *
 *  Created on: Aug 13, 2009
 *      Author: jahre
 */

#ifndef CACHE_INTERFERENCE_HH_
#define CACHE_INTERFERENCE_HH_

#include "base_cache.hh"
#include "mem/mem_req.hh"
#include "mem/cache/tags/lru.hh"
#include <vector>
#include <iostream>

class BaseCache;

class CacheInterference{

private:

	int numLeaderSets;
	int totalSetNumber;
	int numBanks;
	BaseCache* cache;

	std::vector<LRU*> shadowTags;

    bool isLeaderSet(int set);

public:

	CacheInterference(){ }

	CacheInterference(int _numLeaderSets, int _totalSetNumber, int _bankCount, std::vector<LRU*> _shadowTags, BaseCache* _cache);

	int getNumLeaderSets(){
		return numLeaderSets;
	}

	bool access(MemReqPtr& req, bool isCacheMiss);

	void handleResponse(MemReqPtr& req);

	std::vector<int> getMissEstimates();

};

#endif /* CACHE_INTERFERENCE_HH_ */
