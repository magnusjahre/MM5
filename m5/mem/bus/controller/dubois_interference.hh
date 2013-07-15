
#ifndef DUBOIS_INTERFERENCE_HH_
#define DUBOIS_INTERFERENCE_HH_

#include "controller_interference.hh"

class DuBoisInterference : public ControllerInterference{

//private:

public:

    DuBoisInterference(const std::string& _name, int _cpu_cnt, TimingMemoryController* _ctrl);

    void insertRequest(MemReqPtr& req);

    void estimatePrivateLatency(MemReqPtr& req);

    void initialize(int cpu_count){
        fatal("initialize not implemented");
    }

    bool isInitialized(){
        fatal("isInitialized() not implemented");
        return true;
    }

    virtual void insertPrivateVirtualRequest(MemReqPtr& req){
        fatal("not implemented");
    }
};

#endif //DUBOIS_INTERFERENCE_HH_
