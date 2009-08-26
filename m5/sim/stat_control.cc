/*
 * Copyright (c) 2004, 2005
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

// This file will contain default statistics for the simulator that
// don't really belong to a specific simulator object

#include <fstream>
#include <iostream>
#include <list>

#include "base/callback.hh"
#include "base/hostinfo.hh"
#include "base/statistics.hh"
#include "base/str.hh"
#include "base/time.hh"
#include "base/stats/output.hh"
#include "cpu/base.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"
#include "sim/stat_control.hh"
#include "sim/root.hh"

using namespace std;

Stats::Formula hostInstRate;
Stats::Formula hostTickRate;
Stats::Value hostMemory;
Stats::Value hostSeconds;

Stats::Value simTicks;
Stats::Value simInsts;
Stats::Value simFreq;
Stats::Formula simSeconds;

namespace Stats {

Time statTime(true);
Tick startTick;

class SimTicksReset : public Callback
{
  public:
    void process()
    {
	statTime.set();
	startTick = curTick;
    }
};

double
statElapsedTime()
{
    Time now(true);
    Time elapsed = now - statTime;
    return elapsed();
}

Tick
statElapsedTicks()
{
    return curTick - startTick;
}

SimTicksReset simTicksReset;

void
InitSimStats()
{
    simInsts
	.functor(BaseCPU::numSimulatedInstructions)
	.name("sim_insts")
	.desc("Number of instructions simulated")
	.precision(0)
	.prereq(simInsts)
	;

    simSeconds
	.name("sim_seconds")
	.desc("Number of seconds simulated")
	;

    simFreq
	.scalar(Clock::Frequency)
	.name("sim_freq")
	.desc("Frequency of simulated ticks")
	;

    simTicks
	.functor(statElapsedTicks)
	.name("sim_ticks")
	.desc("Number of ticks simulated")
	;

    hostInstRate
	.name("host_inst_rate")
	.desc("Simulator instruction rate (inst/s)")
	.precision(0)
	.prereq(simInsts)
	;

    hostMemory
	.functor(memUsage)
	.name("host_mem_usage")
	.desc("Number of bytes of host memory used")
	.prereq(hostMemory)
	;

    hostSeconds
	.functor(statElapsedTime)
	.name("host_seconds")
	.desc("Real time elapsed on the host")
	.precision(2)
	;

    hostTickRate
	.name("host_tick_rate")
	.desc("Simulator tick rate (ticks/s)")
	.precision(0)
	;

    simSeconds = simTicks / simFreq;
    hostInstRate = simInsts / hostSeconds;
    hostTickRate = simTicks / hostSeconds;

    registerResetCallback(&simTicksReset);
}

class StatEvent : public Event
{
  protected:
    int flags;
    Tick repeat;

  public:
    StatEvent(int _flags, Tick _when, Tick _repeat);
    virtual void process();
    virtual const char *description();
};

StatEvent::StatEvent(int _flags, Tick _when, Tick _repeat)
    : Event(&mainEventQueue, Stat_Event_Pri),
      flags(_flags), repeat(_repeat)
{
    setFlags(AutoDelete);
    schedule(_when);
}

const char *
StatEvent::description()
{
    return "Statistics dump and/or reset";
}

void
StatEvent::process()
{
	if (flags & Stats::Dump) DumpNow();

    if (flags & Stats::Reset) reset();

    if (repeat) schedule(curTick + repeat);
}

list<Output *> OutputList;

void
DumpNow()
{
	list<Output *>::iterator i = OutputList.begin();
	list<Output *>::iterator end = OutputList.end();
	for (; i != end; ++i) {
		Output *output = *i;
		if (!output->valid())
			continue;

		output->output();
	}
}

void
SetupEvent(int flags, Tick when, Tick repeat)
{
    new StatEvent(flags, when, repeat);
}

/* namespace Stats */ }

extern "C" void
debugDumpStats()
{
    Stats::DumpNow();
}

