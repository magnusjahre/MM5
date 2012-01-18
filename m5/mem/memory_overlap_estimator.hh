/*
 * memory_overlap_estimator.hh
 *
 *  Created on: Nov 18, 2011
 *      Author: jahre
 */

#ifndef MEMORY_OVERLAP_ESTIMATOR_HH_
#define MEMORY_OVERLAP_ESTIMATOR_HH_

#include "sim/sim_object.hh"
#include "mem/mem_req.hh"
#include "mem/base_hier.hh"
#include "base/statistics.hh"
#include "mem/requesttrace.hh"
#include "mem/interference_manager.hh"

class InterferenceManager;
class BaseCache;

class MemoryOverlapEstimator : public BaseHier{

private:
	class EstimationEntry{
	public:
		Addr address;
		Tick issuedAt;
		Tick completedAt;
		MemCmd origCmd;
		bool isSharedReq;
		bool isSharedCacheMiss;

		EstimationEntry(Addr _a, Tick _issuedAt, MemCmd _origCmd){
			address = _a;
			issuedAt = _issuedAt;
			origCmd = _origCmd;
			completedAt = 0;
			isSharedReq = false;
			isSharedCacheMiss = false;
		}

		int latency(){
			assert(completedAt != 0);
			return completedAt - issuedAt;
		}

		bool isStore(){
			if(origCmd == Write || origCmd == Soft_Prefetch) return true;
			return false;
		}
	};

	class RequestGroupSignature{
	private:
		int sharedCacheHits;
		int sharedCacheMisses;
		double avgPrivateAccesses;
		double avgSharedLatency;
		double avgStallLength;
		double avgIssueToStall;
		int entries;

	public:
		RequestGroupSignature(int _sharedCacheHits, int _sharedCacheMisses);

		bool match(int _sharedCacheHits, int _sharedCacheMisses);

		void add(double pa, double avgSharedLat, double stallLength, double _avgIssueToStall);

		void populate(std::vector<RequestTraceEntry>* data);

	};

	std::vector<EstimationEntry> pendingRequests;
	std::vector<EstimationEntry> completedRequests;

	std::vector<RequestGroupSignature> groupSignatures;

	Tick stalledAt;
	Tick resumedAt;
	bool isStalled;
	Addr stalledOnAddr;

	int cpuID;
	int cpuCount;

	RequestTrace overlapTrace;
	RequestTrace stallTrace;
	RequestTrace requestGroupTrace;

	Tick stallCycleAccumulator;
	Tick sharedStallCycleAccumulator;
	int totalRequestAccumulator;
	int sharedRequestAccumulator;
	Tick sharedLatencyAccumulator;

	InterferenceManager* interferenceManager;

	Tick lastTraceAt;

	Tick lastActivityCycle;

	Tick hiddenSharedLatencyAccumulator;

protected:
	Stats::Scalar<> privateStallCycles;
	Stats::Scalar<> sharedStallCycles;
	Stats::Scalar<> sharedRequestCount;
	Stats::Scalar<> sharedLoadCount;
	Stats::Scalar<> totalLoadLatency;
	Stats::Scalar<> totalStalls;

	Stats::Scalar<> burstAccumulator;
	Stats::Scalar<> numSharedStalls;
	Stats::Formula avgBurstSize;

public:

	enum StallCause{
		STALL_STORE_BUFFER,
		STALL_DMEM_PRIVATE,
		STALL_DMEM_SHARED,
		STALL_FUNC_UNIT,
		STALL_OTHER,
		NUM_STALL_CAUSES
	};

	enum SharedStallIndentifier{
		SHARED_STALL_ROB,
		SHARED_STALL_EXISTS
	};

private:
	Tick stallCycles[NUM_STALL_CAUSES];
	Tick commitCycles;

	SharedStallIndentifier stallIdentifyAlg;

private:
	void initOverlapTrace();
	void traceOverlap(int committedInstructions);
	void initStallTrace();
	void traceStalls(int committedInstructions);

	void updateRequestGroups(int sharedHits, int sharedMisses, int pa, Tick sl, double stallLength, double avgIssueToStall);
	void initRequestGroupTrace();
	void traceRequestGroups(int committedInstructions);

	bool isSharedStall(bool oldestInstIsShared, int sharedReqs);

public:
	MemoryOverlapEstimator(std::string name,
						   int id,
						   InterferenceManager* _interferenceManager,
						   int cpu_count,
						   HierParams* params);

	void regStats();

	void issuedMemoryRequest(MemReqPtr& req);

	void completedMemoryRequest(MemReqPtr& req, Tick finishedAt, bool hiddenLoad);

	void stalledForMemory(Addr stalledOnCoreAddr);

	void executionResumed();

	void sampleCPU(int committedInstructions);

	void addStall(StallCause cause, Tick cycles, bool memStall = false);

	void addCommitCycle();

	void cpuStarted(Tick firstTick);

	void incrementPrivateRequestCount(MemReqPtr& req);

	void addPrivateLatency(MemReqPtr& req, int latency);

	void addL1Access(MemReqPtr& req, int latency, bool hit);

	void registerL1DataCache(int cpuID, BaseCache* cache);

};

#endif /* MEMORY_OVERLAP_ESTIMATOR_HH_ */
