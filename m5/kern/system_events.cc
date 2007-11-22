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

#include "encumbered/cpu/full/cpu.hh"
#include "kern/kernel_stats.hh"

void
SkipFuncEvent::process(ExecContext *xc)
{
    Addr newpc = xc->regs.intRegFile[ReturnAddressReg];

    DPRINTF(PCEvent, "skipping %s: pc=%x, newpc=%x\n", description,
	    xc->regs.pc, newpc);

    xc->regs.pc = newpc;
    xc->regs.npc = xc->regs.pc + sizeof(MachInst);

    BranchPred *bp = xc->cpu->getBranchPred();
    if (bp != NULL) {
	bp->popRAS(xc->thread_num);
    }
}


FnEvent::FnEvent(PCEventQueue *q, const std::string &desc, Addr addr,
		 Stats::MainBin *bin)
    : PCEvent(q, desc, addr), _name(desc), mybin(bin)
{
}

void
FnEvent::process(ExecContext *xc) 
{
    if (xc->misspeculating())
        return;

    xc->system->kernelBinning->call(xc, mybin);
}

void
IdleStartEvent::process(ExecContext *xc)
{
    xc->kernelStats->setIdleProcess(xc->regs.ipr[AlphaISA::IPR_PALtemp23]);
    remove();
}

void
InterruptStartEvent::process(ExecContext *xc)
{
    xc->kernelStats->mode(Kernel::interrupt);
}

void
InterruptEndEvent::process(ExecContext *xc)
{
    // We go back to kernel, if we are user, inside the rti
    // pal code we will get switched to user because of the ICM write
    xc->kernelStats->mode(Kernel::kernel);
}
