/*
 * traffic_generator.cc
 *
 *  Created on: Apr 17, 2010
 *      Author: jahre
 */

#include "traffic_generator.hh"

using namespace std;

TrafficGenerator::TrafficGenerator(string _name,
		                           Bus* _membus,
		                           int _useID)
: SimObject(_name){

	membus = _membus;
	useID = _useID;

	membus->registerTrafficGenerator(this);

	concurrentRequestLoad = 4;

	for(int i=0;i<concurrentRequestLoad;i++){
		requestCompleted(curTick+1);
	}

	nextRequestAddr = membus->relocateAddrForCPU(useID, 0, membus->getCPUCount()+1);
}

void
TrafficGenerator::sendGeneratedRequest(){
	MemReqPtr req = new MemReq();

	req->cmd = Read;
	req->paddr = nextRequestAddr;
	req->adaptiveMHASenderID = useID;

	membus->sendGeneratedRequest(req);

	nextRequestAddr += 64;
}

void
TrafficGenerator::requestCompleted(Tick willCompleteAt){
	assert(willCompleteAt > curTick);
	TrafficGeneratorRequestEvent* genEvent = new TrafficGeneratorRequestEvent(this);
	genEvent->schedule(willCompleteAt);
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(TrafficGenerator)
    SimObjectParam<Bus* > membus;
    Param<int> use_id;
END_DECLARE_SIM_OBJECT_PARAMS(TrafficGenerator)


BEGIN_INIT_SIM_OBJECT_PARAMS(TrafficGenerator)
    INIT_PARAM(membus, "The memory bus to inject traffic into"),
    INIT_PARAM(use_id, "CPU ID for use when inserting requests")
END_INIT_SIM_OBJECT_PARAMS(TrafficGenerator)


CREATE_SIM_OBJECT(TrafficGenerator)
{
    return new TrafficGenerator(getInstanceName(),
                                membus,
                                use_id);
}

REGISTER_SIM_OBJECT("TrafficGenerator", TrafficGenerator)

#endif //DOXYGEN_SHOULD_SKIP_THIS
