/*
 * cache_interference.cc
 *
 *  Created on: Aug 13, 2009
 *      Author: jahre
 */

#include "cache_interference.hh"
#include <cstdlib>
#include <cmath>

using namespace std;

CacheInterference::CacheInterference(int _numLeaderSets, int _totalSetNumber, int _numBanks, std::vector<LRU*> _shadowTags, BaseCache* _cache, int _numBits){

	totalSetNumber = _totalSetNumber;
	numBanks = _numBanks;
	shadowTags = _shadowTags;
	cache = _cache;

	if(cache->intProbabilityPolicy == BaseCache::IPP_COUNTER_FIXED_INTMAN || cache->intProbabilityPolicy == BaseCache::IPP_COUNTER_FIXED_PRIVATE){
		randomCounterBits = _numBits;
		requestCounters.resize(cache->cpuCount, FixedWidthCounter(false, randomCounterBits));
		responseCounters.resize(cache->cpuCount, FixedWidthCounter(false, randomCounterBits));
	}
	else if(cache->intProbabilityPolicy == BaseCache::IPP_FULL_RANDOM_FLOAT){
		randomCounterBits = 0;
		requestCounters.resize(cache->cpuCount, FixedWidthCounter(false, 0));
		responseCounters.resize(cache->cpuCount, FixedWidthCounter(false, 0));
	}
	else{
		fatal("cache interference probability policy not set");
	}


    numLeaderSets = _numLeaderSets / numBanks;
    if(numLeaderSets == 0) numLeaderSets = totalSetNumber; // 0 means full-map

	if(totalSetNumber % numLeaderSets  != 0){
		fatal("The the total number for sets must be divisible by the number of leader sets");
	}
	assert(numLeaderSets <= totalSetNumber && numLeaderSets > 0);

	doInterferenceInsertion.resize(cache->cpuCount, numLeaderSets == totalSetNumber);

	samplePrivateMisses.resize(cache->cpuCount, MissCounter(0));
	sampleSharedMisses.resize(cache->cpuCount, MissCounter(0));

	interferenceMissProbabilities.resize(cache->cpuCount, InterferenceMissProbability(true, randomCounterBits));

	samplePrivateWritebacks.resize(cache->cpuCount, MissCounter(0));
	sampleSharedResponses.resize(cache->cpuCount, MissCounter(0));

	privateWritebackProbability.resize(cache->cpuCount, InterferenceMissProbability(false, randomCounterBits));

	cache->interferenceManager->registerCacheInterferenceObj(this);

	setsInConstituency = totalSetNumber / numLeaderSets;

	missesSinceLastInterferenceMiss.resize(cache->cpuCount, 0);
	sharedResponsesSinceLastPrivWriteback.resize(cache->cpuCount, 0);

	srand(240000);
}

void
CacheInterference::regStats(string name){

	using namespace Stats;

    extraMissLatency
        .init(cache->cpuCount)
        .name(name + ".cpu_extra_latency")
        .desc("extra latency due to blocks being evicted from the cache")
        .flags(total)
        ;

    numExtraResponses
        .init(cache->cpuCount)
        .name(name + ".cpu_extra_responses")
        .desc("number of responses to extra misses measured with shadow tags")
        .flags(total)
        ;

    numExtraMisses
        .init(cache->cpuCount)
        .name(name + ".cpu_extra_misses")
        .desc("number of extra misses measured with shadow tags")
        .flags(total)
        ;

    shadowTagWritebacks
		.init(cache->cpuCount)
		.name(name + ".shadow_tag_writebacks")
		.desc("the number of writebacks detected in the shadowtags")
		.flags(total)
		;

    estimatedShadowAccesses
        .init(cache->cpuCount)
        .name(name + ".estimated_shadow_accesses")
        .desc("number of shadow accesses from sampled shadow tags")
        .flags(total)
        ;

    estimatedShadowMisses
		.init(cache->cpuCount)
		.name(name + ".estimated_shadow_misses")
		.desc("number of shadow misses from sampled shadow tags")
		.flags(total)
		;

    estimatedShadowMissRate
		.name(name + ".estimated_shadow_miss_rate")
		.desc("miss rate estimate from sampled shadow tags")
		;

    estimatedShadowMissRate = estimatedShadowMisses / estimatedShadowAccesses;

    estimatedShadowInterferenceMisses
		.name(name + ".estimated_shadow_interference_misses")
		.desc("interference miss estimate from sampled shadow tags")
		;

    estimatedShadowInterferenceMisses = cache->missesPerCPU - estimatedShadowMisses;

    interferenceMissDistanceDistribution
		.init(cache->cpuCount, 1, 50, 1)
		.name(name + ".interference_miss_distance_distribution")
        .desc("The distribution of misses between each interference miss")
        .flags(total | pdf | cdf)
		;

    privateWritebackDistribution
    		.init(cache->cpuCount, 1, 50, 1)
    		.name(name + ".private_writeback_distance_distribution")
            .desc("The distribution of the number of shared-mode writebacks between each private-mode writeback")
            .flags(total | pdf | cdf)
    		;
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
				estimatedShadowMisses[req->adaptiveMHASenderID] += setsInConstituency;

			}
			estimatedShadowAccesses[req->adaptiveMHASenderID] += setsInConstituency;
		}

		doAccessStatistics(numberOfSets, req, isCacheMiss, shadowHit);

		if(!cache->useUniformPartitioning
		   && curTick >= cache->detailedSimulationStartTick
		   && doInterferenceInsertion[req->adaptiveMHASenderID]){

			if(numberOfSets == numLeaderSets){
				if(shadowHit && isCacheMiss){
					tagAsInterferenceMiss(req);
				}
			}
			else{
				if(addAsInterference(interferenceMissProbabilities[req->adaptiveMHASenderID].get(req->cmd), req->adaptiveMHASenderID, true)){
					tagAsInterferenceMiss(req);
				}
			}
		}
	}
}

