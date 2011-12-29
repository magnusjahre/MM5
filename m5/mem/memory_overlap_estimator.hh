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

class MemoryOverlapEstimator : public SimObject{

private:
	class EstimationEntry{
	public:
		Addr address;
		Tick issuedAt;
		Tick completedAt;
		MemCmd origCmd;

		EstimationEntry(Addr _a, Tick _issuedAt, MemCmd _origCmd){
			address = _a;
			issuedAt = _issuedAt;
			origCmd = _origCmd;
			completedAt = 0;
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

	std::vector<EstimationEntry> pendingRequests;
	std::vector<EstimationEntry> completedRequests;

	Tick stalledAt;
	Tick resumedAt;
	bool isStalled;

	RequestTrace overlapTrace;

	Tick stallCycleAccumulator;
	Tick sharedStallCycleAccumulator;
	int totalRequestAccumulator;
	int sharedRequestAccumulator;
	Tick sharedLatencyAccumulator;

protected:
	Stats::Scalar<> privateStallCycles;
	Stats::Scalar<> sharedStallCycles;
	Stats::Scalar<> sharedRequestCount;
	Stats::Scalar<> sharedLoadCount;
	Stats::Scalar<> totalLoadLatency;

	Stats::Scalar<> burstAccumulator;
	Stats::Scalar<> numSharedStalls;
	Stats::Formula avgBurstSize;

public:
	MemoryOverlapEstimator(std::string name, int id);

	void regStats();

	void issuedMemoryRequest(MemReqPtr& req);

	void completedMemoryRequest(MemReqPtr& req, Tick finishedAt, bool hiddenLoad);

	void stalledForMemory();

	void executionResumed();

	void traceOverlap(int committedInstructions);

};

#endif /* MEMORY_OVERLAP_ESTIMATOR_HH_ */
