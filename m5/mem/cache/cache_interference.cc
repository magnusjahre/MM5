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

CacheInterference::CacheInterference(std::string _name,
		                             int _cpuCount,
		                             int _numLeaderSets,
		                             int _size,
		                             int _numBits,
		                             InterferenceProbabilityPolicy _ipp,
		                             InterferenceManager* _intman,
		                             int _blockSize,
		                             int _assoc,
		                             int _hitLat,
		                             int _divFac,
		                             HierParams* hp)
: BaseHier(_name, hp){

	size = _size;

	totalSetNumber = size / (_blockSize * _assoc);
	cpuCount = _cpuCount;
	intProbabilityPolicy = _ipp;

	shadowTags = vector<LRU*>(cpuCount, NULL);

	for(int i=0;i<cpuCount;i++){
		shadowTags[i] = new LRU(totalSetNumber,
								_blockSize,
								_assoc,
								_hitLat,
								1,
								true,
								_divFac,
								-1, // max use ways
								i);
		shadowTags[i]->setCacheInterference(this);
	}

	numLeaderSets = _numLeaderSets;
	if(numLeaderSets == 0) numLeaderSets = totalSetNumber; // 0 means full-map
	if(totalSetNumber % numLeaderSets  != 0){
		fatal("The the total number for sets must be divisible by the number of leader sets");
	}
	assert(numLeaderSets <= totalSetNumber && numLeaderSets > 0);

	doInterferenceInsertion.resize(cpuCount, numLeaderSets == totalSetNumber);

	if(intProbabilityPolicy == IPP_COUNTER_FIXED_INTMAN || intProbabilityPolicy == IPP_COUNTER_FIXED_PRIVATE){
		randomCounterBits = _numBits;
		requestCounters.resize(cpuCount, FixedWidthCounter(false, randomCounterBits));
		responseCounters.resize(cpuCount, FixedWidthCounter(false, randomCounterBits));
	}
	else if(intProbabilityPolicy == IPP_FULL_RANDOM_FLOAT){
		randomCounterBits = 0;
		requestCounters.resize(cpuCount, FixedWidthCounter(false, 0));
		responseCounters.resize(cpuCount, FixedWidthCounter(false, 0));
	}
	else if(intProbabilityPolicy == IPP_SEQUENTIAL_INSERT){
		for(int i=0;i<doInterferenceInsertion.size();i++) doInterferenceInsertion[i] = true;
	}
	else{
		fatal("cache interference probability policy not set");
	}

	sequentialReadCount.resize(cpuCount, 0);
	sequentialWritebackCount.resize(cpuCount, 0);


	samplePrivateMisses.resize(cpuCount, MissCounter(0));
	sampleSharedMisses.resize(cpuCount, MissCounter(0));

	interferenceMissProbabilities.resize(cpuCount, InterferenceMissProbability(true, randomCounterBits));

	samplePrivateWritebacks.resize(cpuCount, MissCounter(0));
	sampleSharedResponses.resize(cpuCount, MissCounter(0));

	privateWritebackProbability.resize(cpuCount, InterferenceMissProbability(false, randomCounterBits));

	_intman->registerCacheInterferenceObj(this);

	setsInConstituency = totalSetNumber / numLeaderSets;

	missesSinceLastInterferenceMiss.resize(cpuCount, 0);
	sharedResponsesSinceLastPrivWriteback.resize(cpuCount, 0);


	readMissAccumulator.resize(cpuCount, 0);
	wbMissAccumulator.resize(cpuCount, 0);
	writebackAccumulator.resize(cpuCount, 0);
	interferenceMissAccumulator.resize(cpuCount, 0);
	accessAccumulator.resize(cpuCount, 0);

	srand(240000);
}

