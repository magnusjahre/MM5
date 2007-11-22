/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005
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

/*
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 *
 * Copyright (C) 1997, 1998 by Todd M. Austin
 *
 * This source file is distributed "as is" in the hope that it will be
 * useful.  The tool set comes with no warranty, and no author or
 * distributor accepts any responsibility for the consequences of its
 * use.
 *
 * Everyone is granted permission to copy, modify and redistribute
 * this tool set under the following conditions:
 *
 *    This source code is distributed for non-commercial use only.
 *    Please contact the maintainer for restrictions applying to
 *    commercial use.
 *
 *    Permission is granted to anyone to make or distribute copies
 *    of this source code, either as received or modified, in any
 *    medium, provided that all copyright notices, permission and
 *    nonwarranty notices are preserved, and that the distributor
 *    grants the recipient permission for further redistribution as
 *    permitted by this document.
 *
 *    Permission is granted to distribute this file in compiled
 *    or executable form under the same conditions that apply for
 *    source code, provided that either:
 *
 *    A. it is accompanied by the corresponding machine-readable
 *       source code,
 *    B. it is accompanied by a written offer, with no time limit,
 *       to give anyone a machine-readable copy of the corresponding
 *       source code in return for reimbursement of the cost of
 *       distribution.  This written offer must permit verbatim
 *       duplication by anyone, or
 *    C. it is distributed by someone who received only the
 *       executable form, and is accompanied by a copy of the
 *       written offer of source code that they received concurrently.
 *
 * In other words, you are welcome to use, share and improve this
 * source file.  You are forbidden to forbid anyone else to use, share
 * and improve what you give them.
 */

#include <math.h>
#include <algorithm>

#include "base/cprintf.hh"
#include "cpu/exec_context.hh"
#include "cpu/exetrace.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "sim/sim_events.hh"

#if FULL_SYSTEM
#include "base/remote_gdb.hh"
#include "sim/system.hh"
#endif

using namespace std;

Fault
FullCPU::execute_instruction(DynInst *fetched_inst, int thread_number)
{
    SpecExecContext *xc = fetched_inst->xc;
    //    const RegFile &gdbDebugRegs = xc->regs;
    Fault fault = No_Fault;

    /* maintain $r0 semantics (in spec and non-spec space) */
    xc->regs.intRegFile[ZeroReg] = 0;
    xc->specIntRegFile[ZeroReg] = 0;
#ifdef TARGET_ALPHA
    xc->regs.floatRegFile.d[ZeroReg] = 0.0;
    xc->specFloatRegFile.d[ZeroReg] = 0.0;
#endif				/* TARGET_ALPHA */

#if FULL_SYSTEM
    MachInst inst = fetched_inst->staticInst->machInst;
    xc->setInst(inst);
#endif // FULL_SYSTEM


    /* count functionally executed instructions for EIO consistency check */
    if (!xc->spec_mode) {
	xc->func_exe_inst++;
#if FULL_SYSTEM
	assert(((xc->regs.pc & 1) == 1) ==
	       xc->regs.pal_shadow);
#endif
    }

    // Default value for next_PC.  We need to set this in case the
    // instruction does not.
    xc->regs.npc = xc->regs.pc + sizeof(MachInst);

    // perform functional execution
    if (!(fetched_inst->isNonSpeculative() && xc->spec_mode))
	fault = fetched_inst->execute();

#if FULL_SYSTEM
    if (!xc->misspeculating() && xc->fnbin)
	xc->execute(fetched_inst->staticInst.get());
#endif

    /* control operation sets "next_PC" to next non-spec instruction
     * as necessary */

    if (fetched_inst->isNop())
	exe_nop[thread_number]++;

    fetched_inst->Next_PC = xc->regs.npc;

    if (fault != No_Fault && xc->spec_mode) {
	// Misspeculating... just ignore faults
	fault = No_Fault;
    }

#if FULL_SYSTEM
    // For full-system, get set up to invoke trap handler on a fault.
    // We do this before the instr. trace in case we need to add any
    // trace information here.
    if (fault != No_Fault) {
	xc->ev5_trap(fault);
    }
#endif // FULL_SYSTEM

    /***********************************************
     *  Trace the instruction stream               *
     ***********************************************/
    if (fetched_inst->trace_data) {
	fetched_inst->trace_data->setCPSeq(fetched_inst->correctPathSeq);
	fetched_inst->trace_data->finalize();
    }

#if !FULL_SYSTEM
    // For non-full-system, we should never get a fault on a
    // non-speculative path.  Do this after the trace so we get to see
    // the trace record for the faulting instruction.
    if (fault != No_Fault) {
	panic("non-speculative fault (%d) @ PC %#08x in thread %d",
	      fault, fetched_inst->PC, thread_number);
    }
#endif // !FULL_SYSTEM

    return fault;
}
