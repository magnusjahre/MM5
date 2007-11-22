/*
 * Copyright (c) 2003, 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

#include "base/statistics.hh"
#include "base/trace.hh"
#include "cpu/base.hh"
#include "cpu/sampler/sampler.hh"
#include "sim/builder.hh"
#include "sim/serialize.hh"
#include "sim/sim_events.hh"

using namespace std;

Sampler *SampCPU;

class CpuSwitchEvent : public Event
{
  private:
    
    Sampler *sampler;
    
  public:
    
    CpuSwitchEvent(Tick _when, Sampler *sampler);
    CpuSwitchEvent(Sampler *sampler);
    virtual ~CpuSwitchEvent() {}
    
    void process();

    const char *description() { return "cpu switch event"; }
    void serialize(std::ostream &os);
    void unserialize(Checkpoint *cp, const std::string &section);
    static Serializable *createForUnserialize(Checkpoint *cp, 
					       const std::string &section);
};


CpuSwitchEvent::CpuSwitchEvent(Tick when, Sampler *_sampler)
    : Event(&mainEventQueue, CPU_Switch_Pri), sampler(_sampler)
{
    DPRINTF(Sampler, "New switch event curTick=%d when=%d\n", curTick, when);
    setFlags(AutoDelete);
    schedule(when);
}

//non-scheduling version for createForUnserialized()
CpuSwitchEvent::CpuSwitchEvent(Sampler *_sampler)
    : Event(&mainEventQueue), sampler(_sampler)
{
    setFlags(AutoDelete);
}

void
CpuSwitchEvent::process()
{
    sampler->switchCPUs();
}

void
CpuSwitchEvent::serialize(ostream &os)
{
    paramOut(os, "type", string("CpuSwitchEvent"));
    Event::serialize(os);
    SERIALIZE_OBJPTR(sampler);
}

void
CpuSwitchEvent::unserialize(Checkpoint *cp, const string &section)
{
    Event::unserialize(cp,section);
}

Serializable *
CpuSwitchEvent::createForUnserialize(Checkpoint *cp, 
				     const string &section)
{
    Sampler *sampler;
    UNSERIALIZE_OBJPTR(sampler);
    return new CpuSwitchEvent(sampler);
}

REGISTER_SERIALIZEABLE("CpuSwitchEvent", CpuSwitchEvent)

Sampler::Sampler(const std::string &_name,
		 vector<BaseCPU *> &_phase0_cpus, 
		 vector<BaseCPU *> &_phase1_cpus, 
		 vector<Tick> &_periods)
    : SimObject(_name), phase0_cpus(_phase0_cpus), 
      phase1_cpus(_phase1_cpus),periods(_periods), phase(0)
{
    assert(curTick == 0);

    size = phase0_cpus.size();

    SampCPU = this;
}

Sampler::~Sampler()
{
}

void
Sampler::init()
{
    // set up only the first CPU to run
    for(int i = 0; i < size; i++) {
	phase0_cpus[i]->registerExecContexts();
    }
}

void
Sampler::startup()
{
    new CpuSwitchEvent(curTick + periods[0], this);

    /**
     * @todo this is really a hack.  I saw in some instances that the
     * simulator would not actually exit when it was done sampling, so
     * put this catchall in here.  Return a failure code since we want
     * to notice this situation.
     */
    new SimExitEvent(curTick + periods[0] + periods[1] + periods[0],
		     "We should not have gotten here!  Sampler exit lost",
		     1);
}

/**
 * @todo This is just a quick hack that makes the simulator exit after
 * the last sampling phase, but the intent is to have it repeat the
 * samples.  In order to do this, we need to make the sampling time
 * be relative to the inital curTick on an unserialization, we should
 * also put in a periods parameter to prevent it from running forever.
 */
void
Sampler::switchCPUs()
{
    DPRINTF(Sampler, "switching CPUs");
    if (phase == 1) {
	//We can't switch back from detailed yet, just finish sampling
	//Temporary stop-gap
	new SimExitEvent(curTick, "Done Sampling\n");
    } else {
	//Reset count of cpus who finished switching
	numFinished = 0;
	//Depending on phase, call switchout on the cpus now
	if (phase == 0) {
	    for(int i = 0; i < size; i++) {
		phase0_cpus[i]->switchOut(this);
	    }
	}
	else {
	    //Switch Back to original phase now
	    for(int i = 0; i < size; i++) {
		phase1_cpus[i]->switchOut(this);
	    }
	}
    }
}

void
Sampler::signalSwitched()
{    
    if (++numFinished == size) {
	DPRINTF(Sampler, "Done switching CPUs\n");

	//Signal each cpu to takeover
	if (phase == 0) {
	    for(int i = 0; i < size; i++) {
		phase1_cpus[i]->takeOverFrom(phase0_cpus[i]);
	    }
	    phase = 1;
	    new CpuSwitchEvent(curTick + periods[1], this);
	}
	else {
	    for(int i = 0; i < size; i++) {
		phase0_cpus[i]->takeOverFrom(phase1_cpus[i]);
	    }
	    phase = 0;
	    new CpuSwitchEvent(curTick + periods[0], this);
	}

	Stats::reset();
    }
}

void
Sampler::serialize(ostream &os)
{
    SERIALIZE_SCALAR(phase);
}

void
Sampler::unserialize(Checkpoint *cp, const string &section)
{
    UNSERIALIZE_SCALAR(phase);
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Sampler)

    SimObjectVectorParam<BaseCPU *> phase0_cpus;
    SimObjectVectorParam<BaseCPU *> phase1_cpus;
    VectorParam<Tick> periods;

END_DECLARE_SIM_OBJECT_PARAMS(Sampler)

BEGIN_INIT_SIM_OBJECT_PARAMS(Sampler)

    INIT_PARAM(phase0_cpus, "vector of actual CPUs to run in phase 0"),
    INIT_PARAM(phase1_cpus, "vector of actual CPUs to run in phase 1"),
    INIT_PARAM(periods, "vector of per-phase sample periods")

END_INIT_SIM_OBJECT_PARAMS(Sampler)


CREATE_SIM_OBJECT(Sampler)
{
    if (phase0_cpus.size() != phase1_cpus.size())
	panic("'phase0_cpus' and 'phase1_cpus' vector lengths must match");

    if (periods.size() != 2)
	panic("'periods' vector lengths must be two");

    return new Sampler(getInstanceName(), phase0_cpus, phase1_cpus,
                           periods);
}

REGISTER_SIM_OBJECT("Sampler", Sampler)
