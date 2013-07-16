
#ifndef DUBOIS_INTERFERENCE_HH_
#define DUBOIS_INTERFERENCE_HH_

#include "controller_interference.hh"

class DuBoisInterference : public ControllerInterference{

private:
    std::vector<MemReqPtr> pendingRequests;
    int cpuCount;
    int seqNumCounter;
    bool initialized;

    bool useORA;

    std::vector< std::vector<Addr> > ora;
    std::vector<Addr> sharedActivePage;

    void removeRequest(Addr seqnum);

    bool isEligible(MemReqPtr& req);

public:

    DuBoisInterference(const std::string& _name,
                          int _cpu_cnt,
                          TimingMemoryController* _ctrl,
                          bool _useORA);

    void insertRequest(MemReqPtr& req);

    void estimatePrivateLatency(MemReqPtr& req, Tick busOccupiedFor);

    void initialize(int cpu_count);

    bool isInitialized();

    virtual void insertPrivateVirtualRequest(MemReqPtr& req){
        fatal("not implemented");
    }

    virtual bool addsInterference(){
        return true;
    }
};

#endif //DUBOIS_INTERFERENCE_HH_
