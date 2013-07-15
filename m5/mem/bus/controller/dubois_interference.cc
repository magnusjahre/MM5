
#include "dubois_interference.hh"

using namespace std;

DuBoisInterference::DuBoisInterference(const std::string& _name, int _cpu_cnt, TimingMemoryController* _ctrl)
:ControllerInterference(_name,_cpu_cnt,_ctrl)
{
    fatal("not implemented");
}

void
DuBoisInterference::insertRequest(MemReqPtr& req){
    fatal("not implemented");
}

void
DuBoisInterference::estimatePrivateLatency(MemReqPtr& req){
    fatal("not implemented");
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
