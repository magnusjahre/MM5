/*
 * traffic_generator.hh
 *
 *  Created on: Apr 17, 2010
 *      Author: jahre
 */

#include <string>

#include "sim/sim_object.hh"
#include "sim/builder.hh"
#include "mem/bus/bus.hh"
#include "mem/mem_req.hh"
#include "sim/eventq.hh"

#ifndef TRAFFIC_GENERATOR_HH_
#define TRAFFIC_GENERATOR_HH_

class TrafficGenerator : public SimObject{

private:
	Bus* membus;
	int useID;
	int concurrentRequestLoad;

public:

	TrafficGenerator(std::string _name,
			         Bus* _membus,
			         int _useID);

	void sendGeneratedRequest();

	void requestCompleted(Tick willCompleteAt);
};

class TrafficGeneratorRequestEvent : public Event
{
	TrafficGenerator* gen;

public:
	TrafficGeneratorRequestEvent(TrafficGenerator* _gen)
	: Event(&mainEventQueue), gen(_gen) { }

	void process(){
		gen->sendGeneratedRequest();
		delete this;
	}

	virtual const char *description(){
		return "Traffic Generator Request Event";
	}
};

#endif /* TRAFFIC_GENERATOR_HH_ */
