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

#include <fstream>
#include <iostream>
#include <string>

#include "base/callback.hh"
#include "base/match.hh"
#include "base/misc.hh"
#include "base/output.hh"
#include "base/trace.hh"
#include "sim/eventq.hh"
#include "sim/param.hh"
#include "sim/sim_exit.hh"
#include "sim/root.hh"

using namespace std;

namespace Trace
{
    extern ObjectMatch ignore;
    extern ostream *dprintf_stream;
}

class TraceDumpCallback : public Callback
{
  public:
    virtual void process();
};

void
TraceDumpCallback::process()
{
    Trace::theLog.dump(DebugOut());
}

/////////////////////////////////////////////////////////////////////
// Parameter space for execution address tracing options.  Derive
// from ParamContext so we can override checkParams() function.
class TraceParamContext : public ParamContext
{
  public:
    TraceParamContext(const string &_iniSection)
	: ParamContext(_iniSection, TraceInitPhase) {}

    void checkParams();

  private:

    void setFlags();

    friend class EnableTraceEvent;
};

TraceParamContext traceParams("trace");


SimpleEnumVectorParam<Trace::Flags>
trace_flags(&traceParams, "flags",
	    "categories to be traced",
	    Trace::flagStrings, Trace::numFlagStrings);

Param<Tick> trace_start(&traceParams, "start", "cycle to start tracing", 0);

Param<int> trace_bufsize(&traceParams, "bufsize",
			 "trace circular buffer size (default: send to file)");

Param<string> trace_file(&traceParams, "file", "file for trace output");

Param<bool> trace_dump_on_exit(&traceParams, "dump_on_exit",
			       "dump trace buffer on exit", false);

VectorParam<string> trace_ignore(&traceParams, "ignore",
				 "name strings to ignore", vector<string>());

void
TraceParamContext::setFlags()
{
    for (int i = 0; i < trace_flags.size(); ++i) {
	int idx = trace_flags[i];

	assert(idx >= 0);

	if (idx < Trace::NumFlags) {
	    Trace::flags[idx] = true;
	}
	else {
	    idx -= Trace::NumFlags;
	    assert(idx < Trace::NumCompoundFlags);

	    const Trace::Flags *flagVec = Trace::compoundFlags[idx];

	    for (int j = 0; flagVec[j] != -1; ++j) {
		assert(flagVec[j] < Trace::NumFlags);
		Trace::flags[flagVec[j]] = true;
	    }
	}
    }
}

//
// This event allows us to schedule the setting of the trace flags
// at some future cycle (to support the trace:start parameter).
//
class EnableTraceEvent : public Event
{
  private:
    TraceParamContext *ctx;

  public:
    EnableTraceEvent(TraceParamContext *_ctx, Tick when)
	: Event(&mainEventQueue), ctx(_ctx)
    {
	setFlags(AutoDelete);
	schedule(when);
    }

    virtual void process()
    {
	ctx->setFlags();
    }
};

// check execute options
void
TraceParamContext::checkParams()
{
    using namespace Trace;

#if TRACING_ON
    if (trace_start == 0) {
	// set trace flags immediately
	setFlags();
    }
    else {
	// trace flags start out as 0, set them later at specified cycle
	new EnableTraceEvent(this, trace_start);
    }

    if ((int)trace_bufsize > 0)
	theLog.init(trace_bufsize);

    if (trace_dump_on_exit) {
	registerExitCallback(new TraceDumpCallback);
    }

    dprintf_stream = simout.find(trace_file);

    ignore.setExpression(trace_ignore);

#else
    // Tracing not compiled in... fail if any tracing options are set.
    if (trace_flags.size() > 0) {
	fatal("Trace flags set, but tracing is not compiled in!\n"
	      "Recompile with -DDEBUG or -DTRACING_ON=1.\n");
    }
#endif
}
