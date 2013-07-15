/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/fcfs_memory_controller.hh"

using namespace std;

#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)

FCFSTimingMemoryController::FCFSTimingMemoryController(std::string _name, int _queueLength)
    : TimingMemoryController(_name) {

    queueLength = _queueLength;
    numActivePages = 0;
}

/** Frees locally allocated memory. */
FCFSTimingMemoryController::~FCFSTimingMemoryController(){
}

int FCFSTimingMemoryController::insertRequest(MemReqPtr &req) {

	if(activePages.empty()){
		Addr inval_addr = MemReq::inval_addr;
		assert(getMemoryInterface()->getMemoryBankCount() > 1);
		activePages.resize(getMemoryInterface()->getMemoryBankCount(), inval_addr);
		activatedAt.resize(getMemoryInterface()->getMemoryBankCount(), 0);
	}

	if(req->adaptiveMHASenderID != -1){
		numRequests[req->adaptiveMHASenderID]++;
		int currentPrivateCnt = 0;
		list<MemReqPtr>::iterator queueIt = memoryRequestQueue.begin();
		for( ; queueIt != memoryRequestQueue.end(); queueIt++){
			if((*queueIt)->adaptiveMHASenderID == req->adaptiveMHASenderID){
				currentPrivateCnt++;
			}
		}
		sumPrivateQueueLength[req->adaptiveMHASenderID] += currentPrivateCnt;
	}


    req->inserted_into_memory_controller = curTick;
    memoryRequestQueue.push_back(req);

    DPRINTF(MemoryControllerInterference, "Recieved request from CPU %d, addr %d, cmd %s\n",
    			                          req->adaptiveMHASenderID,
    			                          req->paddr,
    			                          req->cmd.toString());

    if (!isBlocked() && memoryRequestQueue.size() >= queueLength) {
       setBlocked();
    }

    if(memCtrCPUCount > 1 && controllerInterference != NULL && req->interferenceMissAt == 0 && req->adaptiveMHASenderID != -1){
    	controllerInterference->insertRequest(req);
    }

    return 0;
}

bool FCFSTimingMemoryController::hasMoreRequests() {
    return !memoryRequestQueue.empty();
}

MemReqPtr FCFSTimingMemoryController::getRequest() {

    MemReqPtr retval = new MemReq(); // dummy initialization

    MemReqPtr oldest = memoryRequestQueue.front();
    int toBank = getMemoryBankID(oldest->paddr);

    if(isActive(oldest) || isReady(oldest)){

    	retval = oldest;
    	memoryRequestQueue.pop_front();

    	DPRINTF(MemoryController,
    			"Returning ready request for addr %d bank %d, %d active pages\n", oldest->paddr, toBank, numActivePages);
    }
    else{
    	if(activePages[toBank] == MemReq::inval_addr){

			if(numActivePages == max_active_pages){
				int oldestBankID = -1;
				Tick oldestTime = TICK_T_MAX;
				int activeCnt = 0;

				for(int i=0;i<activePages.size();i++){
					if(activePages[i] != MemReq::inval_addr){
						activeCnt++;
						assert(activatedAt[i] != 0);
						if(activatedAt[i] < oldestTime){
							oldestTime = activatedAt[i];
							oldestBankID = i;
						}
					}
				}

				assert(oldestBankID != -1);
				assert(activeCnt == max_active_pages);

				retval->cmd = Close;
				retval->paddr = getPageAddr(activePages[oldestBankID]);

				activePages[oldestBankID] = MemReq::inval_addr;
				activatedAt[oldestBankID] = 0;
				numActivePages--;

				DPRINTF(MemoryController,
						"Need to activate but the max number of banks (%d) is active, closing oldest bank %d, activated by %d, %d active now\n",
						max_active_pages,
						oldestBankID,
						getPageAddr(activePages[oldestBankID]),
						numActivePages);
			}
			else{
				// no active page, issue activate
				assert(numActivePages < max_active_pages);

				retval->cmd = Activate;
				retval->paddr = oldest->paddr;

				activePages[toBank] = getPage(oldest);
				activatedAt[toBank] = curTick;
				numActivePages++;

				DPRINTF(MemoryController,
						"Activating page %d in bank %d, %d pages activated\n",
						getPageAddr(oldest),
						toBank,
						numActivePages);
			}
    	}
    	else{
    		// needed bank is active, issue close command
    		retval->cmd = Close;
    		retval->paddr = getPageAddr(activePages[toBank]);
    		activePages[toBank] = MemReq::inval_addr;
    		activatedAt[toBank] = 0;
    		numActivePages--;

    		DPRINTF(MemoryController,
    				"Closing page %d in bank %d, %d pages activated\n",
    				retval->paddr,
    				toBank,
    				numActivePages);
    	}

    }

    assert(numActivePages <= max_active_pages);

    if(isBlocked() && memoryRequestQueue.size() < queueLength){
        setUnBlocked();
    }

    DPRINTF(MemoryController, "Returning memory request, cmd %s, addr %d\n", retval->cmd, retval->paddr);

    return retval;
}

void
FCFSTimingMemoryController::computeInterference(MemReqPtr& req, Tick busOccupiedFor){
    assert(req->interferenceMissAt == 0);
	if(controllerInterference != NULL){
		controllerInterference->estimatePrivateLatency(req, busOccupiedFor);
	}
}

void
FCFSTimingMemoryController::setOpenPages(std::list<Addr> pages){
	fatal("setOpenPages is deprecated");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(FCFSTimingMemoryController)
    Param<int> queue_size;
END_DECLARE_SIM_OBJECT_PARAMS(FCFSTimingMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(FCFSTimingMemoryController)
    INIT_PARAM_DFLT(queue_size, "Max queue size", 64)
END_INIT_SIM_OBJECT_PARAMS(FCFSTimingMemoryController)


CREATE_SIM_OBJECT(FCFSTimingMemoryController)
{
    return new FCFSTimingMemoryController(getInstanceName(),
                                          queue_size);
}

REGISTER_SIM_OBJECT("FCFSTimingMemoryController", FCFSTimingMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS


