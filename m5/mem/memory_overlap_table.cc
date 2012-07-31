
#include "mem/memory_overlap_table.hh"
#include "base/trace.hh"

using namespace std;

MemoryOverlapTable::MemoryOverlapTable(int totalL1MSHRs, int comTableSize){
	overlapTable.resize(totalL1MSHRs, MemoryOverlapTableEntry());
	reset();
}

void
MemoryOverlapTable::reset(){
	hiddenLatencyRequest = 0;
	hiddenLatencyCompute = 0;
	totalLatency = 0;
	totalStall = 0;

	numPendingReqs = 0;
	if(curTick == 0) pendingComStartAt = 1;
	else pendingComStartAt = curTick;

	headindex = 0;
	tailindex = 0;

	for(int i=0;i<overlapTable.size();i++) overlapTable[i].reset();
}

void
MemoryOverlapTable::requestIssued(MemReqPtr& req){
	Addr addr = req->paddr & ~(MOE_CACHE_BLK_SIZE-1);
	numPendingReqs++;

	DPRINTF(OverlapEstimatorTable, "Adding request %d to %d (tail pointer), head pointer is %d\n",
			addr,
			tailindex,
			headindex);

	if(overlapTable[tailindex].valid) fatal("no invalid table slot, area budget exceeded");

	overlapTable[tailindex].address = addr;
	overlapTable[tailindex].issuedAt = curTick;
	overlapTable[tailindex].windowStart = curTick;
	overlapTable[tailindex].valid = true;

	tailindex = (tailindex + 1) % overlapTable.size();
}

void
MemoryOverlapTable::requestCompleted(MemReqPtr& req){
	numPendingReqs--;

	int curIndex = headindex;
	int numchecks = 0;
	while(overlapTable[curIndex].address != req->paddr ){
		curIndex = (curIndex +1) % overlapTable.size();
		numchecks++;
		assert(numchecks <= overlapTable.size());
	}

	DPRINTF(OverlapEstimatorTable, "Request %d completed, found in table at place %d\n",
			req->paddr,
			curIndex);

	if(req->beenInSharedMemSys){
		if(numPendingReqs > 0){

			if(curTick == 5718) dumpBuffer();

			// STEP 1: move window to start of this request if neccessary
			int oldestIndex = headindex;
			while(oldestIndex != curIndex){
				fatal("this is not oldest");
			}

			// STEP 2: process finished request
			for(int i=0;i<overlapTable.size();i++){
				if(overlapTable[i].valid && i != curIndex){
					assert(overlapTable[curIndex].windowStart <= overlapTable[i].windowStart);
					if(overlapTable[i].tmpComStart != 0) fatal("tmpComStart != 0 not handled");

					int roinc = curTick - overlapTable[i].windowStart - overlapTable[i].tmpComOverlap;
					overlapTable[i].requestOverlap += roinc;
					overlapTable[i].commitOverlap += overlapTable[i].tmpComOverlap;
					overlapTable[i].windowStart = curTick;

					DPRINTF(OverlapEstimatorTable, "Request %d overlaps with %d, moving %d hidden commit cycles (sum %d) and %d hidden request cycles (sum %d), new window start %d\n",
									i,
									curIndex,
									overlapTable[i].tmpComOverlap,
									overlapTable[i].commitOverlap,
									roinc,
									overlapTable[i].requestOverlap,
									overlapTable[i].windowStart);

					overlapTable[i].tmpComOverlap = 0;
				}
			}
		}

		// STEP 4: Add finished request data to aggregate data
		int latency = curTick - overlapTable[curIndex].issuedAt;
		overlapTable[curIndex].stall += curTick - overlapTable[curIndex].windowStart;

		DPRINTF(OverlapEstimatorTable, "Shared request %d, total latency %d, commit overlap %d, request overlap %d, caused stall %d\n",
				curIndex,
				latency,
				overlapTable[curIndex].commitOverlap,
				overlapTable[curIndex].requestOverlap,
				overlapTable[curIndex].stall);

		assert(overlapTable[curIndex].stall == latency - overlapTable[curIndex].requestOverlap - overlapTable[curIndex].commitOverlap);

		hiddenLatencyRequest += overlapTable[curIndex].requestOverlap;
		hiddenLatencyCompute += overlapTable[curIndex].commitOverlap;
		totalLatency += latency;
		totalStall += overlapTable[curIndex].stall;

		assert(totalStall == totalLatency - hiddenLatencyRequest - hiddenLatencyCompute);
	}

	// Step 5: Invalidate request and update head pointer
	invalidateEntry(curIndex, req->paddr);
}

