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
#include "mem/accounting/interference_manager.hh"

#include <vector>
#include <iostream>

class BaseCache;
class InterferenceManager;
class CacheAccessMeasurement;
class LRU;
class LRUBlk;

class CacheInterference : public BaseHier{

public:

	enum InterferenceProbabilityPolicy{
		IPP_FULL_RANDOM_FLOAT, // float
		IPP_SEQUENTIAL_INSERT,
		IPP_SEQUENTIAL_INSERT_RESET,
		IPP_NONE,
		IPP_INVALID
	};

	InterferenceProbabilityPolicy loadProbabilityPolicy;
	InterferenceProbabilityPolicy writebackProbabilityPolicy;


	class MissCounter;
	class FixedWidthCounter;

	class FixedPointProbability{
	private:
//		uint32_t value;
		int numBits;
		bool ready;
		uint32_t max;

		double floatValue;

	public:

		FixedPointProbability();

		void setBits(int _numBits);

		void compute(int numerator, int denominator);
		bool doInsertion(FixedWidthCounter* counter);

//		bool isZero(){
//			assert(ready);
//			assert(numBits > 0);
//			return value == 0;
//		}
//
//		bool isMax(){
//			assert(ready);
//			assert(numBits > 0);
//			return value == max;
//		}

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

	double constituencyFactor;

	bool cachePartitioningEnabled;

	InterferenceManager* interferenceManager;

	std::vector<int> requestCounters;
	std::vector<int> responseCounters;

	std::vector<double> interferenceMissProbabilities;
	std::vector<double> privateWritebackProbability;

	std::vector<MissCounter> sampleInterferenceMisses;
	std::vector<MissCounter> sampleSharedMisses;

	std::vector<MissCounter> commitTracePrivateMisses;
	std::vector<MissCounter> commitTracePrivateAccesses;
	std::vector<MissCounter> commitTraceSharedMisses;

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
	std::vector<int> shadowAccessAccumulator;

	std::vector<int> privateHitEstimateAccumulator;
	std::vector<int> privateAccessEstimateAccumulator;
	std::vector<int> privateInterferenceEstimateAccumulator;
	std::vector<int> privateWritebackEstimateAccumulator;

	std::vector<Tick> lastPendingLoadChangeAt;
	std::vector<int> pendingLoads;
	std::vector<double> overlapAccumulator;
	std::vector<Tick> overlapCycles;

	std::vector<int> itcaInterTaskCutoffs;

	bool LLCCheckpointLoadDisabled;

    bool isLeaderSet(int set);

    void issuePrivateWriteback(int cpuID, Addr addr, BaseCache* cache, int cacheSet = -1);

    void tagAsInterferenceMiss(MemReqPtr& req, int hitLat);

    bool addAsInterference(double probability, int cpuID, bool useRequestCounter);

    LRUBlk* findShadowTagBlock(MemReqPtr& req, int cpuID, bool isLeaderSet, int hitLat);

    LRUBlk* findShadowTagBlockNoUpdate(MemReqPtr& req, int cpuID);

    void doAccessStatistics(int numberOfSets, MemReqPtr& req, bool isCacheMiss, bool isShadowHit);

    void doShadowReplacement(MemReqPtr& req, BaseCache* cache);

    void measureOverlap(MemReqPtr &req, bool possibleIncrease);

    int estimateConstituencyAccesses(bool writeback);

    void checkSampledITCAInterTaskMiss(MemReqPtr &req, bool shadowLeaderSet, bool shadowHit, bool isCacheMiss, Addr cacheAlignedCPUAddr);

public:

	CacheInterference(std::string _name,
			          int _cpuCount,
			          int _numLeaderSets,
			          int _size,
			          InterferenceProbabilityPolicy _loadIPP,
			          InterferenceProbabilityPolicy _writebackIPP,
			          InterferenceManager* _intman,
			          int _blockSize,
			          int _assoc,
			          int _hitLat,
			          int _divFac,
			          double _constituencyFactor,
			          HierParams* _hp,
					  bool _disableLLCCheckpointLoad);

	int getNumLeaderSets(){
		return numLeaderSets;
	}

	void access(MemReqPtr& req, bool isCacheMiss, int hitLat, Tick detailedSimStart, BaseCache* cache);

	void computeCacheCapacityInterference(MemReqPtr& req, BaseCache* cache);

	void handleResponse(MemReqPtr& req, MemReqList writebacks, BaseCache* cache);

	void computeInterferenceProbabilities(int cpuID);

	void regStats();

	std::vector<CacheMissMeasurements> getMissMeasurementSample();

    virtual void serialize(std::ostream &os);

    virtual void unserialize(Checkpoint *cp, const std::string &section);

    double getPrivateCommitTraceMissRate(int cpuID){
    	double misses = commitTracePrivateMisses[cpuID].value;
    	double accesses = commitTracePrivateAccesses[cpuID].value;

    	commitTracePrivateMisses[cpuID].reset();
    	commitTracePrivateAccesses[cpuID].reset();

    	if(accesses == 0.0) return 0.0;
    	return misses / accesses;
    }

    int getSharedCommitTraceMisses(int cpuID){
    	fatal("does not work");
    	int misses = commitTraceSharedMisses[cpuID].value;
    	commitTraceSharedMisses[cpuID].reset();
    	return misses;
    }

    double getOverlap(int cpuID);

    void setCachePartitioningEnabled(){
    	cachePartitioningEnabled = true;
    }

    bool getLLCCheckpointLoadDisabled(){
    	return LLCCheckpointLoadDisabled;
    }

    CacheAccessMeasurement getPrivateHitEstimate(int cpuID);
};

#endif /* CACHE_INTERFERENCE_HH_ */
