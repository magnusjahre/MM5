
#include "dubois_interference.hh"

using namespace std;

DuBoisInterference::DuBoisInterference(const std::string& _name, int _cpu_cnt, TimingMemoryController* _ctrl, bool _useORA)
:ControllerInterference(_name,_cpu_cnt,_ctrl)
{
    cpuCount = _cpu_cnt;
    seqNumCounter = 0;
    initialized = false;

    lastGrantedCPU = -1;
    lastEstimationAt = 0;

    useORA = _useORA;
}

void
DuBoisInterference::initialize(int cpu_count){
    int numBanks = memoryController->getMemoryInterface()->getMemoryBankCount();
    ora.resize(cpuCount, vector<Addr>(numBanks, 0));
    sharedActivePage.resize(numBanks, 0);
    initialized = true;
}

bool
DuBoisInterference::isInitialized(){
    return initialized;
}

void
DuBoisInterference::insertRequest(MemReqPtr& req){

    req->duboisSeqNum = seqNumCounter;
    seqNumCounter++;

    DPRINTF(MemoryControllerInterference, "Received request from CPU %d, addr %d, cmd %s, sequence number %d\n",
            req->adaptiveMHASenderID,
            req->paddr,
            req->cmd.toString(),
            req->duboisSeqNum);

    pendingRequests.push_back(req);
}

void
DuBoisInterference::estimatePrivateLatency(MemReqPtr& req, Tick busOccupiedFor){

    DPRINTF(MemoryControllerInterference, "Request CPU %d, addr %d, cmd %s serviced, computing interference\n",
                req->adaptiveMHASenderID,
                req->paddr,
                req->cmd.toString());

    removeRequest(req->duboisSeqNum);

    vector<Tick> interference = vector<Tick>(cpuCount, 0);

    // Case 1 and 2: Bus and bank contention
    for(int i=0;i<pendingRequests.size();i++){
        if(isEligible(pendingRequests[i])){
            if(pendingRequests[i]->adaptiveMHASenderID != req->adaptiveMHASenderID){

                DPRINTF(MemoryControllerInterference, "Request CPU %d, addr %d, was delayed, adding %d interference cycles\n",
                        pendingRequests[i]->adaptiveMHASenderID,
                        pendingRequests[i]->paddr,
                        busOccupiedFor);

                assert(req->adaptiveMHASenderID != -1);
                interference[pendingRequests[i]->adaptiveMHASenderID] += busOccupiedFor;
                pendingRequests[i]->duboisQueueInterference += busOccupiedFor;
            }

            if(pendingRequests[i]->adaptiveMHASenderID != lastGrantedCPU
               && pendingRequests[i]->inserted_into_memory_controller > lastEstimationAt){

                Tick insertionInterference = curTick - pendingRequests[i]->inserted_into_memory_controller;
                assert(insertionInterference >= 0);

                DPRINTF(MemoryControllerInterference, "Request CPU %d, addr %d, was inserted after last estimation, adding %d interference cycles\n",
                                        pendingRequests[i]->adaptiveMHASenderID,
                                        pendingRequests[i]->paddr,
                                        insertionInterference);

                assert(req->adaptiveMHASenderID != -1);
                interference[pendingRequests[i]->adaptiveMHASenderID] += insertionInterference;
                pendingRequests[i]->duboisQueueInterference += insertionInterference;
            }
        }
        else{
            DPRINTF(MemoryControllerInterference, "Request CPU %d, addr %d, is not eligible (cmd %s, %s, %s, %s)\n",
                                    pendingRequests[i]->adaptiveMHASenderID,
                                    pendingRequests[i]->paddr,
                                    pendingRequests[i]->cmd,
                                    pendingRequests[i]->isStore ? "store" : "load",
                                    pendingRequests[i]->instructionMiss ? "instruction miss" : "data miss",
                                    pendingRequests[i]->interferenceMissAt == 0 ? "intra-thread miss" : "inter-thread miss");
        }
    }



    // Case 3: Row buffer hit becomes row buffer miss
    int curBank = memoryController->getMemoryBankID(req->paddr);
    Addr curPage = memoryController->getPage(req->paddr);
    if(useORA && req->adaptiveMHASenderID != -1){

        DPRINTF(MemoryControllerInterference, "ORA check: CPU %d, addr %d, bank %d, page %d, ora page %d, shared active page %d, %s\n",
                req->adaptiveMHASenderID,
                req->paddr,
                curBank,
                curPage,
                ora[req->adaptiveMHASenderID][curBank],
                sharedActivePage[curBank],
                isEligible(req) ? "eligible" : "not eligible");

        if(ora[req->adaptiveMHASenderID][curBank] == curPage
           && sharedActivePage[curBank] != curPage){

            int serviceInt = 80; // page miss latency 120 - page hit latency 40 = 80

            if(isEligible(req)){
                DPRINTF(MemoryControllerInterference, "Adding service interference for CPU %d, interference %d\n",
                        req->adaptiveMHASenderID,
                        serviceInt);
                memoryController->addBusInterference(serviceInt, 0, req, req->adaptiveMHASenderID);
            }


            for(int i=0;i<pendingRequests.size();i++){
                if(isEligible(pendingRequests[i]) && pendingRequests[i]->adaptiveMHASenderID == req->adaptiveMHASenderID){

                    DPRINTF(MemoryControllerInterference, "The additional service interference for CPU %d also delays req for address %d, interference %d\n",
                                            req->adaptiveMHASenderID,
                                            pendingRequests[i]->paddr,
                                            serviceInt);

                    assert(req->adaptiveMHASenderID != -1);
                    interference[pendingRequests[i]->adaptiveMHASenderID] += serviceInt;
                    pendingRequests[i]->duboisQueueInterference += serviceInt;
                }
            }
        }

        ora[req->adaptiveMHASenderID][curBank] = curPage;

        DPRINTF(MemoryControllerInterference, "ORA update: page for cpu %d, bank %d is now %d\n",
                req->adaptiveMHASenderID,
                curBank,
                ora[req->adaptiveMHASenderID][curBank]);
    }

    sharedActivePage[curBank] = curPage;
    DPRINTF(MemoryControllerInterference, "Shared bank update: bank %d is active for page %d\n",
            curBank,
            sharedActivePage[curBank]);

    for(int i=0;i<interference.size();i++){
        DPRINTF(MemoryControllerInterference, "Adding queue interference for CPU %d, interference %d\n",
                i,
                interference[i]);
        memoryController->addBusInterference(0, interference[i], req, i);
    }

    lastEstimationAt = curTick;
    lastGrantedCPU = req->adaptiveMHASenderID;
}

