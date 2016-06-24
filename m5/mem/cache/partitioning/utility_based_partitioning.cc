/*
 * utility_based_partitioning.cc
 *
 *  Created on: May 2, 2010
 *      Author: jahre
 */

#include "utility_based_partitioning.hh"

UtilityBasedPartitioning::UtilityBasedPartitioning(std::string _name,
												   int _associativity,
												   Tick _epochSize,
												   int _np,
												   CacheInterference* ci)
: CachePartitioning(_name, _associativity, _epochSize, _np, ci) {
	first = true;

	currentHitDistributions.resize(_np, vector<int>());

	allocationTrace = RequestTrace(_name, "AllocationTrace");
	vector<string> header = vector<string>();
	for(int i=0;i<_np;i++){
		stringstream curstr;
		curstr << "CPU" << i;
		header.push_back(curstr.str());
	}
	allocationTrace.initalizeTrace(header);
}

void
UtilityBasedPartitioning::handleRepartitioningEvent(){

	if(first){
		first = false;
		schedulePartitionEvent();
		return;
	}

	DPRINTF(CachePartitioning, "Retrieving hit profiles in CPU order\n");
	assert(currentHitDistributions.size() == shadowTags.size());
	for(int i=0;i<shadowTags.size();i++){
		currentHitDistributions[i] = shadowTags[i]->getHitDistribution();
		debugPrintPartition(currentHitDistributions[i], "Hit distribution: ");
	}

	bestHits = 0;
	bestAllocation = vector<int>(partitioningCpuCount, associativity / partitioningCpuCount);
	enumerateAllocations(vector<int>());

	debugPrintPartition(bestAllocation, "Implementing best partition: ");
	for(int i=0;i<cacheBanks.size();i++){
		cacheBanks[i]->setCachePartition(bestAllocation);
		cacheBanks[i]->enablePartitioning();
	}

	vector<RequestTraceEntry> tracedata = vector<RequestTraceEntry>();
	for(int i=0;i<bestAllocation.size();i++) tracedata.push_back(bestAllocation[i]);
	allocationTrace.addTrace(tracedata);

	schedulePartitionEvent();
}

void
UtilityBasedPartitioning::enumerateAllocations(vector<int> currentAllocation){
	if(currentAllocation.size() < partitioningCpuCount){
		for(int i=1;i<associativity;i++){
			vector<int> newAlloc = currentAllocation;
			newAlloc.push_back(i);
			enumerateAllocations(newAlloc);
		}
	}
	else{
		int allocSum = 0;
		for(int i=0;i<currentAllocation.size();i++){
			allocSum += currentAllocation[i];
		}

		if(allocSum != associativity) return;

		evaluateAllocation(currentAllocation);
	}
}

void
UtilityBasedPartitioning::evaluateAllocation(vector<int> allocation){

	assert(allocation.size() == currentHitDistributions.size());

	int hitSum = 0;
	for(int i=0; i<allocation.size();i++){
		assert(allocation[i] < currentHitDistributions[i].size());
		hitSum += currentHitDistributions[i][allocation[i]];
	}

	if(hitSum > bestHits){
		DPRINTF(CachePartitioning, "New best hits value (%d > %d)\n", hitSum, bestHits);

		bestHits = hitSum;
		bestAllocation = allocation;
		debugPrintPartition(bestAllocation, "New best allocation: ");
	}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(UtilityBasedPartitioning)
    Param<int> associativity;
    Param<int> epoch_size;
    Param<int> np;
    SimObjectParam<CacheInterference* > cache_interference;
END_DECLARE_SIM_OBJECT_PARAMS(UtilityBasedPartitioning)


BEGIN_INIT_SIM_OBJECT_PARAMS(UtilityBasedPartitioning)
    INIT_PARAM(associativity, "Cache associativity"),
    INIT_PARAM_DFLT(epoch_size, "Size of an epoch", 5000000),
	INIT_PARAM(np, "Number of cores"),
	INIT_PARAM_DFLT(cache_interference, "Pointer to the cache interference object", NULL)
END_INIT_SIM_OBJECT_PARAMS(UtilityBasedPartitioning)


CREATE_SIM_OBJECT(UtilityBasedPartitioning)
{
    return new UtilityBasedPartitioning(getInstanceName(),
											 associativity,
											 epoch_size,
											 np,
											 cache_interference);
}

REGISTER_SIM_OBJECT("UtilityBasedPartitioning", UtilityBasedPartitioning)

#endif //DOXYGEN_SHOULD_SKIP_THIS
