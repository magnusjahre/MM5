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

#include <string>

#include "base/callback.hh"
#include "base/hostinfo.hh"
#include "sim/eventq.hh"
#include "sim/param.hh"
#include "sim/sim_events.hh"
#include "sim/sim_exit.hh"
#include "sim/startup.hh"
#include "sim/stats.hh"

using namespace std;

//
// handle termination event
//
void
SimExitEvent::process()
{
    // This event does not autodelete because exitNow may be called,
    // and the function will never be allowed to finish.
    if (theQueue() == &mainEventQueue) {
	string _cause = cause;
	int _code = code;
	delete this;
        exitNow(_cause, _code);
    } else {
	new SimExitEvent(cause, code);
	delete this;
    }
}


const char *
SimExitEvent::description()
{
    return "simulation termination";
}

//
// constructor: automatically schedules at specified time
//
CountedExitEvent::CountedExitEvent(EventQueue *q, const std::string &_cause,
				   Tick _when, int &_downCounter)
    : Event(q, Sim_Exit_Pri),
      cause(_cause),
      downCounter(_downCounter)
{
    // catch stupid mistakes
    assert(downCounter > 0);

    schedule(_when);
}


//
// handle termination event
//
void
CountedExitEvent::process()
{
    if (--downCounter == 0) {
	new SimExitEvent(cause, 0);
    }
}


const char *
CountedExitEvent::description()
{
    return "counted exit";
}

#ifdef CHECK_SWAP_CYCLES
new CheckSwapEvent(&mainEventQueue, CHECK_SWAP_CYCLES);
#endif

void
CheckSwapEvent::process()
{
    /*  Check the amount of free swap space  */
    long swap;

    /*  returns free swap in KBytes  */
    swap = procInfo("/proc/meminfo", "SwapFree:");

    if (swap < 1000)
	ccprintf(cerr, "\a\a\aWarning! Swap space is low (%d)\n", swap);

    if (swap < 100) {
	cerr << "\a\aAborting Simulation! Inadequate swap space!\n\n";
	new SimExitEvent("Lack of swap space");
    }

    schedule(curTick + interval);
}

const char *
CheckSwapEvent::description()
{
    return "check swap";
}

//
// handle progress event: print message and reschedule
//
void
ProgressEvent::process()
{
    DPRINTFN("ProgressEvent\n");
    // reschedule for next interval
    schedule(curTick + interval);
}


const char *
ProgressEvent::description()
{
    return "progress message";
}
