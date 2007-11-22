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
#include <sstream>

#include "base/cprintf.hh"

using namespace std;

// Parameter space for execution address tracing options.  Derive
// from ParamContext so we can override checkParams() function.
class MemTraceParamContext : public ParamContext
{
  public:
    MemTraceParamContext(const string &_iniSection)
	: ParamContext(_iniSection)
    {
    }

    void checkParams();	// defined at bottom of file
    void cleanup();	// defined at bottom of file
};

MemTraceParamContext memTraceParams("memtrace");

Param<string> memtrace_fname(&memTraceParams,
			     "trace",
			     "dump memory traffic <filename>");

Param<int> mem_trace_thread(&memTraceParams,
			    "thread",
			    "which thread to trace", 0);

Param<bool> mem_trace_spec(&memTraceParams,
			   "spec",
			   "trace misspeculated execution", false);

ofstream mem_trace;

void
DynInst::trace_mem(Fault fault, MemCmd cmd, Addr addr,
		   void *p, int nbytes)
{
    stringstream str;
    if (mem_trace.is_open() && (thread_number == mem_trace_thread) &&
	(!spec_mode || mem_trace_spec)) {
	switch (nbytes) {
	  case 1:
	    ccprintf(str, "              %#02x", *(uint8_t *) p);
	    break;
	  case 2:
	    ccprintf(str, "            %#04x", *(uint16_t *) p);
	    break;
	  case 4:
	    ccprintf(str, "        %#08x", *(uint32_t *) p);
	    break;
	  case 8:
	    ccprintf(str, "%#016x", *(uint64_t *) p);
	    break;
	}

	const char *spec_str = (spec_mode) ? "Spec " : "";

	if (cmd == Read)
	    ccprintf(mem_trace,
		     "%d @ %#08x: %sRead  %s from %#08x (fault=%d)\n",
		     curTick, PC, spec_str, str.str(), addr, fault);
	else
	    ccprintf(mem_trace,
		     "%d @ %#08x: %sWrite %s to   %#08x (fault=%d)\n",
		     curTick, PC, spec_str, str.str(), addr, fault);
    }
}



void
MemTraceParamContext::checkParams()
{
    if (mem_trace_fname.isValid()) {
	mem_trace.open(mem_trace_fname, ios::out | ios::trunc);
	if (!mem_trace.is_open())
	    fatal("Unable to open memory trace file");
	ccprintf(mem_trace, "Memory Trace File: %s\n\n", mem_trace_fname);
    }
}


void
MemTraceParamContext::cleanup()
{
    if (mem_trace.is_open())
	mem_trace.close();
}
