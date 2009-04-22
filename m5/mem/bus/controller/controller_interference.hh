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

	Tick privateBankActSeqNum;

	std::vector<std::vector<Addr> > activatedPages;
	std::vector<std::vector<Tick> > activatedAt;

	void checkPrivateOpenPage(MemReqPtr& req);
	bool isPageConflictOnPrivateSystem(MemReqPtr& req);
	void updatePrivateOpenPage(MemReqPtr& req);

protected:

	Stats::Vector<> estimatedNumberOfMisses;
	Stats::Vector<> estimatedNumberOfHits;
	Stats::Vector<> estimatedNumberOfConflicts;

	Stats::Vector<> sumConflictLatEstimate;
	Stats::Formula avgConflictLatEstimate;

	Stats::Vector<> sumPrivateQueueLenghts;
	Stats::Vector<> numRequests;
	Stats::Formula avgQueueLengthEstimate;


	TimingMemoryController* memoryController;
	int contIntCpuCount;

	bool isPageHitOnPrivateSystem(MemReqPtr& req);
	void estimatePageResult(MemReqPtr& req);

	void regStats();

public:

	ControllerInterference(const std::string& _name,
						   int _cpu_count,
						   TimingMemoryController* _ctrl);

	void initializeMemDepStructures(int bankCount);

	virtual void initialize(int cpu_count) = 0;

	virtual void insertRequest(MemReqPtr& req) = 0;

	virtual void estimatePrivateLatency(MemReqPtr& req) = 0;

	virtual bool isInitialized() = 0;

};

#endif //CONTROLLER_INTERFERENCE_HH_
