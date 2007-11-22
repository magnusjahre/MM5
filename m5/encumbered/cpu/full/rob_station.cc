/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

#include "encumbered/cpu/full/create_vector.hh"
#include "encumbered/cpu/full/dep_link.hh"
#include "encumbered/cpu/full/rob_station.hh"
#include "encumbered/cpu/full/writeback.hh"

using namespace std;


//
//  This init() is called during the machine-queue creation
//
void
ROBStation::init(unsigned num_iqueues) {
    odep_list.resize(TheISA::MaxInstDestRegs);
    for (int i = 0; i < TheISA::MaxInstDestRegs; ++i) {
	//	odep_list[i] = new (DepLink *)[num_iqueues];
	odep_list[i].resize(num_iqueues);
    }
}


//
//  This function is called for each new instruction dispatched
//
void
ROBStation::init(DynInst *_inst, InstSeqNum dispatch_seq,
		  unsigned num_iqueues)
{
    inst = _inst;
    eaCompPending = false;

    squashed = false;
    issued = false;
    completed = false;

    recovery_event = 0;

    lsq_entry = 0;
    iq_entry = 0;

    seq = dispatch_seq;

    wb_event = 0;
    delayed_wb_event = 0;

    due_to_complete = 0;
    pred_issue_cycle = 0;
    pred_wb_cycle = 0;

    mem_result = MA_NOT_ISSUED;

    num_outputs = 0;
    for (int i = 0; i < TheISA::MaxInstDestRegs; ++i) {
	onames[i] = -1;

	for (int j = 0; j < num_iqueues; ++j)
	    odep_list[i][j] = 0;
    }

    head_of_chain = false;

    cache_event_ptr = 0;
}


//
//  Mark this ROB entry as belonging to a memory operation
//
void
ROBStation::setMemOp(BaseIQ::iterator lsq)
{
    eaCompPending = true;
    lsq_entry = lsq;
    seq = lsq->seq;
}


void
ROBStation::squash()
{
    tag++;
    squashed = true;

    if (wb_event) {
	wb_event->squash();
	wb_event = 0;
    }

    if (delayed_wb_event) {
	delayed_wb_event->squash();
	delayed_wb_event = 0;
    }

    if (cache_event_ptr) {
	cache_event_ptr->squash();
	cache_event_ptr = 0;
    }

    if (recovery_event) {
	recovery_event->invalidate_branch_entry();
	recovery_event->squash();
	recovery_event = 0;
    }

    inst->squash();
}




#define FLAG_STRING(f)	((f) ? #f " " : "")

void
ROBStation::dump()
{
    Addr seq_PC = inst->PC + sizeof(MachInst);

    cprintf("T%d : %#x `", thread_number, inst->PC);
    cout << inst->staticInst->disassemble(inst->PC);
    cout << "'\n";
    if (inst->isLoad()) {
	cprintf("\tEA: %#x\n", inst->eff_addr);
	cprintf("\tPEA: %#x\n", inst->phys_eff_addr);
    }
    if (inst->Next_PC != seq_PC || inst->Pred_PC != seq_PC)
	cprintf("\tNext_PC: %#08x Pred_PC: %#08x\n",
		inst->Next_PC, inst->Pred_PC);
    cout << "\t";
    if (inst->spec_mode != 0)
	cprintf("spec_mode: %d ", inst->spec_mode);
    cprintf("\t%s%s%s%s%s\n",
	    FLAG_STRING(eaCompPending),
	    FLAG_STRING(inst->recover_inst),
	    FLAG_STRING(issued), FLAG_STRING(completed),
	    FLAG_STRING(squashed));
    cprintf("    inst seq: %d\n", inst->fetch_seq);
    cprintf("    seq: %d\n", seq);
    if (head_of_chain)
	cprintf("    head_chain: %u\n", head_chain);
    cprintf("\tqueue: %d\n", queue_num);
}


void
ROBStation::dump_odeps()
{
    for (int i = 0; i < TheISA::MaxInstDestRegs; ++i)
	dump_odep_list(i);
}

void
ROBStation::dump_odep_list(int i)
{
#if 0
    cprintf("  odep_list[%d]:\n", i);
    if (odep_list[i] != 0) {
	cprintf("    odep #%d\n", i);

	for (int j = 0; j < numIQueues; ++j) {
	    cprintf("      IQ #%d\n", j);
	    for (DepLink *olink = odep_list[i][j]; olink;
		 olink = olink->next_dep) {
		olink->dump();
	    }
	}
    }
    else {
	cout << "\t<empty>\n";
    }
#endif
}
