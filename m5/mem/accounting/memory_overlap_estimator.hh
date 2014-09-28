/*
 * memory_overlap_estimator.hh
 *
 *  Created on: Nov 18, 2011
 *      Author: jahre
 */

#ifndef MEMORY_OVERLAP_ESTIMATOR_HH_
#define MEMORY_OVERLAP_ESTIMATOR_HH_

#define TICK_MAX ULL(0x3FFFFFFFFFFFFF)
#define MOE_CACHE_BLK_SIZE 64

#include "sim/sim_object.hh"
#include "mem/mem_req.hh"
#include "mem/base_hier.hh"
#include "base/statistics.hh"
#include "mem/requesttrace.hh"
#include "mem/accounting/interference_manager.hh"
#include "mem/accounting/memory_overlap_table.hh"
#include "mem/accounting/critical_path_table.hh"
#include "mem/accounting/itca.hh"

class InterferenceManager;
class BaseCache;
class MemoryOverlapTable;
class CriticalPathTable;

class CriticalPathTableMeasurements{
public:
	Tick criticalPathLatency;
	Tick criticalPathInterference;
	Tick criticalPathCommitWhilePending;
	int criticalPathRequests;

	int criticalPathLength;

	CriticalPathTableMeasurements(){
		reset();
	}

	void reset(){
		criticalPathLatency = 0;
		criticalPathInterference = 0;
		criticalPathCommitWhilePending = 0;
		criticalPathRequests = 0;

		criticalPathLength = 0;
	}

	double averageCPLatency(){
		if(criticalPathRequests == 0) return 0.0;
		return (double) ((double) criticalPathLatency / (double) criticalPathRequests);
	}

	double averageCPInterference(){
		if(criticalPathRequests == 0) return 0.0;
		return (double) ((double) criticalPathInterference / (double) criticalPathRequests);
	}

	double averageCPCWP(){
		if(criticalPathRequests == 0) return 0.0;
		return (double) ((double) criticalPathCommitWhilePending / (double) criticalPathRequests);
	}

	double privateLatencyEstimate(){
		return averageCPLatency() - averageCPInterference();
	}
};


class EstimationEntry{
public:
	Addr address;
	Tick issuedAt;
	Tick completedAt;
	Tick interference;
	MemCmd origCmd;
	bool isSharedReq;
	bool isSharedCacheMiss;
	bool isPrivModeSharedCacheMiss;
	bool isL1Hit;
	bool hidesLoad;
	int id;
	int commitCyclesWhileActive;

