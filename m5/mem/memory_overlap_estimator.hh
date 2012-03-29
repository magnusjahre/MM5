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

class EstimationEntry{
public:
	Addr address;
	Tick issuedAt;
	Tick completedAt;
	MemCmd origCmd;
	bool isSharedReq;
	bool isSharedCacheMiss;
	bool isPrivModeSharedCacheMiss;
	bool isL1Hit;
	bool hidesLoad;
	int id;

	EstimationEntry(int _id, Addr _a, Tick _issuedAt, MemCmd _origCmd){
		id = _id;
		address = _a;
		issuedAt = _issuedAt;
		origCmd = _origCmd;
		completedAt = 0;
		isSharedReq = false;
		isSharedCacheMiss = false;
		isPrivModeSharedCacheMiss = false;
		isL1Hit = false;
		hidesLoad = false;
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

class EstimationNode{
public:
	std::vector<EstimationNode*> children;

	int id;
	Addr addr;
	bool privateMemsysReq;
	int commitCyclesWhileActive;

	EstimationNode(int _id, Addr _addr){
		id = _id;
		addr = _addr;

		privateMemsysReq = false;
		commitCyclesWhileActive = 0;
	}

	void addChild(EstimationNode* child){
		children.push_back(child);
	}


};

class MemoryOverlapEstimator : public BaseHier{

private:

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

	class RequestSampleStats{
	public:
		int pmSharedCacheMisses;
		int pmSharedCacheHits;
		int smSharedCacheMisses;
		int smSharedCacheHits;
		int sharedRequests;

		RequestSampleStats(){ reset(); }

		void addStats(EstimationEntry entry);

		void reset(){
			pmSharedCacheMisses = 0;
			pmSharedCacheHits = 0;
			smSharedCacheMisses = 0;
			smSharedCacheHits = 0;
			sharedRequests = 0;
		}
	};

	std::vector<EstimationEntry> pendingRequests;
	std::vector<EstimationEntry> completedRequests;

	std::vector<EstimationNode*> pendingNodes;

	std::vector<RequestGroupSignature> groupSignatures;

	Tick stalledAt;
	Tick resumedAt;
	bool isStalled;
	Addr stalledOnAddr;

	int cpuID;
	int cpuCount;

	int nextReqID;
	std::vector<EstimationNode*> roots;
	EstimationNode* leastRecentlyCompNode;
	RequestSampleStats rss;

	RequestTrace overlapTrace;
	RequestTrace stallTrace;
	RequestTrace requestGroupTrace;

	RequestTrace sharedRequestTrace;
	bool sharedReqTraceEnabled;
	int sharedTraceReqNum;

	Tick stallCycleAccumulator;
	Tick sharedStallCycleAccumulator;
	int totalRequestAccumulator;
	int sharedRequestAccumulator;
	Tick sharedLatencyAccumulator;

	Tick issueToStallAccumulator;
	int issueToStallAccReqs;

	InterferenceManager* interferenceManager;

	Tick lastTraceAt;

	Tick lastActivityCycle;

	Tick hiddenSharedLatencyAccumulator;

	Tick cacheBlockedCycles;

	Tick computeWhilePendingAccumulator;
	int computeWhilePendingReqs;

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

	Stats::Scalar<> hiddenSharedLoads;
	Stats::Scalar<> hiddenPrivateLoads;
	Stats::Formula hiddenSharedLoadRate;

public:

	enum StallCause{
		STALL_STORE_BUFFER,
		STALL_DMEM_PRIVATE,
		STALL_DMEM_SHARED,
		STALL_FUNC_UNIT,
		STALL_EMPTY_ROB,
		STALL_UNKNOWN,
		STALL_OTHER,
		NUM_STALL_CAUSES
	};

	enum SharedStallIndentifier{
		SHARED_STALL_ROB,
		SHARED_STALL_ROB_WRITE,
		SHARED_STALL_EXISTS
	};

private:
	Tick stallCycles[NUM_STALL_CAUSES];
	Tick commitCycles;

	SharedStallIndentifier stallIdentifyAlg;

private:
	void initOverlapTrace();
	void traceOverlap(int committedInstructions, int cpl);
	void initStallTrace();
	void traceStalls(int committedInstructions);

	void initSharedRequestTrace();
	void traceSharedRequest(EstimationEntry entry, Tick stalledAt, Tick resumedAt);

	void updateRequestGroups(int sharedHits, int sharedMisses, int pa, Tick sl, double stallLength, double avgIssueToStall);
	void initRequestGroupTrace();
	void traceRequestGroups(int committedInstructions);

	bool isSharedStall(bool oldestInstIsShared, int sharedReqs, int numSharedWrites);

	EstimationNode* findNode(int id);
	EstimationNode* traverseTree(EstimationNode* node, int id);

	int gatherParaMeasurements(int committedInsts);
	int findCriticalPathLength(std::vector<EstimationNode*> children, int depth);
	void clearTree(std::vector<EstimationNode*> children);

	EstimationNode* findPendingNode(int id);
	void removePendingNode(int id, bool sharedreq);

public:
	MemoryOverlapEstimator(std::string name,
						   int id,
						   InterferenceManager* _interferenceManager,
						   int cpu_count,
						   HierParams* params,
						   SharedStallIndentifier _ident);

	void regStats();

	void issuedMemoryRequest(MemReqPtr& req);

	void completedMemoryRequest(MemReqPtr& req, Tick finishedAt, bool hiddenLoad);

	void l1HitDetected(MemReqPtr& req, Tick finishedAt);

	void stalledForMemory(Addr stalledOnCoreAddr);

	void executionResumed(bool endedBySquash);

	int sampleCPU(int committedInstructions);

	void addStall(StallCause cause, Tick cycles, bool memStall = false);

	void addCommitCycle();

	void cpuStarted(Tick firstTick);

	void incrementPrivateRequestCount(MemReqPtr& req);

	void addPrivateLatency(MemReqPtr& req, int latency);

	void addL1Access(MemReqPtr& req, int latency, bool hit);

	void registerL1DataCache(int cpuID, BaseCache* cache);

	void addDcacheStallCycle();

	double getAvgCWP();

};

#endif /* MEMORY_OVERLAP_ESTIMATOR_HH_ */
