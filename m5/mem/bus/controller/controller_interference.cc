
#include "controller_interference.hh"

ControllerInterference::ControllerInterference(const std::string& _name,
											   int _cpu_count,
											   TimingMemoryController* _ctrl)
: SimObject(_name) {

	privateBankActSeqNum = 0;

	if(_cpu_count == -1) fatal("the number of CPUs must be provided");
	contIntCpuCount = _cpu_count;

	assert(_ctrl != NULL);
	memoryController = _ctrl;
	memoryController->registerInterferenceMeasurement(this);
}

void
ControllerInterference::regStats(){

	estimatedNumberOfHits
		.init(contIntCpuCount)
		.name(name() + ".estimated_private_hits_per_cpu")
		.desc("estimated number of private hits per cpu");

	estimatedNumberOfMisses
		.init(contIntCpuCount)
		.name(name() + ".estimated_private_misses_per_cpu")
		.desc("estimated number of private misses per cpu");

	estimatedNumberOfConflicts
		.init(contIntCpuCount)
		.name(name() + ".estimated_private_conflicts_per_cpu")
		.desc("estimated number of private conflicts per cpu");

	sumConflictLatEstimate
		.init(contIntCpuCount)
		.name(name() + ".sum_conflict_latency_estimate")
		.desc("sum of conflict latency estimates per CPU");

	avgConflictLatEstimate
		.name(name() + ".avg_conflict_latency_estimate")
		.desc("average conflict latency estimates per CPU");

	avgConflictLatEstimate = sumConflictLatEstimate / estimatedNumberOfConflicts;

	sumPrivateQueueLenghts
		.init(contIntCpuCount)
		.name(name() + ".sum_private_queue_lengths")
		.desc("sum of the estimated number of reqs a had to wait for");

	numRequests
		.init(contIntCpuCount)
		.name(name() + ".number_of_reqs_in_estimations")
		.desc("number of requests in the estimations");

	avgQueueLengthEstimate
		.name(name() + ".avg_estimated_queue_length")
		.desc("average number of requests a request has to wait for");

	avgQueueLengthEstimate = sumPrivateQueueLenghts / numRequests;

	prematurelyDroppedRequests
		.init(contIntCpuCount)
		.name(name() + ".prematurely_dropped_requests")
		.desc("number of requests that were dropped before their latency could be collected");

	prematurelyDroppedRatio
		.name(name() + ".prematurely_dropped_ratio")
		.desc("number of dropped requests that got bogus measurement data per recieved request");

	prematurelyDroppedRatio = prematurelyDroppedRequests / numRequests;

	droppedRequests
		.init(contIntCpuCount)
		.name(name() + ".dropped_requests")
		.desc("number of requests that were dropped because of limited buffering");

	droppedRatio
		.name(name() + ".dropped_ratio")
		.desc("number of dropped requests per recieved request");

	droppedRatio = droppedRequests / numRequests;

}

void
ControllerInterference::initializeMemDepStructures(int bankCount){
	activatedPages.resize(contIntCpuCount, std::vector<Addr>(bankCount, 0));
	activatedAt.resize(contIntCpuCount, std::vector<Tick>(bankCount, 0));
}

void
ControllerInterference::estimatePageResult(MemReqPtr& req){

    assert(req->cmd == Read || req->cmd == Writeback || req->cmd == VirtualPrivateWriteback);

    checkPrivateOpenPage(req);

    if(isPageHitOnPrivateSystem(req)){
        req->privateResultEstimate = DRAM_RESULT_HIT;
    }
    else if(isPageConflictOnPrivateSystem(req)){
        req->privateResultEstimate = DRAM_RESULT_CONFLICT;
    }
    else{
        req->privateResultEstimate = DRAM_RESULT_MISS;
    }

    updatePrivateOpenPage(req);
}

void
ControllerInterference::checkPrivateOpenPage(MemReqPtr& req){

    int actCnt = 0;
    Tick minAct = TICK_T_MAX;
    int minID = -1;
    int bank = memoryController->getMemoryBankID(req->paddr);
    assert(req->adaptiveMHASenderID != -1);
    int cpuID = req->adaptiveMHASenderID;

    for(int i=0;i<activatedPages[cpuID].size();i++){
        if(activatedPages[cpuID][i] != 0){
            actCnt++;

            assert(activatedAt[cpuID][i] != 0);
            if(activatedAt[cpuID][i] < minAct){
                minAct = activatedAt[cpuID][i];
                minID = i;
            }
        }
    }

    assert(memoryController->getMaxActivePages() != -1);
    if(actCnt == memoryController->getMaxActivePages()){
        if(activatedPages[cpuID][bank] == 0){
            // the memory controller has closed the oldest activated page, update storage
            activatedPages[cpuID][minID] = 0;
            activatedAt[cpuID][minID] = 0;
        }
    }
    else{
        assert(actCnt < memoryController->getMaxActivePages());
    }
}

void
ControllerInterference::updatePrivateOpenPage(MemReqPtr& req){

    Addr curPage = memoryController->getPage(req->paddr);
    int bank = memoryController->getMemoryBankID(req->paddr);
    int cpuID = req->adaptiveMHASenderID;

    if(activatedPages[cpuID][bank] != curPage){
    	privateBankActSeqNum++;
    	assert(privateBankActSeqNum < TICK_T_MAX);
        activatedAt[cpuID][bank] = privateBankActSeqNum;
    }

    activatedPages[cpuID][bank] = curPage;
}

bool
ControllerInterference::isPageHitOnPrivateSystem(MemReqPtr& req){

    Addr curPage = memoryController->getPage(req->paddr);
    int bank = memoryController->getMemoryBankID(req->paddr);
    int cpuID = req->adaptiveMHASenderID;

    return curPage == activatedPages[cpuID][bank];
}

bool
ControllerInterference::isPageConflictOnPrivateSystem(MemReqPtr& req){

    Addr curPage = memoryController->getPage(req->paddr);
    int bank = memoryController->getMemoryBankID(req->paddr);
    int cpuID = req->adaptiveMHASenderID;

    return curPage != activatedPages[cpuID][bank] && activatedPages[cpuID][bank] != 0;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("ControllerInterference", ControllerInterference);

#endif
