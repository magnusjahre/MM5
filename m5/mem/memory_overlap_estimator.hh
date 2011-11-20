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

class MemoryOverlapEstimator : public SimObject{

private:
	class EstimationEntry{
	public:
		Addr address;
		Tick issuedAt;
		Tick completedAt;

		EstimationEntry(Addr _a, Tick _issuedAt){
			address = _a;
			issuedAt = _issuedAt;
			completedAt = 0;
		}

		int latency(){
			assert(completedAt != 0);
			return completedAt - issuedAt;
		}
	};

	std::vector<EstimationEntry> pendingRequests;
	std::vector<EstimationEntry> completedRequests;

	Tick stalledAt;
	bool isStalled;

public:
	MemoryOverlapEstimator(std::string name, int id);

	void issuedMemoryRequest(MemReqPtr& req);

	void completedMemoryRequest(MemReqPtr& req, Tick finishedAt);

	void stalledForMemory();

	void executionResumed();

};

#endif /* MEMORY_OVERLAP_ESTIMATOR_HH_ */
