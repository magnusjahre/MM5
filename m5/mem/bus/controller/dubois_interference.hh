
#ifndef DUBOIS_INTERFERENCE_HH_
#define DUBOIS_INTERFERENCE_HH_

#include "controller_interference.hh"

class DuBoisInterference : public ControllerInterference{

private:
    std::vector<MemReqPtr> pendingRequests;
    int cpuCount;

    void removeRequest(Addr paddr);

    bool isEligible(MemReqPtr& req);

public:

    DuBoisInterference(const std::string& _name, int _cpu_cnt, TimingMemoryController* _ctrl);

    void insertRequest(MemReqPtr& req);

    void estimatePrivateLatency(MemReqPtr& req, Tick busOccupiedFor);

    void initialize(int cpu_count){
        fatal("initialize() not needed for DuBoisInterference");
    }

    bool isInitialized(){
        return true;
    }

    virtual void insertPrivateVirtualRequest(MemReqPtr& req){
        fatal("not implemented");
    }

    virtual bool addsInterference(){
        return true;
    }
};

#endif //DUBOIS_INTERFERENCE_HH_
