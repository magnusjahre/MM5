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
    cpuCount = _cpuCount;

    tokens.resize(_cpuCount+1, 0.0);
    //targetAllocation.resize(_cpuCount+1, -1.0); // FIXME: reinsert
    targetAllocation.resize(_cpuCount+1, 1.0/(_cpuCount+1)); // FIXME: remove
    lastRunAt = 0;

    invalidRequest = new MemReq();
    assert(invalidRequest->paddr == MemReq::inval_addr);

    close = new MemReq();
    activate = new MemReq();

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

    requests.push_back(req);

    checkBlockingStatus();

    if(memCtrCPUCount > 1 && controllerInterference != NULL && req->interferenceMissAt == 0 && req->adaptiveMHASenderID != -1){
    	controllerInterference->insertRequest(req);
    }

    return 0;
}

void
FixedBandwidthMemoryController::checkBlockingStatus(){

	bool shouldBeBlocked = false;

	assert(requests.size() <= queueLength);
	if(requests.size() == queueLength){
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

bool
FixedBandwidthMemoryController::hasMoreRequests() {
	return !requests.empty();
}

void
FixedBandwidthMemoryController::addTokens(){
	for(int i=0;i<tokens.size();i++){
		if(targetAllocation[i] > 0){
			assert(targetAllocation[i] > 0 && targetAllocation[i] < 1.0);
			double newTokens = (curTick - lastRunAt) * targetAllocation[i];
			DPRINTF(MemoryController, "Adding %f tokens for CPU %d, allocation is %f\n", newTokens, i, targetAllocation[i]);
			tokens[i] += newTokens;
		}
	}

	lastRunAt = curTick;
}

bool
FixedBandwidthMemoryController::hasMoreTokens(MemReqPtr& req1, MemReqPtr& req2){
	return tokens[getQueueID(req1->adaptiveMHASenderID)] > tokens[getQueueID(req2->adaptiveMHASenderID)];
}

bool
FixedBandwidthMemoryController::hasEqualTokens(MemReqPtr& req1, MemReqPtr& req2){
	return tokens[getQueueID(req1->adaptiveMHASenderID)] == tokens[getQueueID(req2->adaptiveMHASenderID)];
}

bool
FixedBandwidthMemoryController::isOlder(MemReqPtr& req1, MemReqPtr& req2){
	return req1->inserted_into_memory_controller < req2->inserted_into_memory_controller;
}

MemReqPtr
FixedBandwidthMemoryController::findHighestPriRequest(){

	MemReqPtr retval = requests[0];

	for(int i=0;i<requests.size();i++){
		MemReqPtr testreq = requests[i];
		if(isReady(testreq)){
			if(isReady(retval) && hasMoreTokens(retval, testreq)){
				continue;
			}
			if(isReady(retval) && hasEqualTokens(retval, testreq) && isOlder(retval, testreq)){
				continue;
			}
			retval = testreq;
		}
		else if(!isReady(testreq) && !isReady(retval)){
			if(hasMoreTokens(retval, testreq)){
				continue;
			}
			if(hasEqualTokens(retval, testreq) && isOlder(retval, testreq)){
				continue;
			}
			retval = testreq;
		}

	}

	assert(retval->paddr != MemReq::inval_addr);
	return retval;
}

void
FixedBandwidthMemoryController::prepareCloseReq(Addr pageAddr){

	close->paddr = getPageAddr(pageAddr);

    close->cmd = Close;
    close->paddr = getPageAddr(pageAddr);
    close->flags &= ~SATISFIED;
}

MemReqPtr
FixedBandwidthMemoryController::findAdminRequests(MemReqPtr& highestPriReq){
	if(isActive(highestPriReq)) return invalidRequest;

	assert(activePages.size() <= max_active_pages);

	// TODO: pass over active banks and close all that don't have pending reqs

	if(!bankIsClosed(highestPriReq)){
		fatal("need to close specific bank");
	}
	else if(activePages.size() == max_active_pages){

		prepareCloseReq(activePages.front());
		activePages.erase(activePages.begin());

		return close;

	}

	Addr page = getPage(highestPriReq);
	activePages.push_back(page);

	activate->cmd = Activate;
	activate->paddr = getPageAddr(page);
	activate->flags &= ~SATISFIED;

	return activate;
}

void
FixedBandwidthMemoryController::removeRequest(MemReqPtr& req){
	vector<MemReqPtr>::iterator it;
	for(it = requests.begin(); it != requests.end(); it++){
		if((*it)->paddr == req->paddr) break;
	}
	assert(it != requests.end());
	DPRINTF(MemoryController, "removing request for addr %d\n", (*it)->paddr);
	requests.erase(it);
}

MemReqPtr
FixedBandwidthMemoryController::getRequest() {

	addTokens();

    MemReqPtr issueReq = findHighestPriRequest();
    DPRINTF(MemoryController, "Highest priority req is from CPU %d, cmd %s, address %d, bank %d\n",
    		issueReq->adaptiveMHASenderID,
    		issueReq->cmd,
    		issueReq->paddr,
    		getMemoryBankID(issueReq->paddr));

    MemReqPtr adminReq = findAdminRequests(issueReq);

    checkBlockingStatus();

    if(adminReq->paddr == MemReq::inval_addr){
    	removeRequest(issueReq);
    	DPRINTF(MemoryController, "Returning %s req for CPU %d, paddr %d\n", issueReq->cmd, issueReq->adaptiveMHASenderID, issueReq->paddr);
    	return issueReq;
    }
    DPRINTF(MemoryController, "DRAM state updates needed, returning %s for paddr %d\n", adminReq->cmd, adminReq->paddr);

    return adminReq;
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