void
CacheInterference::doAccessStatistics(int numberOfSets, MemReqPtr& req, bool isCacheMiss, bool isShadowHit){
	if(numberOfSets == numLeaderSets){
		if(isCacheMiss){

			missesSinceLastInterferenceMiss[req->adaptiveMHASenderID]++;

			if(isShadowHit){
				interferenceMissDistanceDistribution[req->adaptiveMHASenderID].sample(missesSinceLastInterferenceMiss[req->adaptiveMHASenderID]);
				missesSinceLastInterferenceMiss[req->adaptiveMHASenderID] = 0;
			}

		}
	}

	if(curTick >= cache->detailedSimulationStartTick){

		if(isCacheMiss){
			sampleSharedMisses[req->adaptiveMHASenderID].increment(req);
		}
	}
}

bool
CacheInterference::addAsInterference(FixedPointProbability probability, int cpuID, bool useRequestCounter){

	if(cache->intProbabilityPolicy == BaseCache::IPP_COUNTER_FIXED_INTMAN){

		FixedWidthCounter* counter = NULL;
		if(useRequestCounter) counter = &requestCounters[cpuID];
		else counter = &responseCounters[cpuID];

		bool doInsertion = probability.doInsertion(counter);

		counter->inc();

		return doInsertion;
	}
	else if(cache->intProbabilityPolicy == BaseCache::IPP_FULL_RANDOM_FLOAT){
		double randNum = rand() / (double) RAND_MAX;

		if(randNum < probability.getFloatValue()){
			return true;
		}
		return false;
	}
	else if(cache->intProbabilityPolicy == BaseCache::IPP_COUNTER_FIXED_PRIVATE){
		fatal("private counter based policy interference determination not implemented");
	}
	else{
		fatal("unknown interference probabilty policy");
	}
}

