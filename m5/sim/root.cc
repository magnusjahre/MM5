/*
 * Copyright (c) 2002, 2003, 2004, 2005
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

#include <cstring>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "base/misc.hh"
#include "base/output.hh"
#include "sim/builder.hh"
#include "sim/host.hh"
#include "sim/sim_events.hh"
#include "sim/sim_object.hh"
#include "sim/root.hh"

using namespace std;

Tick curTick = 0;
ostream *outputStream;
ostream *configStream;

/// The simulated frequency of curTick. (This is only here for a short time)
Tick ticksPerSecond;

namespace Clock {
/// The simulated frequency of curTick. (In ticks per second)
Tick Frequency;

namespace Float {
double s;
double ms;
double us;
double ns;
double ps;

double Hz;
double kHz;
double MHz;
double GHZ;
/* namespace Float */ }

namespace Int {
Tick s;
Tick ms;
Tick us;
Tick ns;
Tick ps;
/* namespace Float */ }

/* namespace Clock */ }


// Dummy Object
class Root : public SimObject
{
  private:
    Tick max_tick;
    Tick progress_interval;

  public:
    Root(const std::string &name, Tick maxtick, Tick pi)
        : SimObject(name), max_tick(maxtick), progress_interval(pi)
    {}

    virtual void startup();
};

void
Root::startup()
{
    if (max_tick != 0)
	new SimExitEvent(curTick + max_tick, "reached maximum cycle count");

    if (progress_interval != 0)
	new ProgressEvent(&mainEventQueue, progress_interval);
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Root)

    Param<Tick> clock;
    Param<Tick> max_tick;
    Param<Tick> progress_interval;
    Param<string> output_file;

END_DECLARE_SIM_OBJECT_PARAMS(Root)

BEGIN_INIT_SIM_OBJECT_PARAMS(Root)

    INIT_PARAM(clock, "tick frequency"),
    INIT_PARAM(max_tick, "maximum simulation time"),
    INIT_PARAM(progress_interval, "print a progress message"),
    INIT_PARAM(output_file, "file to dump simulator output to")

END_INIT_SIM_OBJECT_PARAMS(Root)

CREATE_SIM_OBJECT(Root)
{
    static bool created = false;
    if (created)
	panic("only one root object allowed!");

    created = true;
    
    outputStream = simout.find(output_file);
    Root *root = new Root(getInstanceName(), max_tick, progress_interval);

    using namespace Clock;
    Frequency = clock;
    Float::s = static_cast<double>(Frequency);
    Float::ms = Float::s / 1.0e3;
    Float::us = Float::s / 1.0e6;
    Float::ns = Float::s / 1.0e9;
    Float::ps = Float::s / 1.0e12;

    Float::Hz  = 1.0 / Float::s;
    Float::kHz = 1.0 / Float::ms;
    Float::MHz = 1.0 / Float::us;
    Float::GHZ = 1.0 / Float::ns;

    Int::s  = Frequency;
    Int::ms = Int::s / 1000;
    Int::us = Int::ms / 1000;
    Int::ns = Int::us / 1000;
    Int::ps = Int::ns / 1000;

    return root;
}

REGISTER_SIM_OBJECT("Root", Root)