void
DuBoisInterference::removeRequest(Addr seqnum){

    int removeIndex = -1;
    for(int i=0;i<pendingRequests.size();i++){
        if(pendingRequests[i]->duboisSeqNum == seqnum){
            assert(removeIndex == -1);
            removeIndex = i;
        }
    }
    assert(removeIndex != -1);

    DPRINTF(MemoryControllerInterference, "Removing request for address %d at index %d, sequence number %d, %d requests pending\n",
            pendingRequests[removeIndex]->paddr,
            removeIndex,
            seqnum,
            pendingRequests.size());

    pendingRequests.erase(pendingRequests.begin()+removeIndex);
}

bool
DuBoisInterference::isEligible(MemReqPtr& req){
    return req->cmd == Read
            && !req->isStore
            && !req->instructionMiss
            && req->interferenceMissAt == 0
            && req->adaptiveMHASenderID != -1;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(DuBoisInterference)
    SimObjectParam<TimingMemoryController*> memory_controller;
    Param<int> cpu_count;
    Param<bool> use_ora;
END_DECLARE_SIM_OBJECT_PARAMS(DuBoisInterference)

BEGIN_INIT_SIM_OBJECT_PARAMS(DuBoisInterference)
    INIT_PARAM_DFLT(memory_controller, "Associated memory controller", NULL),
    INIT_PARAM_DFLT(cpu_count, "number of cpus",-1),
    INIT_PARAM_DFLT(use_ora, "use the ORA to estmate row hit interference effects",true)
END_INIT_SIM_OBJECT_PARAMS(FCFSControllerInterference)

CREATE_SIM_OBJECT(DuBoisInterference)
{
    return new DuBoisInterference(getInstanceName(),
                                   cpu_count,
                                   memory_controller,
                                   use_ora);
}

REGISTER_SIM_OBJECT("DuBoisInterference", DuBoisInterference)

#endif
