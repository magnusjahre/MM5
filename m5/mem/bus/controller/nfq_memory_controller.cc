/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/nfq_memory_controller.hh"

#define LARGE_TICK 10000000000000000ull
#define MAX_ACTIVE_PAGES 4

using namespace std;

NFQMemoryController::NFQMemoryController(std::string _name,
                                         int _rdQueueLength,
                                         int _wrQueueLength,
                                         int _spt,
                                         int _numCPUs,
                                         vector<double> _priorities,
                                         bool _infWriteBW)
    : TimingMemoryController(_name) {

    readQueueLength = _rdQueueLength;
    writeQueueLenght = _wrQueueLength;
    starvationPreventionThreshold = _spt;

    nfqNumCPUs = _numCPUs;

    virtualFinishTimes.resize(nfqNumCPUs+1, 0);
    requests.resize(nfqNumCPUs+1);

    pageCmd = new MemReq();
    pageCmd->cmd = Activate;
    pageCmd->paddr = 0;

    queuedReads = 0;
    queuedWrites = 0;
    starvationCounter = 0;

    infiniteWriteBW = _infWriteBW;

    setUpWeights(_priorities);
}

NFQMemoryController::~NFQMemoryController(){
}

int
NFQMemoryController::getQueueID(int cpuID){
	if(cpuID == -1){
		return nfqNumCPUs;
	}
	return cpuID;
}

void
NFQMemoryController::setUpWeights(std::vector<double> priorities){
    processorIncrements.resize(nfqNumCPUs+1, 0);
    assert(priorities.size() == processorIncrements.size());
    for(int i=0; i<priorities.size(); i++){
    	processorIncrements[i] = (int) ((1 / priorities[i])*1000);
    }
}

int
NFQMemoryController::insertRequest(MemReqPtr &req) {

	if(controllerInterference != NULL && !controllerInterference->isInitialized()){
		controllerInterference->initialize(nfqNumCPUs);
	}

    req->inserted_into_memory_controller = curTick;
    assert(req->cmd == Read || req->cmd == Writeback);

    if(infiniteWriteBW && req->cmd == Writeback){
        return 0;
    }

    req->cmd == Read ? queuedReads++ : queuedWrites++;
    int curCPUID = getQueueID(req->adaptiveMHASenderID);
    assert(curCPUID >= 0 && curCPUID < requests.size());

    if(curCPUID == nfqNumCPUs) fatal("We cannot support requests from unknown CPUs, something must be wrong.");

    Tick minTag = getMinStartTag();
    Tick curFinTag = -1;
	curFinTag = virtualFinishTimes[curCPUID];

    // assign start time and update virtual clock
    req->virtualStartTime = (minTag > curFinTag ? minTag : curFinTag);
    virtualFinishTimes[curCPUID] = req->virtualStartTime + processorIncrements[curCPUID];

    DPRINTF(MemoryController,
            "Inserting request into queue %d (cpu %d), addr %d, start time is %d, minimum time is %d, new finish time %d, weight %d\n",
            curCPUID,
            req->adaptiveMHASenderID,
            req->paddr,
            req->virtualStartTime,
            minTag,
            virtualFinishTimes[curCPUID],
            processorIncrements[curCPUID]);

    requests[curCPUID].push_back(req);

    if((queuedReads >= readQueueLength || queuedWrites >= writeQueueLenght) && !isBlocked() ){
        DPRINTF(MemoryController, "Blocking, #reads %d, #writes %d\n", queuedReads, queuedWrites);
        setBlocked();
    }

    if(memCtrCPUCount > 1 && controllerInterference != NULL && req->interferenceMissAt == 0 && req->adaptiveMHASenderID != -1){
    	controllerInterference->insertRequest(req);
    }

    return 0;
}

Tick
NFQMemoryController::getMinStartTag(){
    Tick min = LARGE_TICK;
    bool allEmpty = true;
    for(int i=0;i<requests.size();i++){
        for(int j=0;j<requests[i].size();j++){
            if(requests[i][j]->virtualStartTime < min){
                min = requests[i][j]->virtualStartTime;
            }
            allEmpty = false;
        }
    }
    if(allEmpty) return 0;
    return min;
}

bool
NFQMemoryController::hasMoreRequests() {

    bool allEmpty = true;

    for(int i=0;i<requests.size();i++){
        if(!requests[i].empty()) allEmpty = false;
    }

    if(!allEmpty){
        return true;
    }

    for(int i=0;i<virtualFinishTimes.size();i++){
        virtualFinishTimes[i] = 0;
    }

    DPRINTF(MemoryController, "No more requests, setting all finish times to 0\n");
    return false;
}

