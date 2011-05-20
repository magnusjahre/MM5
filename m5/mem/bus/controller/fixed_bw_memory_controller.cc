/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/fixed_bw_memory_controller.hh"

using namespace std;

#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)

FixedBandwidthMemoryController::FixedBandwidthMemoryController(std::string _name,
															   int _queueLength,
															   int _cpuCount,
															   int _starvationThreshold)
    : TimingMemoryController(_name) {

    queueLength = _queueLength;
    cpuCount = _cpuCount;
    curSeqNum = 0;

    tokens.resize(_cpuCount+1, 0.0);
    targetAllocation.resize(_cpuCount+1, -1.0);
    lastRunAt = 0;

    invalidRequest = new MemReq();
    assert(invalidRequest->paddr == MemReq::inval_addr);

    close = new MemReq();
    activate = new MemReq();

    if(_cpuCount == 1) fatal("The FixedBandwidth controller only makes sense for multi-core configurations");

    starvationThreshold = _starvationThreshold;
    starvationCounter = 0;
}

/** Frees locally allocated memory. */
FixedBandwidthMemoryController::~FixedBandwidthMemoryController(){
}

int FixedBandwidthMemoryController::insertRequest(MemReqPtr &req) {

	if(controllerInterference != NULL && !controllerInterference->isInitialized()){
		controllerInterference->initialize(cpuCount);
	}

    req->inserted_into_memory_controller = curTick;
    req->memCtrlSequenceNumber = curSeqNum;
    curSeqNum++;

    DPRINTF(MemoryController, "Received request from CPU %d, addr %d, cmd %s, sequence number %d\n",
    			              req->adaptiveMHASenderID,
    			              req->paddr,
    			              req->cmd.toString(),
    			              req->memCtrlSequenceNumber);

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
		DPRINTF(MemoryController, "Unblocking controller, %d queued requests\n", requests.size());
		setUnBlocked();
	}


	if(!isBlocked() && shouldBeBlocked){
		DPRINTF(MemoryController, "Blocking controller, %d queued requests\n", requests.size());
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
			tokens[i] += newTokens;

			DPRINTF(MemoryController, "Adding %f tokens for CPU %d, allocation is %f, tokens are now %f\n",
					newTokens,
					i,
					targetAllocation[i],
					tokens[i]);
		}
	}

	lastRunAt = curTick;
}

bool
FixedBandwidthMemoryController::hasMoreTokens(MemReqPtr& req1, MemReqPtr& req2){
	if(targetAllocation[getQueueID(req1->adaptiveMHASenderID)] == -1.0){
		assert(targetAllocation[getQueueID(req2->adaptiveMHASenderID)] == -1.0);
		return false;
	}

	assert(starvationCounter <= starvationThreshold);
	if(starvationCounter == starvationThreshold){
		return false;
	}

	return tokens[getQueueID(req1->adaptiveMHASenderID)] > tokens[getQueueID(req2->adaptiveMHASenderID)];
}

bool
FixedBandwidthMemoryController::hasEqualTokens(MemReqPtr& req1, MemReqPtr& req2){
	if(targetAllocation[getQueueID(req1->adaptiveMHASenderID)] == -1.0){
		assert(targetAllocation[getQueueID(req2->adaptiveMHASenderID)] == -1.0);
		return true;
	}

	assert(starvationCounter <= starvationThreshold);
	if(starvationCounter == starvationThreshold){
		return true;
	}

	return tokens[getQueueID(req1->adaptiveMHASenderID)] == tokens[getQueueID(req2->adaptiveMHASenderID)];
}

bool
FixedBandwidthMemoryController::isOlder(MemReqPtr& req1, MemReqPtr& req2){
	return req1->memCtrlSequenceNumber < req2->memCtrlSequenceNumber;
}

bool
FixedBandwidthMemoryController::consideredReady(MemReqPtr& req){
	assert(starvationCounter <= starvationThreshold);
	if(starvationCounter == starvationThreshold){
		return false;
	}
	return isReady(req);
}