void
CacheInterference::doShadowReplacement(MemReqPtr& req){

	int shadowSet = shadowTags[req->adaptiveMHASenderID]->extractSet(req->paddr);
	bool isShadowLeaderSet = isLeaderSet(shadowSet);

	assert(req->isShadowMiss);
	LRU::BlkList shadow_compress_list;
	MemReqList shadow_writebacks;
	LRUBlk *shadowBlk = shadowTags[req->adaptiveMHASenderID]->findReplacement(req, shadow_writebacks, shadow_compress_list);
	assert(shadow_writebacks.empty()); // writebacks are not generated in findReplacement()

	if(shadowBlk->isModified()){
		if(numLeaderSets == totalSetNumber){
			shadowTagWritebacks[req->adaptiveMHASenderID]++;

			if(cache->writebackOwnerPolicy == BaseCache::WB_POLICY_SHADOW_TAGS){
				Addr wbAddr = shadowTags[req->adaptiveMHASenderID]->regenerateBlkAddr(shadowBlk->tag, shadowBlk->set);
				issuePrivateWriteback(req->adaptiveMHASenderID, wbAddr);
			}

			privateWritebackDistribution[req->adaptiveMHASenderID].sample(sharedResponsesSinceLastPrivWriteback[req->adaptiveMHASenderID]);
			sharedResponsesSinceLastPrivWriteback[req->adaptiveMHASenderID] = 0;
		}
		else if(isShadowLeaderSet){

			if(curTick >= cache->detailedSimulationStartTick){
				samplePrivateWritebacks[req->adaptiveMHASenderID].increment(req, setsInConstituency);
			}

			shadowTagWritebacks[req->adaptiveMHASenderID] += setsInConstituency;
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

void
CacheInterference::handleResponse(MemReqPtr& req, MemReqList writebacks){

	assert(cache->isShared);
	assert(!shadowTags.empty());
	assert(req->adaptiveMHASenderID != -1);
	assert(req->cmd == Read);

	if(cache->cpuCount > 1){

		if(curTick >= cache->detailedSimulationStartTick){
			sampleSharedResponses[req->adaptiveMHASenderID].increment(req);
			sharedResponsesSinceLastPrivWriteback[req->adaptiveMHASenderID]++;
		}

		LRUBlk* currentBlk = findShadowTagBlockNoUpdate(req, req->adaptiveMHASenderID);
		if(currentBlk == NULL){
			doShadowReplacement(req);
		}

		// Compute cache capacity interference penalty and inform the InterferenceManager
		if(req->interferenceMissAt > 0 && !cache->useUniformPartitioning){

			int extraDelay = (curTick + cache->getHitLatency()) - req->interferenceMissAt;
			req->cacheCapacityInterference += extraDelay;
			extraMissLatency[req->adaptiveMHASenderID] += extraDelay;
			numExtraResponses[req->adaptiveMHASenderID]++;

			assert(cache->interferenceManager != NULL);
			cache->interferenceManager->addInterference(InterferenceManager::CacheCapacity, req, extraDelay);
		}

		if(doInterferenceInsertion[req->adaptiveMHASenderID]
		   && numLeaderSets < totalSetNumber
		   && cache->writebackOwnerPolicy == BaseCache::WB_POLICY_SHADOW_TAGS
		   && !cache->useUniformPartitioning
		   && addAsInterference(privateWritebackProbability[req->adaptiveMHASenderID].get(Read), req->adaptiveMHASenderID, false)){

			int set = shadowTags[req->adaptiveMHASenderID]->extractSet(req->paddr);
			issuePrivateWriteback(req->adaptiveMHASenderID, MemReq::inval_addr, set);
		}

	}
}

void
CacheInterference::issuePrivateWriteback(int cpuID, Addr addr, int cacheSet){
	MemReqPtr virtualWriteback = new MemReq();
	virtualWriteback->cmd = VirtualPrivateWriteback;
	virtualWriteback->paddr = addr;
	virtualWriteback->adaptiveMHASenderID = cpuID;

	if(cacheSet != -1){
		virtualWriteback->sharedCacheSet = cacheSet;
	}

	cache->issueVirtualPrivateWriteback(virtualWriteback);
}

void
CacheInterference::tagAsInterferenceMiss(MemReqPtr& req){
	req->interferenceMissAt = curTick + cache->getHitLatency();
	numExtraMisses[req->adaptiveMHASenderID]++;
}

void
CacheInterference::computeInterferenceProbabilities(int cpuID){

	// Update interference miss probability
	interferenceMissProbabilities[cpuID].updateInterference(samplePrivateMisses[cpuID], sampleSharedMisses[cpuID]);
	samplePrivateMisses[cpuID].reset();
	sampleSharedMisses[cpuID].reset();

	// Update private writeback probability
	privateWritebackProbability[cpuID].update(samplePrivateWritebacks[cpuID], sampleSharedResponses[cpuID]);

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

CacheInterference::InterferenceMissProbability::InterferenceMissProbability(bool _doInterferenceProb, int _numBits){
	interferenceProbability.setBits(_numBits);
	numBits = _numBits;
}

void
CacheInterference::InterferenceMissProbability::update(MissCounter eventOccurences, MissCounter allOccurences){
	interferenceProbability.compute(eventOccurences.value, allOccurences.value);
}

void
CacheInterference::InterferenceMissProbability::updateInterference(MissCounter privateEstimate, MissCounter sharedMeasurement){
	int readInterference = sharedMeasurement.value - privateEstimate.value;
	interferenceProbability.compute(readInterference, sharedMeasurement.value);
}

CacheInterference::FixedPointProbability
CacheInterference::InterferenceMissProbability::get(MemCmd cmd){
	return interferenceProbability;
}

void
CacheInterference::MissCounter::increment(MemReqPtr& req, int amount){
	value += amount;
}

void
CacheInterference::MissCounter::reset(){
	value = 0;
}

CacheInterference::FixedPointProbability::FixedPointProbability(){
	ready = false;
}

void
CacheInterference::FixedPointProbability::setBits(int _numBits){
	ready = true;
	numBits = _numBits;
	max = (1 << numBits) - 1;
}

void
CacheInterference::FixedPointProbability::compute(int numerator, int denominator){
	assert(ready);

	if(denominator == 0){
		value = max;
		floatValue = 1.0;
	}
	else if(numerator <= 0){
		floatValue = 0.0;
		value = 0;
	}
	else if(numerator >= denominator){
		value = max;
		floatValue = 1.0;
	}
	else{

		floatValue = (double) ((double) numerator / (double) denominator);

		uint64_t tmpval = numerator << numBits;
		uint32_t result = tmpval / denominator;
		assert(result <= max);

		value = result;
	}
}

bool
CacheInterference::FixedPointProbability::doInsertion(FixedWidthCounter* counter){

	assert(ready);
	assert(counter->width() == numBits);

	if(counter->get() < value){
		return true;
	}
	return false;
}

void
CacheInterference::FixedWidthCounter::inc(){

	assert(max > 0);
	assert(value <= max);
	if(value == max){
		if(saturate){
			return;
		}
		else{
			value = 0;
		}
	}
	else{
		value++;
	}
}
