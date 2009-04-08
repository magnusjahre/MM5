/*
 * controller_interference.hh
 *
 *  Created on: Mar 25, 2009
 *      Author: jahre
 */

#ifndef CONTROLLER_INTERFERENCE_HH_
#define CONTROLLER_INTERFERENCE_HH_

#include "sim/sim_object.hh"
#include "sim/builder.hh"

#include "mem/requesttrace.hh"
#include "mem/mem_req.hh"
#include "mem/bus/controller/memory_controller.hh"

#include "base/statistics.hh"

class TimingMemoryController;

#include <vector>

class ControllerInterference : public SimObject{

private:

	int contIntCpuCount;
	bool privStorageInited;
	bool doOutOfOrderInsert;

	bool initialized;

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

protected:

	Stats::Vector<> estimatedNumberOfMisses;
	Stats::Vector<> estimatedNumberOfHits;
	Stats::Vector<> estimatedNumberOfConflicts;

public:
	ControllerInterference(const std::string& _name,
						   TimingMemoryController* _ctlr,
						   int _rflimitAllCPUs,
						   bool _doOOOInsert,
						   int _cpu_count);

	void initialize(int cpu_count);

	void initializeMemDepStructures(int bankCount);

	void insertRequest(MemReqPtr& req);

	void estimatePrivateLatency(MemReqPtr& req);

	bool isInitialized(){
		return initialized;
	}

	void regStats();

private:

	void insertRequestOutOfOrder(MemReqPtr& req, PrivateLatencyBufferEntry* newEntry);
	Tick getEstimatedArrivalTime(MemReqPtr& req);

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



};

#endif /* CONTROLLER_INTERFERENCE_HH_ */