MemReqPtr
NFQMemoryController::getRequest() {

	checkMaxActivePages();

    MemReqPtr retval = pageCmd; // dummy initialization

    bool foundReady = false;
    bool foundColumn = false;

    if(starvationCounter < starvationPreventionThreshold){
        // 1. Prioritize ready pages
        foundReady = findColumnRequest(retval, &NFQMemoryController::isReady);

        // 2. Prioritize CAS commands
        if(!foundReady){
            foundColumn = findColumnRequest(retval, &NFQMemoryController::pageActivated);
        }
    }
    else{
        DPRINTF(MemoryController,
                "Bypassing ready check, starvation counter above threshold (cnt=%d, thres=%d)\n",
               starvationCounter,
               starvationPreventionThreshold);
    }

    bool foundRow = false;
    if(!foundReady && !foundColumn){
        // 3. Prioritize commands based on start tags
        foundRow = findRowRequest(retval);
        assert(foundRow);
        if(retval->cmd == Read || retval->cmd == Writeback){
            DPRINTF(MemoryController, "Resetting starvation counter after bypass\n");
            starvationCounter = 0;
        }
    }
    else{
        //a request may have passed an older request
        Tick oldest = getMinStartTag();
        if(oldest < retval->virtualStartTime){
            starvationCounter++;
            DPRINTF(MemoryController, "Incrementing starvation counter, value is now %d\n", starvationCounter);
        }
        else{
            assert(retval->virtualStartTime <= oldest);
            starvationCounter = 0;
            DPRINTF(MemoryController,
                    "Resetting starvation counter (req at %d, lowest %d)\n",
                    retval->virtualStartTime,
                    oldest);
        }
    }

    if(retval->cmd == Read || retval->cmd == Writeback){
    	DPRINTF(MemoryController,
    			"Executing request from cpu %d, cmd %s, addr %d, start time is %d, new finish time %d, weight %d\n",
    			retval->adaptiveMHASenderID,
    			retval->cmd,
    			retval->paddr,
    			retval->virtualStartTime,
    			virtualFinishTimes[getQueueID(retval->adaptiveMHASenderID)],
    			processorIncrements[retval->adaptiveMHASenderID]);
    }

    return retval;
}

bool
NFQMemoryController::findColumnRequest(MemReqPtr& req,
                                       bool (NFQMemoryController::*compare)(MemReqPtr&)){

    Tick minval = LARGE_TICK;
    int minrow = -1;
    int mincol = -1;

    for(int i=0;i<requests.size();i++){
        for(int j=0;j<requests[i].size();j++){
            if( (this->*compare)(requests[i][j]) ){
                if(requests[i][j]->virtualStartTime < minval){
                    minval = requests[i][j]->virtualStartTime;
                    minrow = i;
                    mincol = j;
                }
            }
        }
    }

    if(minrow != -1 && mincol != -1){
        req = prepareColumnRequest(requests[minrow][mincol]);
        requests[minrow].erase(requests[minrow].begin()+mincol);
        return true;
    }

    return false;
}

bool
NFQMemoryController::pageActivated(MemReqPtr& req){
    assert(activePages.size() <= MAX_ACTIVE_PAGES);

    for(int i=0;i<activePages.size();i++){
        if(activePages[i] == getPage(req)){
            return true;
        }
    }
    return false;
}

bool
NFQMemoryController::findRowRequest(MemReqPtr& req){

    Tick minval = LARGE_TICK;
    int minrow = -1;
    int mincol = -1;

    for(int i=0;i<requests.size();i++){
        for(int j=0;j<requests[i].size();j++){
            if(requests[i][j]->virtualStartTime < minval){
                minval = requests[i][j]->virtualStartTime;
                minrow = i;
                mincol = j;
            }
        }
    }

    DPRINTF(MemoryController,
            "Lowest virtual start time is %d, found at in queue %d and column %d\n",
             minval,
             minrow,
			 mincol);


    assert(minrow >= 0 && mincol >= 0);
    MemReqPtr oldestReq = requests[minrow][mincol];

    if(pageActivated(oldestReq)){
        // issuing oldest column command because starvation prevention threshold has been reached
        assert(starvationCounter >= starvationPreventionThreshold);
        req = prepareColumnRequest(oldestReq);
        requests[minrow].erase(requests[minrow].begin()+mincol);
        return true;
    }

    // check for pages that can be closed
    for(int i=0;i<activePages.size();i++){
        bool canClose = true;
        for(int j=0;j<requests.size();j++){
            for(int k=0;k<requests[j].size();k++){
                if(activePages[i] == getPage(requests[j][k])){
                    canClose = false;
                }
            }
        }
        if(canClose){
            req = createCloseReq(activePages[i]);
            activePages.erase(activePages.begin()+i);

            DPRINTF(MemoryController,
                    "Closing page %d request addr %d because it has no pending requests\n",
                    getPage(req),
                    req->paddr);

            return true;
        }
    }

    if(!bankIsClosed(oldestReq)){
        //bank conflict, close bank
        int hitIndex = -1;
        for(int i=0;i<activePages.size();i++){
            if(getMemoryBankID(getPageAddr(activePages[i])) == getMemoryBankID(oldestReq->paddr)){
                hitIndex = i;
            }
        }
        assert(hitIndex >= 0);
        req = createCloseReq(activePages[hitIndex]);
        activePages.erase(activePages.begin()+hitIndex);
        DPRINTF(MemoryController,
                "Closing page %d, request addr %d due to page conflict with addr %d\n",
                getPage(req),
                req->paddr,
                oldestReq->paddr);
    }
    else if(activePages.size() >= MAX_ACTIVE_PAGES){
        // a page must be closed before we can issue the request
        // close the page that has been active for the longest time
        req = createCloseReq(activePages[0]);
        activePages.erase(activePages.begin());
        DPRINTF(MemoryController,
                "Closing page %d, request addr %d to issue oldest request\n",
                getPage(req),
                req->paddr);

    }
    else{
        req = createActivateReq(oldestReq);
        activePages.push_back(getPage(oldestReq));
        DPRINTF(MemoryController,
                "Activating page, req addr %d, page addr %d, %d pages are currently active\n",
                req->paddr,
                getPage(req),
                activePages.size());
    }

    return true;
}

