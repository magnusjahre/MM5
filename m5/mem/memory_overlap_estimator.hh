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
#include "base/statistics.hh"
#include "mem/requesttrace.hh"
#include "mem/interference_manager.hh"

class InterferenceManager;
class BaseCache;

class MemoryOverlapEstimator : public SimObject{

private:
	class EstimationEntry{
	public:
		Addr address;
		Tick issuedAt;
		Tick completedAt;
		MemCmd origCmd;
		bool isSharedReq;

		EstimationEntry(Addr _a, Tick _issuedAt, MemCmd _origCmd){
			address = _a;
			issuedAt = _issuedAt;
			origCmd = _origCmd;
			completedAt = 0;
			isSharedReq = false;
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
		int numSharedAccesses;
		double avgPrivateAccesses;
		double avgSharedLatency;
		int entries;

	public:
		RequestGroupSignature(int sa);

		bool match(int sa);

		void add(double pa, double avgSharedLat);

		void dump();

	};

	std::vector<EstimationEntry> pendingRequests;
	std::vector<EstimationEntry> completedRequests;

	std::vector<RequestGroupSignature> groupSignatures;

	Tick stalledAt;
	Tick resumedAt;
	bool isStalled;

	int cpuID;

	RequestTrace overlapTrace;
	RequestTrace stallTrace;

	Tick stallCycleAccumulator;
	Tick sharedStallCycleAccumulator;
	int totalRequestAccumulator;
	int sharedRequestAccumulator;
	Tick sharedLatencyAccumulator;

	InterferenceManager* interferenceManager;

	Tick lastTraceAt;

	Tick lastActivityCycle;

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
		STALL_DMEM,
		STALL_FUNC_UNIT,
		STALL_OTHER,
		NUM_STALL_CAUSES
	};

private:
	Tick stallCycles[NUM_STALL_CAUSES];
	Tick commitCycles;

private:
	void initOverlapTrace();
	void traceOverlap(int committedInstructions);
	void initStallTrace();
	void traceStalls(int committedInstructions);

	void updateRequestGroups(int sa, int pa, Tick sl);
	void traceRequestGroups(int committedInstructions);

public:
	MemoryOverlapEstimator(std::string name, int id, InterferenceManager* _interferenceManager);

	void regStats();

	void issuedMemoryRequest(MemReqPtr& req);

	void completedMemoryRequest(MemReqPtr& req, Tick finishedAt, bool hiddenLoad);

	void stalledForMemory();

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