MemReqPtr
FixedBandwidthMemoryController::findHighestPriRequest(){

	MemReqPtr retval = requests[0];

	for(int i=0;i<requests.size();i++){
		MemReqPtr testreq = requests[i];
		if(consideredReady(testreq)){
			if(consideredReady(retval) && hasMoreTokens(retval, testreq)){
				continue;
			}
			if(consideredReady(retval) && hasEqualTokens(retval, testreq) && isOlder(retval, testreq)){
				continue;
			}
			retval = testreq;
		}
		else if(!consideredReady(testreq) && !consideredReady(retval)){
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

	// TODO: add support for closed page policy here

	if(isActive(highestPriReq)) return invalidRequest;

	assert(activePages.size() <= max_active_pages);

	if(!bankIsClosed(highestPriReq)){

		vector<Addr>::iterator it;
		for(it=activePages.begin();it!=activePages.end();it++){
			if(getMemoryBankID(getPageAddr(*it)) == getMemoryBankID(highestPriReq->paddr)){
				break;
			}
		}

		assert(it != activePages.end());
		prepareCloseReq(*it);
		activePages.erase(it);
		return close;
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

	if(req->memCtrlSequenceNumber== requests[0]->memCtrlSequenceNumber){
		starvationCounter = 0;

		DPRINTF(MemoryController, "Request for addr %d, sequence number %d is the oldest, resetting counter\n",
				req->paddr,
				req->memCtrlSequenceNumber);
	}
	else{
		starvationCounter++;
		DPRINTF(MemoryController, "Request for addr %d, sequence number %d is not oldest, starvation counter is now %d\n",
				req->paddr,
				req->memCtrlSequenceNumber,
				starvationCounter);
	}

	vector<MemReqPtr>::iterator it;
	for(it = requests.begin(); it != requests.end(); it++){
		if((*it)->memCtrlSequenceNumber == req->memCtrlSequenceNumber) break;
	}
	assert(it != requests.end());
	DPRINTF(MemoryController, "Removing request for addr %d, sequence number %d\n",
			(*it)->paddr,
			(*it)->memCtrlSequenceNumber);
	requests.erase(it);
}

void
FixedBandwidthMemoryController::lastRequestLatency(int cpuID, int latency){
	tokens[getQueueID(cpuID)] = tokens[getQueueID(cpuID)] - latency;
	DPRINTF(MemoryController, "Last request for CPU %d took %d ticks, tokens are now %f\n",
			cpuID,
			latency,
			tokens[getQueueID(cpuID)]);
}

MemReqPtr
FixedBandwidthMemoryController::getRequest() {

	DPRINTFR(MemoryController, "Current Queue: ");
	for(int i=0;i<requests.size();i++){
		DPRINTFR(MemoryController, "(%d, %d, %d, %d)",
				requests[i]->adaptiveMHASenderID,
				requests[i]->paddr,
				requests[i]->memCtrlSequenceNumber,
				requests[i]->inserted_into_memory_controller);
	}
	DPRINTFR(MemoryController, "\n");

	addTokens();

    MemReqPtr issueReq = findHighestPriRequest();
    DPRINTF(MemoryController, "Highest priority req is from CPU %d, cmd %s, address %d, bank %d, queued requests %d\n",
    		issueReq->adaptiveMHASenderID,
    		issueReq->cmd,
    		issueReq->paddr,
    		getMemoryBankID(issueReq->paddr),
    		requests.size());

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

void
FixedBandwidthMemoryController::setBandwidthQuotas(std::vector<double> quotas){
	assert(quotas.size() == targetAllocation.size());
	for(int i=0;i<targetAllocation.size();i++){
		targetAllocation[i] = quotas[i];
		tokens[i] = 0.0;
		DPRINTF(MemoryController, "New allocation for CPU %d is %f, resetting tokens\n", i, targetAllocation[i]);
	}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(FixedBandwidthMemoryController)
    Param<int> queue_size;
	Param<int> cpu_count;
	Param<int> starvation_threshold;
END_DECLARE_SIM_OBJECT_PARAMS(FixedBandwidthMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(FixedBandwidthMemoryController)
    INIT_PARAM_DFLT(queue_size, "Max queue size", 64),
    INIT_PARAM(cpu_count, "Number of cores in the system"),
    INIT_PARAM_DFLT(starvation_threshold, "Number of consecutive requests that are allowed to bypass other requests", 10)
END_INIT_SIM_OBJECT_PARAMS(FixedBandwidthMemoryController)


CREATE_SIM_OBJECT(FixedBandwidthMemoryController)
{
    return new FixedBandwidthMemoryController(getInstanceName(),
                                          	  queue_size,
                                          	  cpu_count,
                                          	  starvation_threshold);
}

REGISTER_SIM_OBJECT("FixedBandwidthMemoryController", FixedBandwidthMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS


