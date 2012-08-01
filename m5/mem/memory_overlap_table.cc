
#include "memory_overlap_table.hh"
#include "base/trace.hh"
#include "sim/builder.hh"

using namespace std;

MemoryOverlapTable::MemoryOverlapTable(string _name, int totalL1MSHRs, int unknownTableSize)
: SimObject(_name){
	overlapTable.resize(totalL1MSHRs, MemoryOverlapTableEntry());
	unknownLatencyBuffer.resize(unknownTableSize, UnknownLatencyBufferEntry(totalL1MSHRs));
	initTableTrace();
	reset();
}

void
MemoryOverlapTable::initTableTrace(){
	overlapTableTrace = RequestTrace(name(), "Data", 1);

	vector<string> headers;
	headers.push_back("Committed instructions");
	headers.push_back("Total latency");
	headers.push_back("Total stall");
	headers.push_back("Request Overlap");
	headers.push_back("Commit Overlap");

	overlapTableTrace.initalizeTrace(headers);
}

void
MemoryOverlapTable::traceTable(int insts){
	vector<RequestTraceEntry> data;

	data.push_back(insts);
	data.push_back(totalLatency);
	data.push_back(totalStall);
	data.push_back(hiddenLatencyRequest);
	data.push_back(hiddenLatencyCompute);

	overlapTableTrace.addTrace(data);

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

	unknownLatencyHead = 0;
	unknownLatencyTail = 0;

	for(int i=0;i<unknownLatencyBuffer.size();i++) unknownLatencyBuffer[i].reset();
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

int
MemoryOverlapTable::findTempWindowEndIndex(int curIndex){
	assert(overlapTable[headindex].windowStart <= overlapTable[curIndex].windowStart);
	if(overlapTable[headindex].windowStart == overlapTable[curIndex].windowStart){
		DPRINTF(OverlapEstimatorTable, "Finished request is in same window as head, returning -1\n");
		return -1;
	}

	int stepsIndex = headindex;
	while(stepsIndex != curIndex){
		if(overlapTable[stepsIndex].valid){
			DPRINTF(OverlapEstimatorTable, "Step index is %d with window start %d, head window start is %d\n",
					stepsIndex,
					overlapTable[stepsIndex].windowStart,
					overlapTable[headindex].windowStart);

			if(overlapTable[stepsIndex].windowStart > overlapTable[headindex].windowStart){
				DPRINTF(OverlapEstimatorTable, "Request %d start at %d is a temporary window border\n",
						stepsIndex,
						overlapTable[stepsIndex].windowStart);

				return stepsIndex;
			}
		}
		stepsIndex = (stepsIndex + 1) % overlapTable.size();
	}

	DPRINTF(OverlapEstimatorTable, "Current index is step end, returning %d\n", curIndex);
	return curIndex;
}

void
MemoryOverlapTable::handleUnknownLatencies(int curIndex){
	int tmpWindowEnd = findTempWindowEndIndex(curIndex);

	if(tmpWindowEnd == -1) return;

	int firstULBuffer = -1;
	while(tmpWindowEnd != -1){
		int ulbufferIndex = getFreeULBufferEntry();
		if(firstULBuffer == -1) firstULBuffer = ulbufferIndex;

		unknownLatencyBuffer[ulbufferIndex].latency = overlapTable[tmpWindowEnd].windowStart - overlapTable[headindex].windowStart;
		DPRINTF(OverlapEstimatorTable, "Initializing UL entry %d with latency %d\n",
										ulbufferIndex,
										unknownLatencyBuffer[ulbufferIndex].latency);

		int stepIndex = headindex;
		while(stepIndex != tmpWindowEnd){
			if(overlapTable[stepIndex].valid){
				unknownLatencyBuffer[ulbufferIndex].bufferInvolved[stepIndex] = true;
				unknownLatencyBuffer[ulbufferIndex].count++;

				if(unknownLatencyBuffer[ulbufferIndex].commit == 0) unknownLatencyBuffer[ulbufferIndex].commit = overlapTable[stepIndex].tmpComOverlap;
				assert(unknownLatencyBuffer[ulbufferIndex].commit == overlapTable[stepIndex].tmpComOverlap);
				overlapTable[stepIndex].tmpComOverlap = 0;

				overlapTable[stepIndex].windowStart = overlapTable[tmpWindowEnd].windowStart;

				DPRINTF(OverlapEstimatorTable, "Request %d is involved with unknown latency buffer %d, updating window start to %d, count to %d and commit to %d\n",
						stepIndex,
						ulbufferIndex,
						overlapTable[tmpWindowEnd].windowStart,
						unknownLatencyBuffer[ulbufferIndex].count,
						unknownLatencyBuffer[ulbufferIndex].commit);
			}
			stepIndex = (stepIndex + 1) % overlapTable.size();
		}

		tmpWindowEnd = findTempWindowEndIndex(curIndex);
	}

	updateUnknownCommitLatencies(curIndex, firstULBuffer);
}

int
MemoryOverlapTable::decrULIndex(int index){
	int newindex = index -1;
	if(newindex >= 0) return newindex;
	return unknownLatencyBuffer.size()-1;
}

void
MemoryOverlapTable::updateUnknownCommitLatencies(int curIndex, int firstULBuffer){
	int commitAccumulator = overlapTable[curIndex].tmpComOverlap;
	int bufferpos = decrULIndex(unknownLatencyTail);
	while(bufferpos != firstULBuffer){
		assert(unknownLatencyBuffer[bufferpos].valid);

		int oldComVal = unknownLatencyBuffer[bufferpos].commit;
		unknownLatencyBuffer[bufferpos].commit -= commitAccumulator;
		int oldLatVal= unknownLatencyBuffer[bufferpos].latency;
		unknownLatencyBuffer[bufferpos].latency -= unknownLatencyBuffer[bufferpos].commit;

		DPRINTF(OverlapEstimatorTable, "Commit for UL entry %d set to %d (was %d), latency to %d (was %d), accumulator was %d, accumulator is %d\n",
				bufferpos,
				unknownLatencyBuffer[bufferpos].commit,
				oldComVal,
				unknownLatencyBuffer[bufferpos].latency,
				oldLatVal,
				commitAccumulator,
				oldComVal);

		commitAccumulator = oldComVal;
		bufferpos = decrULIndex(bufferpos);
	}


	int oldCom = unknownLatencyBuffer[firstULBuffer].commit;
	int oldLat = unknownLatencyBuffer[firstULBuffer].latency;
	unknownLatencyBuffer[firstULBuffer].commit -= commitAccumulator;
	unknownLatencyBuffer[firstULBuffer].latency -= unknownLatencyBuffer[firstULBuffer].commit;

	DPRINTF(OverlapEstimatorTable, "Adjusted commit for first element to %d (was %d), latency adjusted to %d (was %d)\n",
			unknownLatencyBuffer[firstULBuffer].commit,
			oldCom,
			unknownLatencyBuffer[firstULBuffer].latency,
			oldLat);
}

void
MemoryOverlapTable::processULBuffer(int curIndex, bool addReq){

	bool removedEntry = false;
	int uncindex = unknownLatencyHead;
	while(uncindex != unknownLatencyTail){
		if(unknownLatencyBuffer[uncindex].bufferInvolved[curIndex]){
			assert(unknownLatencyBuffer[uncindex].valid);

			if(addReq){
				overlapTable[curIndex].stall += unknownLatencyBuffer[uncindex].latency;
				overlapTable[curIndex].commitOverlap += unknownLatencyBuffer[uncindex].commit;

				DPRINTF(OverlapEstimatorTable, "Current request %d is shared and has unknown latency, adding %d stall cycles (sum %d) and %d commit cycles (sum %d), request overlap is %d\n",
						curIndex,
						unknownLatencyBuffer[uncindex].latency,
						overlapTable[curIndex].stall,
						unknownLatencyBuffer[uncindex].commit,
						overlapTable[curIndex].commitOverlap,
						overlapTable[curIndex].requestOverlap);

				for(int i=0;i<overlapTable.size();i++){
					if(unknownLatencyBuffer[uncindex].bufferInvolved[i] && i != curIndex){
						assert(overlapTable[i].valid);
						overlapTable[i].requestOverlap += unknownLatencyBuffer[uncindex].latency;
						overlapTable[i].commitOverlap += unknownLatencyBuffer[uncindex].commit;

						DPRINTF(OverlapEstimatorTable, "Request %d is also in unknown window, adding %d overlapped cycles (sum %d) and %d commit cycles (sum %d), stall is %d\n",
								i,
								unknownLatencyBuffer[uncindex].latency,
								overlapTable[i].requestOverlap,
								unknownLatencyBuffer[uncindex].commit,
								overlapTable[i].commitOverlap,
								overlapTable[i].stall);
					}
				}

				DPRINTF(OverlapEstimatorTable, "Invalidating UL entry %d, UL head %d, UL tail %d\n",
						uncindex,
						unknownLatencyHead,
						unknownLatencyTail);

				unknownLatencyBuffer[uncindex].reset();
				removedEntry = true;
			}
			else{
				unknownLatencyBuffer[uncindex].bufferInvolved[curIndex] = false;
				unknownLatencyBuffer[uncindex].count--;

				DPRINTF(OverlapEstimatorTable, "Request %d is private or shared store, setting flag to false and reducing count to %d\n",
						curIndex,
						unknownLatencyBuffer[uncindex].count);

				assert(unknownLatencyBuffer[uncindex].count >= 0);
				if(unknownLatencyBuffer[uncindex].count == 0){
					DPRINTF(OverlapEstimatorTable, "Invalidating buffer %d due to no pending requests\n", uncindex);
					unknownLatencyBuffer[uncindex].reset();
					removedEntry = true;
				}
			}
		}

		uncindex = (uncindex + 1) % unknownLatencyBuffer.size();
	}

	if(removedEntry){
		while(!unknownLatencyBuffer[unknownLatencyHead].valid){
			if(unknownLatencyHead == unknownLatencyTail){
				assert(!unknownLatencyBuffer[unknownLatencyHead].valid);
				DPRINTF(OverlapEstimatorTable, "UL table is empty, UL head %d, UL tail %d\n",
						unknownLatencyHead,
						unknownLatencyTail);

				return;
			}
			unknownLatencyHead = (unknownLatencyHead + 1) % unknownLatencyBuffer.size();
		}
		DPRINTF(OverlapEstimatorTable, "UL table pointer update finished, UL head %d, UL tail %d\n",
								unknownLatencyHead,
								unknownLatencyTail);
	}
}

void
MemoryOverlapTable::handleCommitInProgress(int index){
	if(overlapTable[index].tmpComStart != 0){
		int comInc = curTick - overlapTable[index].tmpComStart;
		overlapTable[index].tmpComOverlap += comInc;
		overlapTable[index].tmpComStart = curTick;

		DPRINTF(OverlapEstimatorTable, "Request %d overlaps with non-completed commit, adding %d temp commit cycles (sum %d) and setting commit start to %d\n",
				index,
				comInc,
				overlapTable[index].tmpComOverlap,
				overlapTable[index].tmpComStart = curTick);
	}
}

int
MemoryOverlapTable::findRequest(Addr addr){
	int curIndex = headindex;
	while(curIndex != tailindex){
		if(overlapTable[curIndex].address == addr){
			return curIndex;
		}
		curIndex = (curIndex +1) % overlapTable.size();
	}

	return -1;
}

bool
MemoryOverlapTable::isSharedRead(MemReqPtr& req, bool hiddenLoad){
	if(req->beenInSharedMemSys){
		if(hiddenLoad){
			DPRINTF(OverlapEstimatorTable, "Request %d hides a load, add it\n", req->paddr);
			return true;
		}
		if(req->isStore){
			DPRINTF(OverlapEstimatorTable, "Request %d is a store, skip it\n", req->paddr);
			return false;
		}
		DPRINTF(OverlapEstimatorTable, "Request %d is a regular load, add it\n", req->paddr);
		return true;
	}
	DPRINTF(OverlapEstimatorTable, "Request %d is private, skip it\n", req->paddr);
	return false;
}

void
MemoryOverlapTable::requestCompleted(MemReqPtr& req, bool hiddenLoad){

	int curIndex = findRequest(req->paddr);
	if(curIndex == -1){
		DPRINTF(OverlapEstimatorTable, "Request %d completed, but was not found in the table (reset or full buffers)\n",
					req->paddr);
		return;
	}

	numPendingReqs--;
	DPRINTF(OverlapEstimatorTable, "Request %d completed, found in table at place %d\n",
			req->paddr,
			curIndex);

	bool addRequest = isSharedRead(req, hiddenLoad);
	// check for unknown latencies that can be resolved
	processULBuffer(curIndex, addRequest);

	if(addRequest){
		if(numPendingReqs > 0){

			// STEP 1: move window to start of this request if necessary
			handleUnknownLatencies(curIndex);

			// STEP 2: process finished request
			for(int i=0;i<overlapTable.size();i++){
				if(overlapTable[i].valid && i != curIndex){
					assert(overlapTable[curIndex].windowStart <= overlapTable[i].windowStart);
					handleCommitInProgress(i);

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

		// STEP 3: Add finished request data to aggregate data
		handleCommitInProgress(curIndex);
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

	// Step 4: Invalidate request and update head pointer
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

int
MemoryOverlapTable::getFreeULBufferEntry(){
	int oldtail = unknownLatencyTail;
	unknownLatencyTail = (unknownLatencyTail + 1) % unknownLatencyBuffer.size();
	DPRINTF(OverlapEstimatorTable, "Allocating new unknown latency entry at pos %d\n", oldtail);

	if(unknownLatencyBuffer[oldtail].valid) fatal("Storage overflow in UL buffer");
	unknownLatencyBuffer[oldtail].valid = true;
	return oldtail;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapTable)
    Param<int> request_table_size;
	Param<int> unknown_table_size;
END_DECLARE_SIM_OBJECT_PARAMS(MemoryOverlapTable)

BEGIN_INIT_SIM_OBJECT_PARAMS(MemoryOverlapTable)
	INIT_PARAM_DFLT(request_table_size, "The size of the request table", 400),
	INIT_PARAM_DFLT(unknown_table_size, "The size of the unknown latency table", 50)
END_INIT_SIM_OBJECT_PARAMS(MemoryOverlapTable)

CREATE_SIM_OBJECT(MemoryOverlapTable)
{

    return new MemoryOverlapTable(getInstanceName(),
    		                       request_table_size,
    		                       unknown_table_size);
}

REGISTER_SIM_OBJECT("MemoryOverlapTable", MemoryOverlapTable)
#endif

