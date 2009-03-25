/*
 * controller_interference.hh
 *
 *  Created on: Mar 25, 2009
 *      Author: jahre
 */

#ifndef CONTROLLER_INTERFERENCE_HH_
#define CONTROLLER_INTERFERENCE_HH_

#include "mem/requesttrace.hh"
#include "mem/mem_req.hh"
#include "mem/bus/controller/memory_controller.hh"

#include <vector>

class ControllerInterference{

private:

	int contIntCpuCount;
	bool privStorageInited;

	TimingMemoryController* memoryController;

	std::vector<RequestTrace> privateExecutionOrderTraces;
	std::vector<RequestTrace> privateArrivalOrderEstimationTraces;
	std::vector<Tick> currentPrivateSeqNum;

	int rfLimitAllCPUs;

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
			return scheduled && latencyRetrieved;
		}
	};

	Tick privateBankActSeqNum;
	std::vector<PrivateLatencyBufferEntry*> headPointers;
	std::vector<PrivateLatencyBufferEntry*> tailPointers;
	std::vector<int> readyFirstLimits;
	std::vector<std::vector<PrivateLatencyBufferEntry*> > privateLatencyBuffer;

	std::vector<std::vector<Addr> > activatedPages;
	std::vector<std::vector<Tick> > activatedAt;

public:
	ControllerInterference(TimingMemoryController* _ctlr, int _rflimitAllCPUs);

	void initialize(int cpu_count);

	void initializeMemDepStructures(int bankCount);

	void insertRequest(MemReqPtr& req);

	void estimatePrivateLatency(MemReqPtr& req);

private:

	PrivateLatencyBufferEntry* schedulePrivateRequest(int fromCPU);
	void executePrivateRequest(PrivateLatencyBufferEntry* entry, int fromCPU, int headPos);
	void updateHeadPointer(PrivateLatencyBufferEntry* entry, int headPos, int fromCPU);
	int getArrivalIndex(PrivateLatencyBufferEntry* entry, int fromCPU);
	int getQueuePosition(PrivateLatencyBufferEntry* entry, int fromCPU);
	void deleteBufferRange(int toIndex, int fromCPU);

	void checkPrivateOpenPage(MemReqPtr& req);
	bool isPageHitOnPrivateSystem(MemReqPtr& req);
	bool isPageConflictOnPrivateSystem(MemReqPtr& req);
	void updatePrivateOpenPage(MemReqPtr& req);

	void estimatePageResult(MemReqPtr& req);

	void dumpBufferStatus(int CPUID);

	Tick getEstimatedArrivalTime(MemReqPtr& req);

};

#endif /* CONTROLLER_INTERFERENCE_HH_ */
