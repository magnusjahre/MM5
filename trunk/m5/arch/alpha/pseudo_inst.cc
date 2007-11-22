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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>

#include <string>

#include "arch/alpha/pseudo_inst.hh"
#include "arch/alpha/vtophys.hh"
#include "cpu/base.hh"
#include "cpu/sampler/sampler.hh"
#include "cpu/exec_context.hh"
#include "kern/kernel_stats.hh"
#include "sim/param.hh"
#include "sim/serialize.hh"
#include "sim/sim_exit.hh"
#include "sim/stat_control.hh"
#include "sim/stats.hh"
#include "sim/system.hh"
#include "sim/debug.hh"

using namespace std;

extern Sampler *SampCPU;

using namespace Stats;

namespace AlphaPseudo
{
    bool doStatisticsInsts;
    bool doCheckpointInsts;
    bool doQuiesce;

    void
    arm(ExecContext *xc)
    {
	xc->kernelStats->arm();
    }

    void
    quiesce(ExecContext *xc)
    {
	if (!doQuiesce)
	    return;

	xc->suspend();
	xc->kernelStats->quiesce();
    }

    void
    ivlb(ExecContext *xc)
    {
	xc->kernelStats->ivlb();
    }

    void
    ivle(ExecContext *xc)
    {
    }

    void
    m5exit_old(ExecContext *xc)
    {
	SimExit(curTick, "m5_exit_old instruction encountered");
    }

    void
    m5exit(ExecContext *xc)
    {
	Tick delay = xc->regs.intRegFile[16];
	Tick when = curTick + delay * Clock::Int::ns;
	SimExit(when, "m5_exit instruction encountered");
    }

    void
    resetstats(ExecContext *xc)
    {
	if (!doStatisticsInsts)
	    return;

	Tick delay = xc->regs.intRegFile[16];
	Tick period = xc->regs.intRegFile[17];

	Tick when = curTick + delay * Clock::Int::ns;
	Tick repeat = period * Clock::Int::ns;

	using namespace Stats;
	SetupEvent(Reset, when, repeat);
    }

    void
    dumpstats(ExecContext *xc)
    {
	if (!doStatisticsInsts)
	    return;

	Tick delay = xc->regs.intRegFile[16];
	Tick period = xc->regs.intRegFile[17];

	Tick when = curTick + delay * Clock::Int::ns;
	Tick repeat = period * Clock::Int::ns;

	using namespace Stats;
	SetupEvent(Dump, when, repeat);
    }

    void
    dumpresetstats(ExecContext *xc)
    {
	if (!doStatisticsInsts)
	    return;

	Tick delay = xc->regs.intRegFile[16];
	Tick period = xc->regs.intRegFile[17];

	Tick when = curTick + delay * Clock::Int::ns;
	Tick repeat = period * Clock::Int::ns;

	using namespace Stats;
	SetupEvent(Dump|Reset, when, repeat);
    }

    void
    m5checkpoint(ExecContext *xc)
    {
	if (!doCheckpointInsts)
	    return;

	Tick delay = xc->regs.intRegFile[16];
	Tick period = xc->regs.intRegFile[17];

	Tick when = curTick + delay * Clock::Int::ns;
	Tick repeat = period * Clock::Int::ns;

	Checkpoint::setup(when, repeat);
    }

    void
    readfile(ExecContext *xc)
    {
       	const string &file = xc->cpu->system->params->readfile;
	if (file.empty()) {
	    xc->regs.intRegFile[0] = ULL(0);
	    return;
	}

	Addr vaddr = xc->regs.intRegFile[16];
	uint64_t len = xc->regs.intRegFile[17];
	uint64_t offset = xc->regs.intRegFile[18];
	uint64_t result = 0;

	int fd = ::open(file.c_str(), O_RDONLY, 0);
	if (fd < 0)
	    panic("could not open file %s\n", file);

        if (::lseek(fd, offset, SEEK_SET) < 0)
            panic("could not seek: %s", strerror(errno));

	char *buf = new char[len];
	char *p = buf;
	while (len > 0) {
            int bytes = ::read(fd, p, len);
	    if (bytes <= 0)
		break;

	    p += bytes;
	    result += bytes;
	    len -= bytes;
	}

	close(fd);
	CopyIn(xc, vaddr, buf, result);
	delete [] buf;
	xc->regs.intRegFile[0] = result;
    }

    class Context : public ParamContext
    {
      public:
	Context(const string &section) : ParamContext(section) {}
	void checkParams();
    };

    Context context("pseudo_inst");

    Param<bool> __quiesce(&context, "quiesce",
			  "enable quiesce instructions",
			  true);
    Param<bool> __statistics(&context, "statistics",
			     "enable statistics pseudo instructions",
			     true);
    Param<bool> __checkpoint(&context, "checkpoint", 
			     "enable checkpoint pseudo instructions",
			     true);

    void
    Context::checkParams()
    {
	doQuiesce = __quiesce;
	doStatisticsInsts = __statistics;
	doCheckpointInsts = __checkpoint;
    }

    void debugbreak(ExecContext *xc)
    {
        debug_break();
    }

    void switchcpu(ExecContext *xc)
    {
        if (SampCPU)
            SampCPU->switchCPUs();
    }
}
