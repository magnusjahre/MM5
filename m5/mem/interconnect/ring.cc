
#include "sim/builder.hh"
#include "ring.hh"

using namespace std;

Ring::Ring(const std::string &_name, 
                               int _width, 
                               int _clock,
                               int _transDelay,
                               int _arbDelay,
                               int _cpu_count,
                               HierParams *_hier,
                               AdaptiveMHA* _adaptiveMHA)
    : AddressDependentIC(_name,
                         _width, 
                         _clock, 
                         _transDelay, 
                         _arbDelay,
                         _cpu_count,
                         _hier,
                         _adaptiveMHA)
{
    initQueues(_cpu_count,_cpu_count);
}

void
Ring::send(MemReqPtr& req, Tick time, int fromID){
    
    fatal("send ni");
    
}

void 
Ring::arbitrate(Tick time){
    
    fatal("arbitrate ni");
}

void 
Ring::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){
    fatal("deliver ni");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Ring)
    Param<int> width;
    Param<int> clock;
    Param<int> transferDelay;
    Param<int> arbitrationDelay;
    Param<int> cpu_count;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
END_DECLARE_SIM_OBJECT_PARAMS(Ring)

BEGIN_INIT_SIM_OBJECT_PARAMS(Ring)
    INIT_PARAM(width, "the width of the crossbar transmission channels"),
    INIT_PARAM(clock, "crossbar clock"),
    INIT_PARAM(transferDelay, "crossbar transfer delay in CPU cycles"),
    INIT_PARAM(arbitrationDelay, "crossbar arbitration delay in CPU cycles"),
    INIT_PARAM(cpu_count, "the number of CPUs in the system"),
    INIT_PARAM_DFLT(hier, "Hierarchy global variables", &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "AdaptiveMHA object", NULL)
END_INIT_SIM_OBJECT_PARAMS(Ring)

CREATE_SIM_OBJECT(Ring)
{
    return new Ring(getInstanceName(),
                              width,
                              clock,
                              transferDelay,
                              arbitrationDelay,
                              cpu_count,
                              hier,
                              adaptive_mha);
}

REGISTER_SIM_OBJECT("Ring", Ring)

#endif //DOXYGEN_SHOULD_SKIP_THIS