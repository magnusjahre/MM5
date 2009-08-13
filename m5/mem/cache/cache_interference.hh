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

	std::vector<bool> doInterferenceInsertion;

	std::vector<double> interferenceMissProbabilities;
	std::vector<double> interferenceWritebackProbabilities;

	std::vector<int> samplePrivateMisses;
	std::vector<int> sampleSharedMisses;

	std::vector<Tick> sumSharedMissLatency;
	std::vector<int> numSharedMisses;

    bool isLeaderSet(int set);

public:

	CacheInterference(){ }

	CacheInterference(int _numLeaderSets, int _totalSetNumber, int _bankCount, std::vector<LRU*> _shadowTags, BaseCache* _cache);

	int getNumLeaderSets(){
		return numLeaderSets;
	}

	void access(MemReqPtr& req, bool isCacheMiss);

	void handleResponse(MemReqPtr& req);

	std::vector<int> getMissEstimates();

	void computeInterferenceProbabilities(int cpuID);

	void initiateInterferenceInsertions(int cpuID){
		doInterferenceInsertion[cpuID] = true;
	}

	bool interferenceInsertionsInitiated(int cpuID){
		return doInterferenceInsertion[cpuID];
	}
};

#endif /* CACHE_INTERFERENCE_HH_ */
