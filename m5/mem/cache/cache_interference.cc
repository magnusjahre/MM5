/*
 * cache_interference.cc
 *
 *  Created on: Aug 13, 2009
 *      Author: jahre
 */

#include "cache_interference.hh"

using namespace std;

CacheInterference::CacheInterference(int _numLeaderSets, int _totalSetNumber, int _numBanks, std::vector<LRU*> _shadowTags, BaseCache* _cache){

	totalSetNumber = _totalSetNumber;
	numBanks = _numBanks;
	shadowTags = _shadowTags;
	cache = _cache;

    numLeaderSets = _numLeaderSets / numBanks; //params.bankCount;
    if(numLeaderSets == 0) numLeaderSets = totalSetNumber; // 0 means full-map

	if(totalSetNumber % numLeaderSets  != 0){
		fatal("The the total number for sets must be divisible by the number of leader sets");
	}
	assert(numLeaderSets <= totalSetNumber && numLeaderSets > 0);

}

bool
CacheInterference::access(MemReqPtr& req, bool isCacheMiss){

	if(!shadowTags.empty()){

		bool shadowHit = false;
		bool shadowLeaderSet = false;

		// access tags to update LRU stack
		assert(cache->isShared);
		assert(req->adaptiveMHASenderID != -1);
		assert(req->cmd == Writeback || req->cmd == Read);

		int numberOfSets = shadowTags[req->adaptiveMHASenderID]->getNumSets();
		LRUBlk* shadowBlk = shadowTags[req->adaptiveMHASenderID]->findBlock(req, cache->getHitLatency());
		int shadowSet = shadowTags[req->adaptiveMHASenderID]->extractSet(req->paddr);
		shadowLeaderSet = isLeaderSet(shadowSet);

		if(shadowBlk != NULL){
			shadowHit = true;
			if(req->cmd == Writeback){
				shadowBlk->status |= BlkDirty;
			}
		}
		else{
			shadowHit = false;
		}
		req->isShadowMiss = !shadowHit;

		if(shadowLeaderSet){
			int setsInConstituency = numberOfSets / numLeaderSets;
			if(shadowBlk == NULL){
				cache->estimatedShadowMisses[req->adaptiveMHASenderID] += setsInConstituency;
			}
			cache->estimatedShadowAccesses[req->adaptiveMHASenderID] += setsInConstituency;
		}

		// TODO: add probibalistic interference method and return result
		// true -> pass on as interference miss, false -> do not pass on
	}

	return false;
}

void
CacheInterference::handleResponse(MemReqPtr& req){
	if(!shadowTags.empty() && cache->cpuCount > 1){
		assert(req->adaptiveMHASenderID != -1);

		LRUBlk* currentBlk = shadowTags[req->adaptiveMHASenderID]->findBlock(req, cache->getHitLatency());

		int shadowSet = shadowTags[req->adaptiveMHASenderID]->extractSet(req->paddr);
		bool isShadowLeaderSet = isLeaderSet(shadowSet);

		if(currentBlk == NULL){
     		        assert(req->isShadowMiss);
     		        if(isShadowLeaderSet) assert(req->interferenceMissAt == 0);
			LRU::BlkList shadow_compress_list;
			MemReqList shadow_writebacks;
			LRUBlk *shadowBlk = shadowTags[req->adaptiveMHASenderID]->findReplacement(req, shadow_writebacks, shadow_compress_list);
			assert(shadow_writebacks.empty()); // writebacks are not generated in findReplacement()

			if(shadowBlk->isModified() && isShadowLeaderSet){
          			assert(isShadowLeaderSet);
				cache->shadowTagWritebacks[req->adaptiveMHASenderID]++;

				if(cache->writebackOwnerPolicy == BaseCache::WB_POLICY_SHADOW_TAGS){
					MemReqPtr virtualWriteback = new MemReq();
					virtualWriteback->cmd = VirtualPrivateWriteback;
					virtualWriteback->paddr = shadowTags[req->adaptiveMHASenderID]->regenerateBlkAddr(shadowBlk->tag, shadowBlk->set);
					virtualWriteback->adaptiveMHASenderID = req->adaptiveMHASenderID;
					cache->issueVirtualPrivateWriteback(virtualWriteback);
				}
			}

			// set block values to the values of the new occupant
			shadowBlk->tag = shadowTags[req->adaptiveMHASenderID]->extractTag(req->paddr, shadowBlk);
			shadowBlk->asid = req->asid;
			assert(req->xc || !cache->doData());
			shadowBlk->xc = req->xc;
			shadowBlk->status = BlkValid;
			shadowBlk->origRequestingCpuID = req->adaptiveMHASenderID;
			assert(!shadowBlk->isModified());
		}
		else{
		  assert(!req->isShadowMiss);
		}
	}

	if(req->interferenceMissAt > 0 && cache->isShared && !cache->useUniformPartitioning){
		assert(req->adaptiveMHASenderID != -1);
		assert(req->cmd == Read);
		int extraDelay = (curTick + cache->getHitLatency()) - req->interferenceMissAt;
		req->cacheCapacityInterference += extraDelay;
		cache->extraMissLatency[req->adaptiveMHASenderID] += extraDelay;
		cache->numExtraResponses[req->adaptiveMHASenderID]++;

		//TODO: add new interference estimation model
		assert(cache->interferenceManager != NULL);
		cache->interferenceManager->addInterference(InterferenceManager::CacheCapacity, req, extraDelay);
	}
}

vector<int>
CacheInterference::getMissEstimates(){
	return vector<int>();
}

bool
CacheInterference::isLeaderSet(int set){

	assert(numLeaderSets != -1);
	if(numLeaderSets == totalSetNumber) return true;

	int setsInConstituency = totalSetNumber / numLeaderSets;
	int constituencyNumber = set / setsInConstituency;
	int leaderSet = constituencyNumber * setsInConstituency + (constituencyNumber % setsInConstituency);

	return leaderSet == set;
}
