/*
 * cache_interference.cc
 *
 *  Created on: Aug 13, 2009
 *      Author: jahre
 */

#include "cache_interference.hh"
#include <cstdlib>

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

	doInterferenceInsertion.resize(cache->cpuCount, numLeaderSets == totalSetNumber);

	samplePrivateMisses.resize(cache->cpuCount, MissCounter(0,0));
	sampleSharedMisses.resize(cache->cpuCount, MissCounter(0,0));

	interferenceMissProbabilities.resize(cache->cpuCount, InterferenceMissProbability(true));

	samplePrivateWritebacks.resize(cache->cpuCount, MissCounter(0,0));
	sampleSharedResponses.resize(cache->cpuCount, MissCounter(0,0));

	privateWritebackProbability.resize(cache->cpuCount, InterferenceMissProbability(false));

	cache->interferenceManager->registerCacheInterferenceObj(this);

	setsInConstituency = totalSetNumber / numLeaderSets;

	srand(240000);
}

void
CacheInterference::access(MemReqPtr& req, bool isCacheMiss){

	assert(cache->isShared);
	assert(!shadowTags.empty());

	if(cache->cpuCount > 1){

		bool shadowHit = false;
		bool shadowLeaderSet = false;

		// access tags to update LRU stack
		assert(cache->isShared);
		assert(req->adaptiveMHASenderID != -1);
		assert(req->cmd == Writeback || req->cmd == Read);

		int numberOfSets = shadowTags[req->adaptiveMHASenderID]->getNumSets();
		LRUBlk* shadowBlk = findShadowTagBlock(req, req->adaptiveMHASenderID);
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
			if(shadowBlk == NULL){
				if(curTick >= cache->detailedSimulationStartTick){
					samplePrivateMisses[req->adaptiveMHASenderID].increment(req, setsInConstituency);
				}
				cache->estimatedShadowMisses[req->adaptiveMHASenderID] += setsInConstituency;

			}
			cache->estimatedShadowAccesses[req->adaptiveMHASenderID] += setsInConstituency;
		}

		if(curTick >= cache->detailedSimulationStartTick){

			if(isCacheMiss){
				sampleSharedMisses[req->adaptiveMHASenderID].increment(req);
			}
		}

		if(!cache->useUniformPartitioning
		   && curTick >= cache->detailedSimulationStartTick
		   && doInterferenceInsertion[req->adaptiveMHASenderID]){

			if(numberOfSets == numLeaderSets){
				if(shadowHit && isCacheMiss){
					tagAsInterferenceMiss(req);
				}
			}
			else{
				if(addAsInterference(interferenceMissProbabilities[req->adaptiveMHASenderID].get(req->cmd))){
					tagAsInterferenceMiss(req);
				}
			}
		}
	}
}

bool
CacheInterference::addAsInterference(double probability){

	double randNum = rand() / (double) RAND_MAX;

	if(randNum < probability){
		return true;
	}
	return false;
}

