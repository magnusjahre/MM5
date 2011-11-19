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

public:
	MemoryOverlapEstimator(std::string name, int id);

	void issuedMemoryRequest(MemReqPtr& req);

	void completedMemoryRequest(MemReqPtr& req, Tick finishedAt);

	void stalledForMemory();

	void executionResumed();

};

#endif /* MEMORY_OVERLAP_ESTIMATOR_HH_ */
