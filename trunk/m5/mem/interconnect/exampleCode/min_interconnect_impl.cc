
#include "sim/builder.hh"
#include "ideal_interconnect.hh"
        
using namespace std;

void
IdealInterconnect::request(Tick time, int fromID){
    fatal("not impl");
}

void
IdealInterconnect::send(MemReqPtr& req, Tick time, int fromID){
    fatal("not impl");
}

void
IdealInterconnect::arbitrate(Tick cycle){
    fatal("not impl");
}

void
IdealInterconnect::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){
    fatal("not impl");
}

void
IdealInterconnect::setBlocked(int fromInterface){
    fatal("not impl");
}

void
IdealInterconnect::clearBlocked(int fromInterface){
    fatal("not impl");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(IdealInterconnect)
    Param<int> width;
    Param<int> clock;
    Param<int> transferDelay;
    Param<int> arbitrationDelay;
    SimObjectParam<HierParams *> hier;
END_DECLARE_SIM_OBJECT_PARAMS(IdealInterconnect)

BEGIN_INIT_SIM_OBJECT_PARAMS(IdealInterconnect)
    INIT_PARAM(width, "set this to 0, is not used"),
    INIT_PARAM(clock, "ideal interconnect clock"),
    INIT_PARAM(transferDelay, "ideal interconnect transfer delay in CPU cycles"),
    INIT_PARAM(arbitrationDelay, "ideal interconnect arbitration delay in CPU cycles"),
    INIT_PARAM_DFLT(hier,
                    "Hierarchy global variables",
                    &defaultHierParams)
END_INIT_SIM_OBJECT_PARAMS(IdealInterconnect)

CREATE_SIM_OBJECT(IdealInterconnect)
{
    return new IdealInterconnect(getInstanceName(),
                                 width,
                                 clock,
                                 transferDelay,
                                 arbitrationDelay,
                                 hier);
}

REGISTER_SIM_OBJECT("IdealInterconnect", IdealInterconnect)

#endif //DOXYGEN_SHOULD_SKIP_THIS