void
MemoryOverlapTable::invalidateEntry(int curIndex, Addr addr){
	DPRINTF(OverlapEstimatorTable, "Request %d is done, clearing position %d\n",
			addr,
			curIndex);
	overlapTable[curIndex].reset();

	int itcnt = 0;
	while(!overlapTable[headindex].valid){
		if(headindex == tailindex){
			DPRINTF(OverlapEstimatorTable, "Head reached tail at %d\n", headindex);
			assert(numPendingReqs == 0);
			return;
		}

		int tmpid = headindex;
		headindex = (headindex + 1) % overlapTable.size();
		DPRINTF(OverlapEstimatorTable, "Request %d is invalidated, updating head pointer to %d, tail pointer is %d\n",
				tmpid,
				headindex,
				tailindex);

		itcnt++;
		assert(itcnt <= overlapTable.size());
	}
}

void
MemoryOverlapTable::executionStalled(){
	assert(pendingComStartAt > 0);
	if(numPendingReqs > 0){
		for(int i=0;i<overlapTable.size();i++){
			if(overlapTable[i].valid){

				int overlapDuration = 0;
				if(overlapTable[i].windowStart > pendingComStartAt){
					overlapDuration = curTick - overlapTable[i].windowStart;
				}
				else{
					overlapDuration = curTick - pendingComStartAt;
				}

				overlapTable[i].tmpComOverlap += overlapDuration;
				overlapTable[i].tmpComStart = 0;

				DPRINTF(OverlapEstimatorTable, "Stall: Requests in slot %d overlaps (start %d, window start %d), incrementing temporary commit overlap by %d to %d\n",
						i,
						overlapTable[i].issuedAt,
						overlapTable[i].windowStart,
						overlapDuration,
						overlapTable[i].tmpComOverlap);
			}
		}
	}
	else{
		DPRINTF(OverlapEstimatorTable, "Execution stalled and no pending reqs, no action\n");
	}

	pendingComStartAt = 0;
}

void
MemoryOverlapTable::executionResumed(){
	assert(pendingComStartAt == 0);
	pendingComStartAt = curTick;
	DPRINTF(OverlapEstimatorTable, "Execution resumed\n");

	if(numPendingReqs > 0){
		for(int i=0;i<overlapTable.size();i++){
			if(overlapTable[i].valid){
				assert(overlapTable[i].tmpComStart == 0);
				overlapTable[i].tmpComStart = curTick;
				DPRINTF(OverlapEstimatorTable, "Request entry %d is pending, recording commit start\n", i);
			}
		}
	}
}

void
MemoryOverlapTable::dumpBuffer(){
	cout << "Overlap table contents @ " << curTick << ", head is " << headindex << ", tail is " << tailindex << "\n";
	for(int i=0;i<overlapTable.size();i++){
		if(overlapTable[i].valid){
			cout << i << ", addr " << overlapTable[i].address
					<< ", issued " << overlapTable[i].issuedAt
					<< ", window start " << overlapTable[i].windowStart
					<< ", tmp com " << overlapTable[i].tmpComOverlap
					<< ", com " << overlapTable[i].commitOverlap
					<< ", req " << overlapTable[i].requestOverlap
					<< ", stall " << overlapTable[i].stall
					<< "\n";
		}
	}
}
