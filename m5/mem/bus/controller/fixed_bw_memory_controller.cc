/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/fixed_bw_memory_controller.hh"

using namespace std;

#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)

FixedBandwidthMemoryController::FixedBandwidthMemoryController(std::string _name, int _queueLength, int _cpuCount)
    : TimingMemoryController(_name) {

    queueLength = _queueLength;
    numActivePages = 0;
    cpuCount = _cpuCount;

    perCoreReads.resize(_cpuCount, vector<MemReqPtr>());

    if(_cpuCount == 1) fatal("The FixedBandwidth controller only makes sense for multi-core configurations");
}

/** Frees locally allocated memory. */
FixedBandwidthMemoryController::~FixedBandwidthMemoryController(){
}

int FixedBandwidthMemoryController::insertRequest(MemReqPtr &req) {


    req->inserted_into_memory_controller = curTick;

    DPRINTF(MemoryController, "Recieved request from CPU %d, addr %d, cmd %s\n",
    			              req->adaptiveMHASenderID,
    			              req->paddr,
    			              req->cmd.toString());


    if(req->adaptiveMHASenderID != -1 && req->cmd == Read){
    	DPRINTF(MemoryController, "Added request to read queue for core %d\n", req->adaptiveMHASenderID);
    	perCoreReads[req->adaptiveMHASenderID].push_back(req);
    }
    else{
    	DPRINTF(MemoryController, "Added request to other queue\n");
    	otherRequests.push_back(req);
    }

    checkBlockingStatus();

    if(memCtrCPUCount > 1 && controllerInterference != NULL && req->interferenceMissAt == 0 && req->adaptiveMHASenderID != -1){
    	controllerInterference->insertRequest(req);
    }

    return 0;
}

void
FixedBandwidthMemoryController::checkBlockingStatus(){

	bool shouldBeBlocked = false;

	for(int i=0;i<perCoreReads.size();i++){
		assert(perCoreReads[i].size() <= queueLength);
		if(perCoreReads[i].size() == queueLength){
			shouldBeBlocked = true;
		}
	}
	assert(otherRequests.size() <= queueLength);
	if(otherRequests.size() == queueLength){
		shouldBeBlocked = true;
	}

	if(isBlocked() && !shouldBeBlocked){
		DPRINTF(MemoryController, "Unblocking controller\n");
		setUnBlocked();
	}


	if(!isBlocked() && shouldBeBlocked){
		DPRINTF(MemoryController, "Blocking controller\n");
		setBlocked();
	}

}

bool FixedBandwidthMemoryController::hasMoreRequests() {
	for(int i=0;i<perCoreReads.size();i++){
		if(!perCoreReads[i].empty()) return true;
	}
	if(!otherRequests.empty()) return true;
    return false;
}

MemReqPtr FixedBandwidthMemoryController::getRequest() {

    MemReqPtr retval = new MemReq(); // dummy initialization

    fatal("get request not implemeneted");

    checkBlockingStatus();

    return retval;
}

void
FixedBandwidthMemoryController::computeInterference(MemReqPtr& req, Tick busOccupiedFor){
    assert(req->interferenceMissAt == 0);
	if(controllerInterference != NULL){
		controllerInterference->estimatePrivateLatency(req);
	}
}

void
FixedBandwidthMemoryController::setOpenPages(std::list<Addr> pages){
	fatal("setOpenPages is deprecated");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(FixedBandwidthMemoryController)
    Param<int> queue_size;
	Param<int> cpu_count;
END_DECLARE_SIM_OBJECT_PARAMS(FixedBandwidthMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(FixedBandwidthMemoryController)
    INIT_PARAM_DFLT(queue_size, "Max queue size", 64),
    INIT_PARAM(cpu_count, "Number of cores in the system")
END_INIT_SIM_OBJECT_PARAMS(FixedBandwidthMemoryController)


CREATE_SIM_OBJECT(FixedBandwidthMemoryController)
{
    return new FixedBandwidthMemoryController(getInstanceName(),
                                          	  queue_size,
                                          	  cpu_count);
}

REGISTER_SIM_OBJECT("FixedBandwidthMemoryController", FixedBandwidthMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS


