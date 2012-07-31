
#ifndef MEMORY_OVERLAP_TABLE_HH_
#define MEMORY_OVERLAP_TABLE_HH_

#include "mem/memory_overlap_estimator.hh"

class MemoryOverlapTable{
private:
	class MemoryOverlapTableEntry{
	public:
		Addr address;
		Tick issuedAt;
		Tick windowStart;
		bool valid;

		Tick tmpComStart;
		int tmpComOverlap;

		int commitOverlap;
		int requestOverlap;
		int stall;

		MemoryOverlapTableEntry(){
			reset();
		}

		void reset(){
			address = MemReq::inval_addr;
			issuedAt = 0;
			valid = false;
			commitOverlap = 0;
			requestOverlap = 0;
			stall = 0;
			tmpComStart = 0;
			tmpComOverlap = 0;
			windowStart = 0;
		}
	};

	Tick hiddenLatencyRequest;
	Tick hiddenLatencyCompute;
	Tick totalLatency;
	Tick totalStall;

	std::vector<MemoryOverlapTableEntry> overlapTable;
	Tick pendingComStartAt;
	int numPendingReqs;

	int headindex;
	int tailindex;

public:
	MemoryOverlapTable(){ }

	MemoryOverlapTable(int totalNumL1MSHRs, int comTableSize);

	void reset();

	void requestIssued(MemReqPtr& req);

	void requestCompleted(MemReqPtr& req);

	void executionResumed();

	void executionStalled();

private:
	void dumpBuffer();
	void invalidateEntry(int curIndex, Addr addr);
};

#endif
