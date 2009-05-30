
#ifndef RDFCFS_CONTROLLER_INTERFERENCE_HH_
#define RDFCFS_CONTROLLER_INTERFERENCE_HH_

#include "controller_interference.hh"

class RDFCFSControllerInterference : public ControllerInterference{

private:

	bool privStorageInited;
	bool doOutOfOrderInsert;
	bool useAverageLatencies;
	bool usePureHeadPointerQueueModel;

	bool initialized;

	std::vector<RequestTrace> privateExecutionOrderTraces;
	std::vector<RequestTrace> privateArrivalOrderEstimationTraces;
	std::vector<Tick> currentPrivateSeqNum;

	int rfLimitAllCPUs;

	int bufferSize;
	bool infiniteBuffer;

	std::vector<int> lastQueueDelay;
	std::vector<int> lastServiceDelay;

	struct PrivateLatencyBufferEntry{
		PrivateLatencyBufferEntry* headAtEntry;
		PrivateLatencyBufferEntry* previous;
		PrivateLatencyBufferEntry* next;
		bool scheduled;
		bool latencyRetrieved;
		MemReqPtr req;
		bool inAccessTrace;

		PrivateLatencyBufferEntry(MemReqPtr& _req){
			headAtEntry = NULL;
			previous = NULL;
			next = NULL;
			scheduled = false;
			latencyRetrieved = false;
			req = _req;
			inAccessTrace = false;
		}

		bool canDelete(){
			if(req->cmd == VirtualPrivateWriteback){
				return scheduled;
			}

			return scheduled && latencyRetrieved;
		}
	};


	std::vector<PrivateLatencyBufferEntry*> headPointers;
	std::vector<PrivateLatencyBufferEntry*> tailPointers;
	std::vector<PrivateLatencyBufferEntry*> oldestEntryPointer;
	std::vector<int> readyFirstLimits;
	std::vector<std::vector<PrivateLatencyBufferEntry*> > privateLatencyBuffer;

public:
	RDFCFSControllerInterference(const std::string& _name,
				     TimingMemoryController* _ctlr,
				     int _rflimitAllCPUs,
				     bool _doOOOInsert,
				     int _cpu_count,
				     int _buffer_size,
				     bool _use_avg_lats,
				     bool _pure_head_ptr_model);

	void initialize(int cpu_count);

	void insertRequest(MemReqPtr& req);

	void estimatePrivateLatency(MemReqPtr& req);

	bool isInitialized(){
		return initialized;
	}

	void insertPrivateVirtualRequest(MemReqPtr& req);

private:

	void removeOldestRequest(MemReqPtr& req);

	void insertRequestOutOfOrder(MemReqPtr& req, PrivateLatencyBufferEntry* newEntry);
	Tick getEstimatedArrivalTime(MemReqPtr& req);

	PrivateLatencyBufferEntry* schedulePrivateRequest(int fromCPU);
	void executePrivateRequest(PrivateLatencyBufferEntry* entry, int fromCPU, int headPos);
	void updateHeadPointer(PrivateLatencyBufferEntry* entry, int headPos, int fromCPU);
	int getArrivalIndex(PrivateLatencyBufferEntry* entry, int fromCPU);
	int getQueuePosition(PrivateLatencyBufferEntry* entry, int fromCPU);

	void freeUsedEntries(int fromCPU);
//	void deleteBufferRange(int toIndex, int fromCPU);



	void dumpBufferStatus(int CPUID);



};

#endif /* RDFCFS_CONTROLLER_INTERFERENCE_HH_ */

