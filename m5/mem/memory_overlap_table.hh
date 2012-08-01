
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

	class UnknownLatencyBufferEntry{
	public:
		int latency;
		int commit;
		std::vector<bool> bufferInvolved;
		int count;
		bool valid;

		UnknownLatencyBufferEntry(int numBuffers){
			bufferInvolved.resize(numBuffers, false);
			reset();
		}

		void reset(){
			latency = 0;
			count = 0;
			commit = 0;
			for(int i=0;i<bufferInvolved.size();i++) bufferInvolved[i] = false;
			valid = false;
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

	std::vector<UnknownLatencyBufferEntry> unknownLatencyBuffer;
	int unknownLatencyHead;
	int unknownLatencyTail;

	RequestTrace overlapTableTrace;

public:
	MemoryOverlapTable(){ }

	MemoryOverlapTable(int totalNumL1MSHRs, int unknownTableSize);

	void reset();

	void requestIssued(MemReqPtr& req);

	void requestCompleted(MemReqPtr& req, bool hiddenLoad);

	void executionResumed();

	void executionStalled();

	void initTableTrace();
	void traceTable(int insts);

private:
	void dumpBuffer();
	void invalidateEntry(int curIndex, Addr addr);

	void handleUnknownLatencies(int curIndex);
	void updateUnknownCommitLatencies(int curIndex, int firstULBuffer);
	int findTempWindowEndIndex(int curIndex);

	void processULBuffer(int curIndex, bool addReq);

	int getFreeULBufferEntry();

	int decrULIndex(int index);

	void handleCommitInProgress(int index);

	int findRequest(Addr addr);

	bool isSharedRead(MemReqPtr& req, bool hiddenLoad);
};

#endif