void
CacheInterference::regStats(){

	using namespace Stats;

    extraMissLatency
        .init(cpuCount)
        .name(name() + ".cpu_extra_latency")
        .desc("extra latency due to blocks being evicted from the cache")
        .flags(total)
        ;

    numExtraResponses
        .init(cpuCount)
        .name(name() + ".cpu_extra_responses")
        .desc("number of responses to extra misses measured with shadow tags")
        .flags(total)
        ;

    numExtraMisses
        .init(cpuCount)
        .name(name() + ".cpu_extra_misses")
        .desc("number of extra misses measured with shadow tags")
        .flags(total)
        ;

    shadowTagWritebacks
		.init(cpuCount)
		.name(name() + ".shadow_tag_writebacks")
		.desc("the number of writebacks detected in the shadowtags")
		.flags(total)
		;

    estimatedShadowAccesses
        .init(cpuCount)
        .name(name() + ".estimated_shadow_accesses")
        .desc("number of shadow accesses from sampled shadow tags")
        .flags(total)
        ;

    estimatedShadowMisses
		.init(cpuCount)
		.name(name() + ".estimated_shadow_misses")
		.desc("number of shadow misses from sampled shadow tags")
		.flags(total)
		;

    estimatedShadowMissRate
		.name(name() + ".estimated_shadow_miss_rate")
		.desc("miss rate estimate from sampled shadow tags")
		;

    estimatedShadowMissRate = estimatedShadowMisses / estimatedShadowAccesses;

//    estimatedShadowInterferenceMisses
//		.name(name() + ".estimated_shadow_interference_misses")
//		.desc("interference miss estimate from sampled shadow tags")
//		;
//
//    estimatedShadowInterferenceMisses = cache->missesPerCPU - estimatedShadowMisses;

    interferenceMissDistanceDistribution
		.init(cpuCount, 1, 50, 1)
		.name(name() + ".interference_miss_distance_distribution")
        .desc("The distribution of misses between each interference miss")
        .flags(total | pdf | cdf)
		;

    privateWritebackDistribution
    		.init(cpuCount, 1, 50, 1)
    		.name(name() + ".private_writeback_distance_distribution")
            .desc("The distribution of the number of shared-mode writebacks between each private-mode writeback")
            .flags(total | pdf | cdf)
    		;

    for(int i=0;i<shadowTags.size();i++){
        stringstream tmp;
        tmp << i;
        shadowTags[i]->regStats(name()+".shadowtags.cpu"+tmp.str());
    }
}

void
CacheInterference::access(MemReqPtr& req, bool isCacheMiss, int hitLat, Tick detailedSimStart){

	assert(!shadowTags.empty());

	bool shadowHit = false;
	bool shadowLeaderSet = false;

	// access tags to update LRU stack
	assert(req->adaptiveMHASenderID != -1);
	assert(req->cmd == Writeback || req->cmd == Read);

	accessAccumulator[req->adaptiveMHASenderID]++;
	if(isCacheMiss){
		if(req->cmd == Read) readMissAccumulator[req->adaptiveMHASenderID]++;
		else wbMissAccumulator[req->adaptiveMHASenderID]++;
	}

	int numberOfSets = shadowTags[req->adaptiveMHASenderID]->getNumSets();

	int shadowSet = shadowTags[req->adaptiveMHASenderID]->extractSet(req->paddr);
	shadowLeaderSet = isLeaderSet(shadowSet);

	LRUBlk* shadowBlk = findShadowTagBlock(req, req->adaptiveMHASenderID, shadowLeaderSet, hitLat);


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
		if(shadowBlk == NULL){ // shadow miss
			if(curTick >= detailedSimStart){
				samplePrivateMisses[req->adaptiveMHASenderID].increment(req, setsInConstituency);
			}
			estimatedShadowMisses[req->adaptiveMHASenderID] += setsInConstituency;
		}
		else{ // shadow hit
			if(isCacheMiss && curTick >= detailedSimStart){
				// shared cache miss and shadow hit -> need to tag reqs as interference requests
				sequentialReadCount[req->adaptiveMHASenderID] += setsInConstituency;
				interferenceMissAccumulator[req->adaptiveMHASenderID] += setsInConstituency;
			}
		}

		estimatedShadowAccesses[req->adaptiveMHASenderID] += setsInConstituency;
	}
	if(curTick >= detailedSimStart){
		if(isCacheMiss){
			sampleSharedMisses[req->adaptiveMHASenderID].increment(req);
		}
	}

	doAccessStatistics(numberOfSets, req, isCacheMiss, shadowHit);

	if(curTick >= detailedSimStart
       && doInterferenceInsertion[req->adaptiveMHASenderID]
	   && cpuCount > 1){

		if(numberOfSets == numLeaderSets){
			if(shadowHit && isCacheMiss){
				tagAsInterferenceMiss(req, hitLat);
			}
		}
		else{
			if(addAsInterference(interferenceMissProbabilities[req->adaptiveMHASenderID].get(req->cmd), req->adaptiveMHASenderID, true)){
				tagAsInterferenceMiss(req, hitLat);
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
}

bool
CacheInterference::addAsInterference(FixedPointProbability probability, int cpuID, bool useRequestCounter){

	assert(numLeaderSets < totalSetNumber);

	if(intProbabilityPolicy == IPP_COUNTER_FIXED_INTMAN){

		FixedWidthCounter* counter = NULL;
		if(useRequestCounter) counter = &requestCounters[cpuID];
		else counter = &responseCounters[cpuID];

		bool doInsertion = probability.doInsertion(counter);

		counter->inc();

		return doInsertion;
	}
	else if(intProbabilityPolicy == IPP_FULL_RANDOM_FLOAT){
		double randNum = rand() / (double) RAND_MAX;

		if(randNum < probability.getFloatValue()){
			return true;
		}
		return false;
	}
	else if(intProbabilityPolicy == IPP_SEQUENTIAL_INSERT){
		int* counter = 0;
		if(useRequestCounter) counter = &sequentialReadCount[cpuID];
		else counter = &sequentialWritebackCount[cpuID];

		if(*counter > 0){
			(*counter)--;
			return true;
		}
		return false;
	}
	else if(intProbabilityPolicy == IPP_COUNTER_FIXED_PRIVATE){
		fatal("private counter based policy interference determination not implemented");
	}
	else{
		fatal("unknown interference probabilty policy");
	}
}

void
CacheInterference::doShadowReplacement(MemReqPtr& req, BaseCache* cache){

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
				issuePrivateWriteback(req->adaptiveMHASenderID, wbAddr, cache);
			}

			privateWritebackDistribution[req->adaptiveMHASenderID].sample(sharedResponsesSinceLastPrivWriteback[req->adaptiveMHASenderID]);
			sharedResponsesSinceLastPrivWriteback[req->adaptiveMHASenderID] = 0;
		}
		else if(isShadowLeaderSet){

			if(curTick >= cache->detailedSimulationStartTick){
				samplePrivateWritebacks[req->adaptiveMHASenderID].increment(req, setsInConstituency);
				sequentialWritebackCount[req->adaptiveMHASenderID] += setsInConstituency;
			}

			shadowTagWritebacks[req->adaptiveMHASenderID] += setsInConstituency;
		}
	}

	// set block values to the values of the new occupant
	shadowBlk->tag = shadowTags[req->adaptiveMHASenderID]->extractTag(req->paddr, shadowBlk);
	shadowBlk->asid = req->asid;
	assert(req->xc);
	shadowBlk->xc = req->xc;
	shadowBlk->status = BlkValid;
	shadowBlk->origRequestingCpuID = req->adaptiveMHASenderID;
	assert(!shadowBlk->isModified());
}

void
CacheInterference::handleResponse(MemReqPtr& req, MemReqList writebacks, BaseCache* cache){

	assert(!shadowTags.empty());
	assert(req->adaptiveMHASenderID != -1);
	assert(req->cmd == Read);

	if(curTick >= cache->detailedSimulationStartTick){
		sampleSharedResponses[req->adaptiveMHASenderID].increment(req);
		sharedResponsesSinceLastPrivWriteback[req->adaptiveMHASenderID]++;
	}

	writebackAccumulator[req->adaptiveMHASenderID] += writebacks.size();

	LRUBlk* currentBlk = findShadowTagBlockNoUpdate(req, req->adaptiveMHASenderID);
	if(currentBlk == NULL){
		doShadowReplacement(req, cache);
	}

	// Compute cache capacity interference penalty and inform the InterferenceManager
	if(req->interferenceMissAt > 0 && cpuCount > 1){

		int extraDelay = (curTick + cache->getHitLatency()) - req->interferenceMissAt;
		req->cacheCapacityInterference += extraDelay;
		extraMissLatency[req->adaptiveMHASenderID] += extraDelay;
		numExtraResponses[req->adaptiveMHASenderID]++;

		assert(cache->interferenceManager != NULL);
		cache->interferenceManager->addInterference(InterferenceManager::CacheCapacity, req, extraDelay);
	}

	if(doInterferenceInsertion[req->adaptiveMHASenderID]
	                           && cpuCount > 1
	                           && numLeaderSets < totalSetNumber
	                           && cache->writebackOwnerPolicy == BaseCache::WB_POLICY_SHADOW_TAGS
	                           && addAsInterference(privateWritebackProbability[req->adaptiveMHASenderID].get(Read), req->adaptiveMHASenderID, false)){

		int set = shadowTags[req->adaptiveMHASenderID]->extractSet(req->paddr);
		issuePrivateWriteback(req->adaptiveMHASenderID, MemReq::inval_addr, cache, set);
	}


}