	EstimationEntry(int _id, Addr _a, Tick _issuedAt, MemCmd _origCmd){
		id = _id;
		address = _a;
		issuedAt = _issuedAt;
		origCmd = _origCmd;
		completedAt = 0;
		interference = 0;
		isSharedReq = false;
		isSharedCacheMiss = false;
		isPrivModeSharedCacheMiss = false;
		isL1Hit = false;
		hidesLoad = false;
		commitCyclesWhileActive = 0;
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

class MemoryGraphNode{
public:

	std::vector<MemoryGraphNode*>* children;
	std::vector<MemoryGraphNode*>* parents;
	int validParents;

	int id;
	Tick startedAt;
	Tick finishedAt;

	bool visited;
	int depth;

	MemoryGraphNode(int _id, Tick _start){
		id = _id;
		startedAt = _start;
		finishedAt = 0;

		visited = false;

		children = new std::vector<MemoryGraphNode* >();
		parents = new std::vector<MemoryGraphNode* >();
		depth = -1;
		validParents = 0;
	}

	virtual ~MemoryGraphNode(){
		delete children;
		delete parents;
	}

	void addChild(MemoryGraphNode* child);

	void addParent(MemoryGraphNode* parent);

	void removeParent(MemoryGraphNode* parent);

	virtual bool addToCPL() = 0;

	virtual Addr getAddr(){
		return 0;
	}

	virtual const char* name() = 0;

	int lat(){
		if(finishedAt > 0) return finishedAt - startedAt;
		return -1;
	}

};

class ComputeNode : public MemoryGraphNode{
public:

	ComputeNode(int _id, Tick _start) : MemoryGraphNode(_id, _start){ }

	~ComputeNode(){ }

	bool addToCPL(){
		return false;
	}

	const char* name(){
		return "compute";
	}

};

class RequestNode : public MemoryGraphNode{
public:

	Addr addr;
	bool privateMemsysReq;
	int commitCyclesWhileActive;
	bool isLoad;
	bool isSharedCacheMiss;
	bool causedStall;

	RequestNode(int _id, Addr _addr, Tick _start): MemoryGraphNode(_id, _start)
	{
		addr = _addr;

		privateMemsysReq = false;
		commitCyclesWhileActive = 0;
		isLoad = false;
		isSharedCacheMiss = false;
		causedStall = false;
	}

	~RequestNode(){ }

	bool addToCPL(){
		return isLoad;
	}

	Addr getAddr(){
		return addr;
	}

	const char* name(){
		return "request";
	}

	bool during(Tick _tick){
		if(_tick >= startedAt && _tick < finishedAt) return true;
		return false;
	}

	Tick distanceToParent(ComputeNode* compute){
		if(compute->startedAt >= startedAt) return TICK_MAX;
		return startedAt - compute->startedAt;
	}

	Tick distanceToChild(ComputeNode* compute){
		if(finishedAt >= compute->finishedAt) return TICK_MAX;
		return compute->finishedAt - finishedAt;
	}
};

class BurstStats{
public:
	Tick startedAt;
	Tick finishedAt;
	int numRequests;

	BurstStats();

	void addRequest(MemoryGraphNode* node);

	bool overlaps(MemoryGraphNode* node);
};

class OverlapStatistics{
public:

	int graphCPL;
	int tableCPL;
	double avgBurstSize;
	double avgBurstLength;
	double avgInterBurstOverlap;
	double avgTotalComWhilePend;
	double avgComWhileBurst;
	CriticalPathTableMeasurements cptMeasurements;
	Tick itcaAccountedCycles;

	OverlapStatistics(){
		graphCPL = 0;
		tableCPL = 0;
		avgBurstSize = 0.0;
		avgBurstLength = 0.0;
		avgInterBurstOverlap = 0.0;
		avgTotalComWhilePend = 0.0;
		avgComWhileBurst = 0.0;
		itcaAccountedCycles = 0;
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

	MemoryOverlapTable* overlapTable;
	CriticalPathTable* criticalPathTable;
	ITCA* itca;

	std::vector<EstimationEntry*> pendingRequests;
	std::vector<EstimationEntry*> completedRequests;

	//std::vector<RequestNode*> pendingNodes;

	std::vector<RequestGroupSignature> groupSignatures;

	std::vector<RequestNode* > completedRequestNodes;
	std::vector<RequestNode* > burstRequests;
	std::vector<ComputeNode* > completedComputeNodes;
	ComputeNode* pendingComputeNode;
	ComputeNode* lastComputeNode;
	int nextComputeNodeID;

	Tick stalledAt;
	Tick resumedAt;
	bool isStalled;
	Addr stalledOnAddr;

	int cpuID;
	int cpuCount;

	int nextReqID;
	int reqNodeID;
	MemoryGraphNode* root;
	RequestSampleStats rss;

	std::vector<BurstStats> burstInfo;

	RequestTrace overlapTrace;
	RequestTrace stallTrace;
	RequestTrace requestGroupTrace;

	RequestTrace sharedRequestTrace;
	bool sharedReqTraceEnabled;
	int sharedTraceReqNum;

	int sampleID;
	int traceSampleID;

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
	Tick computeWhilePendingTotalAccumulator;
	int computeWhilePendingReqs;

	bool isStalledOnWrite;
	int numWriteStalls;

	bool doGraphAnalysis;

	Tick currentStallFullROB;
	Tick stallWithFullROBAccumulator;
	Tick privateStallWithFullROBAccumulator;
	Tick boisAloneStallEstimate;
	Tick boisAloneStallEstimateTrace;
	Tick boisMemsysInterferenceTrace;
	Tick boisLostStallCycles;

protected:
	Stats::Scalar<> privateStallCycles;
	Stats::Scalar<> sharedStallCycles;
	Stats::Scalar<> sharedRequestCount;
	Stats::Scalar<> sharedLoadCount;
	Stats::Scalar<> totalLoadLatency;
	Stats::Scalar<> totalStalls;

	Stats::Scalar<> numSharedStallsForROB;
	Stats::Scalar<> numSharedStallsWithFullROB;
	Stats::Formula sharedStallFullROBRatio;
	Stats::Formula sharedStallNotFullROBRatio;

	Stats::Scalar<> burstAccumulator;
	Stats::Scalar<> numSharedStalls;
	Stats::Formula avgBurstSize;

	Stats::Scalar<> hiddenSharedLoads;
	Stats::Scalar<> hiddenPrivateLoads;
	Stats::Formula hiddenSharedLoadRate;

	Stats::Distribution<> cpl_table_error;

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
	void traceSharedRequest(EstimationEntry* entry, Tick stalledAt, Tick resumedAt);

	void updateRequestGroups(int sharedHits, int sharedMisses, int pa, Tick sl, double stallLength, double avgIssueToStall);
	void initRequestGroupTrace();
	void traceRequestGroups(int committedInstructions);

	bool isSharedStall(bool oldestInstIsShared, int sharedReqs, int numSharedWrites);

	//MemoryGraphNode* findNode(int id);
	//MemoryGraphNode* traverseTree(MemoryGraphNode* node, int id);

	OverlapStatistics gatherParaMeasurements(int committedInsts);
	std::list<MemoryGraphNode* > findTopologicalOrder(MemoryGraphNode* root);
	int findCriticalPathLength(MemoryGraphNode* node, int depth);
	void clearData();

//	RequestNode* findPendingNode(int id);
//	void removePendingNode(int id, bool sharedreq);

	bool checkReachability();
	void unsetVisited();

	double findComputeBurstOverlap();
	void populateBurstInfo();

	void processCompletedRequests(bool stalledOnShared, std::vector<RequestNode* > reqs);
	RequestNode* buildRequestNode(EstimationEntry* entry, bool causedStall);
	void setParent(RequestNode* node);
	void setChild(RequestNode* node);

	bool pointerExists(MemoryGraphNode* ptr);

	void addBoisEstimateCycles(Tick aloneStallTicks);

public:
	MemoryOverlapEstimator(std::string name,
						   int id,
						   InterferenceManager* _interferenceManager,
						   int cpu_count,
						   HierParams* params,
						   SharedStallIndentifier _ident,
						   bool _sharedReqTraceEnabled,
						   bool _graphAnalysisEnabled,
						   MemoryOverlapTable* _overlapTable,
						   int _traceSampleID,
						   int _cplTableBufferSize,
						   ITCA* _itca);

	~MemoryOverlapEstimator();

	void regStats();

	void issuedMemoryRequest(MemReqPtr& req);

	void completedMemoryRequest(MemReqPtr& req, Tick finishedAt, bool hiddenLoad);

	void l1HitDetected(MemReqPtr& req, Tick finishedAt);

	void stalledForMemory(Addr stalledOnCoreAddr);

	void executionResumed(bool endedBySquash);

	OverlapStatistics sampleCPU(int committedInstructions);

	void addStall(StallCause cause, Tick cycles, bool memStall = false);

	void addCommitCycle();

	void cpuStarted(Tick firstTick);

	void incrementPrivateRequestCount(MemReqPtr& req);

	void addPrivateLatency(MemReqPtr& req, int latency);

	void addL1Access(MemReqPtr& req, int latency, bool hit);

	void registerL1DataCache(int cpuID, BaseCache* cache);

	void addDcacheStallCycle();

	void addROBFullCycle();

	double getAvgCWP();

	int getNumWriteStalls();

	Tick getBoisAloneStallEstimate();

	int getTraceSampleID(){
		return traceSampleID;
	}

	void itcaIntertaskMiss(Addr addr, bool isInstructionMiss){
		itca->intertaskMiss(addr, isInstructionMiss);
	}

	void itcaCPUStalled(ITCA::ITCACPUStalls type){
		itca->itcaCPUStalled(type);
	}

	void itcaCPUResumed(ITCA::ITCACPUStalls type){
		itca->itcaCPUResumed(type);
	}

	void itcaInstructionMiss(Addr addr);
	void itcaInstructionMissResolved(Addr addr, Tick willFinishAt);
	void itcaSquash(Addr addr);

	void itcaSetROBEmpty() { itca->setROBEmpty(); }
	void itcaClearROBEmpty() { itca->clearROBEmpty(); }
};

#endif /* MEMORY_OVERLAP_ESTIMATOR_HH_ */
