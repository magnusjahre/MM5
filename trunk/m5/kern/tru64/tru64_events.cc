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

#include "cpu/exec_context.hh"
#include "cpu/base.hh"
#include "kern/system_events.hh"
#include "kern/tru64/tru64_events.hh"
#include "kern/tru64/dump_mbuf.hh"
#include "kern/tru64/printf.hh"
#include "mem/functional/memory_control.hh"
#include "targetarch/arguments.hh"
#include "targetarch/isa_traits.hh"

//void SkipFuncEvent::process(ExecContext *xc);

void
BadAddrEvent::process(ExecContext *xc)
{
    // The following gross hack is the equivalent function to the
    // annotation for vmunix::badaddr in:
    // simos/simulation/apps/tcl/osf/tlaser.tcl

    uint64_t a0 = xc->regs.intRegFile[ArgumentReg0];

    if (!TheISA::IsK0Seg(a0) ||
	xc->memctrl->badaddr(TheISA::K0Seg2Phys(a0) & EV5::PAddrImplMask)) {

	DPRINTF(BADADDR, "badaddr arg=%#x bad\n", a0);
	xc->regs.intRegFile[ReturnValueReg] = 0x1;
	SkipFuncEvent::process(xc);
    }
    else
	DPRINTF(BADADDR, "badaddr arg=%#x good\n", a0);
}

void
PrintfEvent::process(ExecContext *xc)
{
    if (DTRACE(Printf)) {
	DebugOut() << curTick << ": " << xc->cpu->name() << ": ";

	AlphaArguments args(xc);
	tru64::Printf(args);
    }
}

void
DebugPrintfEvent::process(ExecContext *xc)
{
    if (DTRACE(DebugPrintf)) {
	if (!raw)
	    DebugOut() << curTick << ": " << xc->cpu->name() << ": ";

	AlphaArguments args(xc);
	tru64::Printf(args);
    }
}

void
DumpMbufEvent::process(ExecContext *xc)
{
    if (DTRACE(DebugPrintf)) {
	AlphaArguments args(xc);
	tru64::DumpMbuf(args);
    }
}