MemReqPtr&
NFQMemoryController::createCloseReq(Addr pageAddr){
    pageCmd->cmd = Close;
    pageCmd->paddr = getPageAddr(pageAddr);
    pageCmd->flags &= ~SATISFIED;
    return pageCmd;
}

MemReqPtr&
NFQMemoryController::createActivateReq(MemReqPtr& req){

    Addr pageAddr = getPage(req);
    pageCmd->cmd = Activate;
    pageCmd->paddr = getPageAddr(pageAddr);
    pageCmd->flags &= ~SATISFIED;
    return pageCmd;
}

MemReqPtr&
NFQMemoryController::prepareColumnRequest(MemReqPtr& req){
    assert(req->cmd == Read || req->cmd == Writeback);
    req->cmd == Writeback ? queuedWrites-- : queuedReads--;

    if(queuedReads < readQueueLength && queuedWrites < writeQueueLenght && isBlocked() ){
        DPRINTF(MemoryController, "Unblocking, #reads %d, #writes %d\n", queuedReads, queuedWrites);
        setUnBlocked();
    }

    DPRINTF(MemoryController,
            "Returning column request, start time %d, addr %d, cpu %d\n",
            req->virtualStartTime,
            req->paddr,
            req->adaptiveMHASenderID);

    return req;
}

void
NFQMemoryController::setOpenPages(std::list<Addr> pages){
    while(!pages.empty()){
        activePages.push_back(pages.front());
        pages.pop_front();
    }
}

void
NFQMemoryController::printRequestQueue(Tick fromTick){
    bool allEmpty = true;

    for(int i=0;i<requests.size();i++){
        if(!requests[i].empty()) allEmpty = false;
    }

    if(curTick >= fromTick){
        if(!allEmpty){
            cout << "\n";
            cout << "Request queue at " << curTick << "\n";

            for(int i=0;i<requests.size();i++){
                cout << "Queue " << i << ":";
                for(int j=0;j<requests[i].size();j++){
                    assert(requests[i][j]->cmd == Writeback || requests[i][j]->cmd == Read);
                    cout << " " << requests[i][j]->paddr << "(" << (requests[i][j]->cmd == Writeback ? "W" : "R") << ", " << requests[i][j]->virtualStartTime << ")";
                }
                cout << "\n";
            }
            cout << "\n";
        }
    }
}

void
NFQMemoryController::computeInterference(MemReqPtr& req, Tick busOccupiedFor){
    assert(req->interferenceMissAt == 0);
	if(controllerInterference != NULL){
		controllerInterference->estimatePrivateLatency(req, busOccupiedFor);
	}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(NFQMemoryController)
    Param<int> rd_queue_size;
    Param<int> wr_queue_size;
    Param<int> starvation_prevention_thres;
    Param<int> num_cpus;
    VectorParam<double> priorities;
    Param<bool> inf_write_bw;
END_DECLARE_SIM_OBJECT_PARAMS(NFQMemoryController)


BEGIN_INIT_SIM_OBJECT_PARAMS(NFQMemoryController)
    INIT_PARAM_DFLT(rd_queue_size, "Max read queue size", 64),
    INIT_PARAM_DFLT(wr_queue_size, "Max write queue size", 64),
    INIT_PARAM_DFLT(starvation_prevention_thres, "Starvation Prevention Threshold", 3),
    INIT_PARAM_DFLT(num_cpus, "Number of CPUs", -1),
    INIT_PARAM(priorities, "The per thread priorities"),
    INIT_PARAM_DFLT(inf_write_bw, "Infinite writeback bandwidth", false)

END_INIT_SIM_OBJECT_PARAMS(NFQMemoryController)


CREATE_SIM_OBJECT(NFQMemoryController)
{
    return new NFQMemoryController(getInstanceName(),
                                   rd_queue_size,
                                   wr_queue_size,
                                   starvation_prevention_thres,
                                   num_cpus,
                                   priorities,
                                   inf_write_bw);
}

REGISTER_SIM_OBJECT("NFQMemoryController", NFQMemoryController)

#endif //DOXYGEN_SHOULD_SKIP_THIS