void
CacheInterference::issuePrivateWriteback(int cpuID, Addr addr, BaseCache* cache, int cacheSet){
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
CacheInterference::tagAsInterferenceMiss(MemReqPtr& req, int hitLat){
	req->interferenceMissAt = curTick + hitLat;
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
CacheInterference::findShadowTagBlock(MemReqPtr& req, int cpuID, bool isLeaderSet, int hitLat){
	return shadowTags[cpuID]->findBlock(req, hitLat, isLeaderSet, setsInConstituency);
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

std::vector<CacheMissMeasurements>
CacheInterference::getMissMeasurementSample(){
	std::vector<CacheMissMeasurements> missMeasurements;

	for(int i=0;i<cpuCount;i++){
		vector<int> hits = shadowTags[i]->getHitDistribution();
		vector<int> cumulativeMisses = vector<int>(hits.size(), 0);
		for(int j=0;j<hits.size();j++) cumulativeMisses[j] =  accessAccumulator[i] - hits[j];
		shadowTags[i]->resetHitCounters();

		CacheMissMeasurements tmp(readMissAccumulator[i],
							      wbMissAccumulator[i],
								  interferenceMissAccumulator[i],
								  accessAccumulator[i],
								  writebackAccumulator[i],
								  cumulativeMisses);

		readMissAccumulator[i] = 0;
		wbMissAccumulator[i] = 0;
		interferenceMissAccumulator[i] = 0;
		accessAccumulator[i] = 0;
		writebackAccumulator[i] = 0;

		missMeasurements.push_back(tmp);
	}

	return missMeasurements;
}


void
CacheInterference::serialize(std::ostream &os){
	assert(cpuCount == 1);
	shadowTags[0]->serialize(os, name());
}

void
CacheInterference::unserialize(Checkpoint *cp, const std::string &section){
	string* filenames = new string[cpuCount];
	UNSERIALIZE_ARRAY(filenames, cpuCount);
	for(int i=0;i<cpuCount;i++){
		shadowTags[i]->unserialize(cp, section, filenames[i]);
	}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(CacheInterference)
	Param<int> cpuCount;
	Param<int> leaderSets;
	Param<int> size;
    Param<string> interference_probability_policy;
    Param<int> ipp_bits;
    SimObjectParam<InterferenceManager* > interferenceManager;
    Param<int> blockSize;
    Param<int> assoc;
    Param<int> hitLatency;
    Param<int> divisionFactor;
END_DECLARE_SIM_OBJECT_PARAMS(CacheInterference)

BEGIN_INIT_SIM_OBJECT_PARAMS(CacheInterference)
	INIT_PARAM(cpuCount, "Number of cores"),
	INIT_PARAM_DFLT(leaderSets, "Number of leader sets", 64),
	INIT_PARAM(size, "The size of the cache in bytes"),
    INIT_PARAM_DFLT(interference_probability_policy, "interference probability policy to use", "float"),
    INIT_PARAM_DFLT(ipp_bits, "The resolution of the probability (used in a subset of IPP modes)", 6),
    INIT_PARAM(interferenceManager, "Pointer to the interference manager"),
    INIT_PARAM(blockSize, "Cache block size"),
    INIT_PARAM(assoc, "Associativity"),
    INIT_PARAM(hitLatency, "The cache hit latency"),
    INIT_PARAM(divisionFactor, "The number of cores in shared mode when run in private mode")
END_INIT_SIM_OBJECT_PARAMS(CacheInterference)

CREATE_SIM_OBJECT(CacheInterference)
{

    CacheInterference::InterferenceProbabilityPolicy ipp;
    string ipp_name = interference_probability_policy;
    if(ipp_name == "float"){
    	ipp = CacheInterference::IPP_FULL_RANDOM_FLOAT;
    }
    else if(ipp_name == "fixed"){
    	ipp = CacheInterference::IPP_COUNTER_FIXED_INTMAN;
    }
    else if(ipp_name == "fixed-private"){
    	ipp = CacheInterference::IPP_COUNTER_FIXED_PRIVATE;
    }
    else if(ipp_name == "sequential"){
    	ipp = CacheInterference::IPP_SEQUENTIAL_INSERT;
    }
    else{
    	fatal("Unknown interference probability policy provided");
    }

    HierParams* hp = new HierParams("InterferenceHier", false, true, cpuCount);

	return new CacheInterference(getInstanceName(),
								 cpuCount,
								 leaderSets,
								 size,
								 ipp_bits,
								 ipp,
								 interferenceManager,
								 blockSize,
								 assoc,
								 hitLatency,
								 divisionFactor,
								 hp);
}

REGISTER_SIM_OBJECT("CacheInterference", CacheInterference)

#endif //DOXYGEN_SHOULD_SKIP_THIS
