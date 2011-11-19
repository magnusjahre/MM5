/*
 * memory_overlap_estimator.cc
 *
 *  Created on: Nov 18, 2011
 *      Author: jahre
 */

#include "memory_overlap_estimator.hh"
#include "sim/builder.hh"

using namespace std;

MemoryOverlapEstimator::MemoryOverlapEstimator(string name, int id)
: SimObject(name){

}

void
MemoryOverlapEstimator::issuedMemoryRequest(MemReqPtr& req){
	fatal("request issued not implemented");
}

void
MemoryOverlapEstimator::completedMemoryRequest(MemReqPtr& req, Tick finishedAt){
	fatal("request completed not implemented");
}

void
MemoryOverlapEstimator::stalledForMemory(){
	fatal("stalled for memory not implemented");
}

void
MemoryOverlapEstimator::executionResumed(){
	fatal("execution resumed not implemented");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
    Param<int> id;
END_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

BEGIN_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)
	INIT_PARAM(id, "ID of this estimator")
END_INIT_SIM_OBJECT_PARAMS(MemoryOverlapEstimator)

CREATE_SIM_OBJECT(MemoryOverlapEstimator)
{
    return new MemoryOverlapEstimator(getInstanceName(), id);
}

REGISTER_SIM_OBJECT("MemoryOverlapEstimator", MemoryOverlapEstimator)

#endif
