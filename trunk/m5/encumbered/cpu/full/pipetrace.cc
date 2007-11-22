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
#include <iostream>
#include <fstream>
#include <iomanip>

#include "base/misc.hh"
#include "base/output.hh"
#include "encumbered/cpu/full/pipetrace.hh"
#include "mem/mem_req.hh"
#include "sim/builder.hh"

using namespace std;

const char * const PipeTrace::stageNames[] = {"IF", "DA", "EX", "WB", "CT"};

PipeTrace::PipeTrace(const std::string &name, const std::string &filename,
		     std::string &_range, bool exit_mode, bool statsValid,
		     std::vector<std::string> _stats)
    : SimObject(name), range(_range), exit_when_done(exit_mode)
{
    outfile = simout.find(filename);
    active = false;
    useStats = statsValid;

    if (!_range.length() || !range.valid())
	panic("pipetrace: Can't parse range spec");

    //  Save the statistic names for later...
    if (useStats && _stats.size())
	for (int i = 0; i < _stats.size(); ++i)
	    stat_names.push_back(_stats[i]);
}


void
PipeTrace::regStats()
{
#if 0
    /**
     * @todo make this work with the new stats package
     */
    if (useStats && stat_names.size()) {
	for (int i = 0; i < stat_names.size(); ++i) {
	    const char *s = stat_names[i].c_str();
	    stat_stat_t *p = stat_find_stat(sdb, s);
	    if (p)
		statistics.push_back(p);
	    else {
		statistics.push_back(0);  // place-holder
		warn("pipetrace: can't find statistic %s", s);
	    }
	}
    }
#endif
}


//
//  Destructor
//
PipeTrace::~PipeTrace()
{
}


//
//  Call this every cycle...  Handles closing of output stream
//
bool
PipeTrace::newCycle(Tick cycle)
{
    bool exit_status = false;

    //  update the active flag
    if (cycle == range) {
	active = true;

	*outfile << "@ " << dec << cycle << "\n";

#if 0
	/**
	 * @todo make this work with the new stats package
	 */
	if (!statistics.empty()) {
	    for (int i = 0; i < statistics.size(); ++i) {
		// skip the "place-holder" entries
		if (statistics[i]) {
		    double val = stat_value_as_double(sdb, statistics[i]);
		    *outfile << "<" << stat_names[i] << ">\t" << val << endl;
		}
	    }
	}
#endif
    } else {
	active = false;

	if (cycle > range) {
	    delete outfile;
	    outfile = NULL;

	    if (exit_when_done)
		exit_status = true;
	}
    }

    return exit_status;
}


void
PipeTrace::newInst(DynInst *inst)
{
    if (!active)
	return;

    string s;

    *outfile << "+ " << dec << inst->fetch_seq
	     << hex << " 0x" << inst->PC
	     << " 0x";

    if (inst->eff_addr == MemReq::inval_addr)
	*outfile << "0000000000000000";
    else
	*outfile << std::setfill('0') << std::setw(16) << inst->eff_addr;

    *outfile << " " << dec;

    inst->dump(s);
    *outfile << s
	     << "  [T" << dec << inst->thread_number << ", CP#"
	     << inst->correctPathSeq << "]"
	     << endl;
}


void
PipeTrace::moveInst(DynInst *inst, stageID new_stage, unsigned events,
		    unsigned miss_latency, unsigned longest_event)
{
    if (!active)
	return;

    //  make sure we have a sensible value for miss latency...
    unsigned lat = events ? miss_latency : 0;

    *outfile << "* " << dec << inst->fetch_seq << " " << stageNames[new_stage];

    //	outfile->setf(ios::hex);
    *outfile << std::setfill('0')
	     << " 0x" << hex << std::setw(4) << events
	     << " " << dec << lat
	     << " 0x" << hex << std::setw(4) << longest_event
	     << "  [T" << dec << inst->thread_number << ", CP#"
	     << inst->correctPathSeq << "]"
	     << endl;
    outfile->width(0);
}


void
PipeTrace::deleteInst(DynInst *inst)
{
    if (!active)
	return;

    *outfile << "- " << dec << inst->fetch_seq
	     << "  [T"
	     << dec << inst->thread_number << ", CP#"
	     << inst->correctPathSeq << "]"
	     << endl;
}


//////////////////////////////////////////////////////////////////////////////
//   Interface to INI file mechanism
//////////////////////////////////////////////////////////////////////////////


BEGIN_DECLARE_SIM_OBJECT_PARAMS(PipeTrace)

    Param<string> file;
    Param<string> range;
    Param<bool> exit_when_done;
    VectorParam<string> statistics;

END_DECLARE_SIM_OBJECT_PARAMS(PipeTrace)


BEGIN_INIT_SIM_OBJECT_PARAMS(PipeTrace)

    INIT_PARAM(file, "output file name"),
    INIT_PARAM(range, "range of cycles to trace"),
    INIT_PARAM(exit_when_done,
	       "terminate simulation when done collecting ptrace data"),
    INIT_PARAM(statistics, "stats to include in pipe-trace")

END_INIT_SIM_OBJECT_PARAMS(PipeTrace)


CREATE_SIM_OBJECT(PipeTrace)
{
    vector<string> stats = statistics;

    PipeTrace *rv = new PipeTrace(getInstanceName(), file,
				  range, exit_when_done,
				  stats.size() > 0, stats);

    return rv;
}

REGISTER_SIM_OBJECT("PipeTrace", PipeTrace)