void
CacheInterference::handleResponse(MemReqPtr& req, MemReqList writebacks){

	assert(cache->isShared);
	assert(!shadowTags.empty());
	assert(req->adaptiveMHASenderID != -1);
	assert(req->cmd == Read);

	if(cache->cpuCount > 1){

		if(curTick >= cache->detailedSimulationStartTick && !writebacks.empty()){
			sampleSharedResponses[req->adaptiveMHASenderID].increment(req, writebacks.size());
		}

		LRUBlk* currentBlk = findShadowTagBlockNoUpdate(req, req->adaptiveMHASenderID);
		int shadowSet = shadowTags[req->adaptiveMHASenderID]->extractSet(req->paddr);
		bool isShadowLeaderSet = isLeaderSet(shadowSet);

		if(currentBlk == NULL){
			assert(req->isShadowMiss);
			LRU::BlkList shadow_compress_list;
			MemReqList shadow_writebacks;
			LRUBlk *shadowBlk = shadowTags[req->adaptiveMHASenderID]->findReplacement(req, shadow_writebacks, shadow_compress_list);
			assert(shadow_writebacks.empty()); // writebacks are not generated in findReplacement()

			if(shadowBlk->isModified()){
				if(numLeaderSets == totalSetNumber){
					cache->shadowTagWritebacks[req->adaptiveMHASenderID]++;

					if(cache->writebackOwnerPolicy == BaseCache::WB_POLICY_SHADOW_TAGS){
						issuePrivateWriteback(req->adaptiveMHASenderID,
								shadowTags[req->adaptiveMHASenderID]->regenerateBlkAddr(shadowBlk->tag, shadowBlk->set));
					}
				}
				else if(isShadowLeaderSet){
					if(curTick >= cache->detailedSimulationStartTick){
						samplePrivateWritebacks[req->adaptiveMHASenderID].increment(req, setsInConstituency);
					}
					cache->shadowTagWritebacks[req->adaptiveMHASenderID] += setsInConstituency;
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


		// Compute cache capacity interference penalty and inform the InterferenceManager
		if(req->interferenceMissAt > 0 && !cache->useUniformPartitioning){

			int extraDelay = (curTick + cache->getHitLatency()) - req->interferenceMissAt;
			req->cacheCapacityInterference += extraDelay;
			cache->extraMissLatency[req->adaptiveMHASenderID] += extraDelay;
			cache->numExtraResponses[req->adaptiveMHASenderID]++;

			assert(cache->interferenceManager != NULL);
			cache->interferenceManager->addInterference(InterferenceManager::CacheCapacity, req, extraDelay);
		}

		if(doInterferenceInsertion[req->adaptiveMHASenderID]
		   && numLeaderSets < totalSetNumber
		   && cache->writebackOwnerPolicy == BaseCache::WB_POLICY_SHADOW_TAGS
		   && !cache->useUniformPartitioning
		   && !writebacks.empty()
		   && addAsInterference(privateWritebackProbability[req->adaptiveMHASenderID].get(Read))){

			MemReqPtr writeback = writebacks.front();
			issuePrivateWriteback(req->adaptiveMHASenderID, writeback->paddr);
		}
	}
}

void
CacheInterference::issuePrivateWriteback(int cpuID, Addr addr){
	MemReqPtr virtualWriteback = new MemReq();
	virtualWriteback->cmd = VirtualPrivateWriteback;
	virtualWriteback->paddr = addr;
	virtualWriteback->adaptiveMHASenderID = cpuID;
	cache->issueVirtualPrivateWriteback(virtualWriteback);
}

void
CacheInterference::tagAsInterferenceMiss(MemReqPtr& req){
	req->interferenceMissAt = curTick + cache->getHitLatency();
	cache->numExtraMisses[req->adaptiveMHASenderID]++;
}

void
CacheInterference::computeInterferenceProbabilities(int cpuID){

	// Update interference miss probability
	interferenceMissProbabilities[cpuID].update(samplePrivateMisses[cpuID], sampleSharedMisses[cpuID]);
	samplePrivateMisses[cpuID].reset();
	sampleSharedMisses[cpuID].reset();

	// Update private writeback probability
	assert(samplePrivateWritebacks[cpuID].writes == 0);
	assert(sampleSharedResponses[cpuID].writes == 0);

	privateWritebackProbability[cpuID].update(samplePrivateWritebacks[cpuID], sampleSharedResponses[cpuID]);
	assert(privateWritebackProbability[cpuID].writeInterferenceProbability == 0.0);

	samplePrivateWritebacks[cpuID].reset();
	sampleSharedResponses[cpuID].reset();
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

LRUBlk*
CacheInterference::findShadowTagBlock(MemReqPtr& req, int cpuID){
	int hitLat = cache->getHitLatency();
	return shadowTags[cpuID]->findBlock(req, hitLat);
}

LRUBlk*
CacheInterference::findShadowTagBlockNoUpdate(MemReqPtr& req, int cpuID){
	return shadowTags[cpuID]->findBlock(req->paddr, req->asid);
}

CacheInterference::InterferenceMissProbability::InterferenceMissProbability(bool _doInterferenceProb){
	readInterferenceProbability = 0.0;
	writeInterferenceProbability = 0.0;

	doInterferenceProb = _doInterferenceProb;
}

void
CacheInterference::InterferenceMissProbability::update(MissCounter privateEstimate, MissCounter sharedMeasurement){

	if(doInterferenceProb){
		double readInterference = (double) sharedMeasurement.reads - (double) privateEstimate.reads;
		readInterferenceProbability = computeProbability(readInterference, (double) sharedMeasurement.reads);
	}
	else{
		readInterferenceProbability = computeProbability((double) privateEstimate.reads, (double) sharedMeasurement.reads);
	}

	if(doInterferenceProb){

		double writeInterference = (double) sharedMeasurement.writes - (double) privateEstimate.writes;
		writeInterferenceProbability = computeProbability(writeInterference, (double) sharedMeasurement.writes);
	}
	else{
		assert(sharedMeasurement.writes == 0);
		assert(privateEstimate.writes == 0);
	}
}

double
CacheInterference::InterferenceMissProbability::computeProbability(double occurrences, double total){
	if(total == 0.0) return 0.0;
	if(occurrences <= 0.0) return 0.0;
	if(occurrences > total) return 1.0;
	return occurrences / total;
}

double
CacheInterference::InterferenceMissProbability::get(MemCmd cmd){
	if(cmd == Read){
		return readInterferenceProbability;
	}
	else if(cmd == Writeback){
		return writeInterferenceProbability;
	}

	fatal("Unknown memory command");
	return 0.0;
}

void
CacheInterference::MissCounter::increment(MemReqPtr& req, int amount){
	if(req->cmd == Read){
		reads += amount;
	}
	else if(req->cmd == Writeback){
		writes += amount;
	}
	else{
		fatal("Unsupported memory command encountered");
	}
}

void
CacheInterference::MissCounter::reset(){
	reads = 0;
	writes = 0;
}
