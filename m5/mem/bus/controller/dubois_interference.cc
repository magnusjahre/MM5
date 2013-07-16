
#include "dubois_interference.hh"

using namespace std;

DuBoisInterference::DuBoisInterference(const std::string& _name, int _cpu_cnt, TimingMemoryController* _ctrl)
:ControllerInterference(_name,_cpu_cnt,_ctrl)
{
    cpuCount = _cpu_cnt;
    seqNumCounter = 0;
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
    //TODO: implement ORA
    Tick serviceInt = 0;

    for(int i=0;i<interference.size();i++){
        if(req->adaptiveMHASenderID != i){
            DPRINTF(MemoryControllerInterference, "Adding queue interference for CPU %d, interference %d\n",
                    i,
                    interference[i]);
            memoryController->addBusInterference(0, interference[i], req, i);
        }
        else{
            if(isEligible(req)){
                assert(i == req->adaptiveMHASenderID);
                assert(interference[i] == 0);

                DPRINTF(MemoryControllerInterference, "Adding service interference for CPU %d, interference %d\n",
                        i,
                        serviceInt);
                memoryController->addBusInterference(serviceInt, 0, req, i);
            }
        }
    }
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
END_DECLARE_SIM_OBJECT_PARAMS(DuBoisInterference)

BEGIN_INIT_SIM_OBJECT_PARAMS(DuBoisInterference)
    INIT_PARAM_DFLT(memory_controller, "Associated memory controller", NULL),
    INIT_PARAM_DFLT(cpu_count, "number of cpus",-1)
END_INIT_SIM_OBJECT_PARAMS(FCFSControllerInterference)

CREATE_SIM_OBJECT(DuBoisInterference)
{
    return new DuBoisInterference(getInstanceName(),
                                   cpu_count,
                                   memory_controller);
}

REGISTER_SIM_OBJECT("DuBoisInterference", DuBoisInterference)

#endif
