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

#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "base/callback.hh"
#include "base/output.hh"
#include "base/time.hh"
#include "sim/param.hh"
#include "sim/sim_exit.hh"
#include "sim/sim_events.hh"
#include "sim/stat_control.hh"
#include "sim/stats.hh"
#include "sim/root.hh"

using namespace std;

CallbackQueue exitCallbacks;

void
registerExitCallback(Callback *callback)
{
    exitCallbacks.add(callback);
}

void
exitNow(const char *cause, int exit_code)
{
    exitNow(string(cause), exit_code);
}


// Process any exit callbacks
// print stats, uninitialize simulator components, and exit w/ exitcode
void
exitNow(const string &cause, int exit_code)
{
    exitCallbacks.process();
    exitCallbacks.clear();

    cout.flush();

    cerr << "Terminating simulation -- " << cause << endl;
    if (simout.isFile(*outputStream))
	*outputStream << "Terminating simulation -- " << cause << endl;

    ParamContext::cleanupAllContexts();

    // print simulation stats
    Stats::DumpNow();

    Time now(true);

    cerr << "Simulation complete at " << now << endl;
    if (simout.isFile(*outputStream))
	*outputStream << "Simulation complete at " << now << endl;

    // all done!
    exit(exit_code);
}

void
SimExit(Tick when, const char *message)
{
    new SimExitEvent(when, message);
}

SimExitEvent *
exitThisCycle()
{
    //  automagically scheduled for this cycle
    return new SimExitEvent("user requested");
}
