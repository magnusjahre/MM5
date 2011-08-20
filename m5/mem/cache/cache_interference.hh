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
#include "mem/policy/performance_measurement.hh"
#include "mem/interference_manager.hh"

#include <vector>
#include <iostream>

class BaseCache;
class InterferenceManager;
class LRU;
class LRUBlk;

class CacheInterference : public BaseHier{

public:

	enum InterferenceProbabilityPolicy{
		IPP_FULL_RANDOM_FLOAT, // float
		IPP_COUNTER_FIXED_INTMAN, // fixed
		IPP_COUNTER_FIXED_PRIVATE, // fixed-private
		IPP_SEQUENTIAL_INSERT,
		IPP_INVALID
	};

	InterferenceProbabilityPolicy intProbabilityPolicy;


	class MissCounter;
	class FixedWidthCounter;

	class FixedPointProbability{
	private:
		uint32_t value;
		int numBits;
		bool ready;
		uint32_t max;

		double floatValue;

	public:

		FixedPointProbability();

		void setBits(int _numBits);

		void compute(int numerator, int denominator);
		bool doInsertion(FixedWidthCounter* counter);

		bool isZero(){
			assert(ready);
			assert(numBits > 0);
			return value == 0;
		}

		bool isMax(){
			assert(ready);
			assert(numBits > 0);
			return value == max;
		}

		float getFloatValue(){
			assert(ready);
			return floatValue;
		}

	};

	class FixedWidthCounter{
	private:
		uint32_t value;
		bool saturate;
		uint32_t max;
		int bitWidth;

	public:
		FixedWidthCounter(){
			value = 0;
			saturate = false;
		}

		FixedWidthCounter(bool _saturate, int _numBits){
			value = 0;
			saturate = _saturate;
			bitWidth = _numBits;
			assert(_numBits < 32);
			max = (1 << _numBits)-1;
		}

		void inc();

		uint32_t get(){
			assert(max > 0);
			return value;
		}

		int width(){
			return bitWidth;
		}
	};

	class InterferenceMissProbability{
	public:
		FixedPointProbability interferenceProbability;
		int numBits;

		InterferenceMissProbability(bool _doInterferenceProb, int _numBits);

		void update(MissCounter eventOccurences, MissCounter allOccurences);
		void updateInterference(MissCounter privateEstimate, MissCounter sharedEstimate);

		FixedPointProbability get(MemCmd req);
	};

	class MissCounter{
	public:
		int value;

		MissCounter(int _value)
		: value(_value) {}

		void increment(MemReqPtr& req, int amount = 1);

		void reset();
	};

public:
	std::vector<LRU*> shadowTags;
	int cpuCount;

protected:
    Stats::Vector<> extraMissLatency;
    Stats::Vector<> numExtraResponses;
    Stats::Vector<> numExtraMisses;

    Stats::Vector<> shadowTagWritebacks;

	Stats::Vector<> estimatedShadowAccesses;
	Stats::Vector<> estimatedShadowMisses;
	Stats::Formula estimatedShadowMissRate;
//	Stats::Formula estimatedShadowInterferenceMisses;

	Stats::VectorDistribution<> interferenceMissDistanceDistribution;
	Stats::VectorDistribution<> privateWritebackDistribution;

private:

	int numLeaderSets;
	int size;
	int totalSetNumber;
	int setsInConstituency;

	int randomCounterBits;
	std::vector<FixedWidthCounter> requestCounters;
	std::vector<FixedWidthCounter> responseCounters;

	std::vector<bool> doInterferenceInsertion;

	std::vector<InterferenceMissProbability> interferenceMissProbabilities;

	std::vector<InterferenceMissProbability> privateWritebackProbability;

	std::vector<MissCounter> samplePrivateMisses;
	std::vector<MissCounter> sampleSharedMisses;

	std::vector<MissCounter> sampleSharedResponses;
	std::vector<MissCounter> samplePrivateWritebacks;

	std::vector<int> missesSinceLastInterferenceMiss;
	std::vector<int> sharedResponsesSinceLastPrivWriteback;

	std::vector<int> sequentialReadCount;
	std::vector<int> sequentialWritebackCount;

	std::vector<int> readMissAccumulator;
	std::vector<int> wbMissAccumulator;
	std::vector<int> writebackAccumulator;
	std::vector<int> interferenceMissAccumulator;
	std::vector<int> accessAccumulator;

    bool isLeaderSet(int set);

    void issuePrivateWriteback(int cpuID, Addr addr, BaseCache* cache, int cacheSet = -1);

    void tagAsInterferenceMiss(MemReqPtr& req, int hitLat);

    bool addAsInterference(FixedPointProbability probability, int cpuID, bool useRequestCounter);

    LRUBlk* findShadowTagBlock(MemReqPtr& req, int cpuID, bool isLeaderSet, int hitLat);

    LRUBlk* findShadowTagBlockNoUpdate(MemReqPtr& req, int cpuID);

    void doAccessStatistics(int numberOfSets, MemReqPtr& req, bool isCacheMiss, bool isShadowHit);

    void doShadowReplacement(MemReqPtr& req, BaseCache* cache);

public:

	CacheInterference(std::string _name,
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
			          HierParams* _hp);

	int getNumLeaderSets(){
		return numLeaderSets;
	}

	void access(MemReqPtr& req, bool isCacheMiss, int hitLat, Tick detailedSimStart);

	void handleResponse(MemReqPtr& req, MemReqList writebacks, BaseCache* cache);

	void computeInterferenceProbabilities(int cpuID);

	void initiateInterferenceInsertions(int cpuID){
		doInterferenceInsertion[cpuID] = true;
	}

	bool interferenceInsertionsInitiated(int cpuID){
		return doInterferenceInsertion[cpuID];
	}

	void regStats();

	std::vector<CacheMissMeasurements> getMissMeasurementSample();

    virtual void serialize(std::ostream &os);

    virtual void unserialize(Checkpoint *cp, const std::string &section);
};

#endif /* CACHE_INTERFERENCE_HH_ */
