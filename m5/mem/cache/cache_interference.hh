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

public:
	class MissCounter;

	class InterferenceMissProbability{
	public:
		double readInterferenceProbability;
		double writeInterferenceProbability;

		InterferenceMissProbability(bool _doInterferenceProb);

		void update(MissCounter privateEstimate, MissCounter sharedEstimate);

		double get(MemCmd req);

	private:
		bool doInterferenceProb;
		double computeProbability(double occurrences, double total);

	};

	class MissCounter{
	public:
		int reads;
		int writes;

		MissCounter(int _reads, int _writes)
		: reads(_reads), writes(_writes) {}

		void increment(MemReqPtr& req, int amount = 1);

		void reset();
	};

protected:
    Stats::Vector<> extraMissLatency;
    Stats::Vector<> numExtraResponses;
    Stats::Vector<> numExtraMisses;

    Stats::Vector<> shadowTagWritebacks;

	Stats::Vector<> estimatedShadowAccesses;
	Stats::Vector<> estimatedShadowMisses;
	Stats::Formula estimatedShadowMissRate;
	Stats::Formula estimatedShadowInterferenceMisses;

	Stats::VectorDistribution<> interferenceMissDistanceDistribution;
	Stats::VectorDistribution<> privateWritebackDistribution;

private:

	int numLeaderSets;
	int totalSetNumber;
	int numBanks;
	BaseCache* cache;
	int setsInConstituency;

	std::vector<LRU*> shadowTags;

	std::vector<bool> doInterferenceInsertion;

	std::vector<InterferenceMissProbability> interferenceMissProbabilities;

	std::vector<InterferenceMissProbability> privateWritebackProbability;

	std::vector<MissCounter> samplePrivateMisses;
	std::vector<MissCounter> sampleSharedMisses;

	std::vector<MissCounter> sampleSharedResponses;
	std::vector<MissCounter> samplePrivateWritebacks;

	std::vector<int> missesSinceLastInterferenceMiss;
	std::vector<int> sharedWritebacksSinceLastPrivWriteback;

    bool isLeaderSet(int set);

    void issuePrivateWriteback(int cpuID, Addr addr);

    void tagAsInterferenceMiss(MemReqPtr& req);

    bool addAsInterference(double probability);

    LRUBlk* findShadowTagBlock(MemReqPtr& req, int cpuID);

    LRUBlk* findShadowTagBlockNoUpdate(MemReqPtr& req, int cpuID);

public:

	CacheInterference(){ }

	CacheInterference(int _numLeaderSets, int _totalSetNumber, int _bankCount, std::vector<LRU*> _shadowTags, BaseCache* _cache);

	int getNumLeaderSets(){
		return numLeaderSets;
	}

	void access(MemReqPtr& req, bool isCacheMiss);

	void handleResponse(MemReqPtr& req, MemReqList writebacks);

	void computeInterferenceProbabilities(int cpuID);

	void initiateInterferenceInsertions(int cpuID){
		doInterferenceInsertion[cpuID] = true;
	}

	bool interferenceInsertionsInitiated(int cpuID){
		return doInterferenceInsertion[cpuID];
	}

	void regStats(std::string name);
};

#endif /* CACHE_INTERFERENCE_HH_ */
