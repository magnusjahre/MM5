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

#include <iostream>

#include "base/cprintf.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/create_vector.hh"
#include "encumbered/cpu/full/iq/iq_station.hh"

#define FLAG_STRING(f)	((f) ? #f " " : "")

using namespace std;

void
IQStation::dump(int length)
{
    Addr seq_PC = inst->PC + sizeof(MachInst);

    if (length == 0)
	return;

    inst->dump();

    if (inst->Next_PC != seq_PC || inst->Pred_PC != seq_PC)
	cprintf("\tNext_PC: %#x Pred_PC: %#x\n", inst->Next_PC, inst->Pred_PC);
    cout << "\t";
    if (inst->spec_mode != 0)
	cprintf("spec_mode: %d ", inst->spec_mode);
    cprintf("FLAGS: %s%s%s%s%s%s",
	    FLAG_STRING(in_LSQ), FLAG_STRING(ea_comp),
	    FLAG_STRING(inst->recover_inst),
	    FLAG_STRING(queued), FLAG_STRING(squashed),
	    //	    FLAG_STRING(blocked)
	    "");
    cprintf("\n\tPhysical addr: %#x\n", inst->eff_addr);
    if (ops_ready())
	cout << " ops-rdy";

    cprintf("\n\tseq: %d\n", seq);

    if (length == 1)
	return;

    cprintf("\tSegment #%d\n"
	    "\tpred_ready_time = %d\n"
	    "\tseg0_entry_time = %d\n"
	    "\tst_zero_time    = %d\n"
	    "\tfirst_op_ready  = %d\n"
	    "\tpred_last_op_index = %d\n"
	    "\tlr_prediction      = %d\n",
	    segment_number, pred_ready_time, seg0_entry_time, st_zero_time,
	    first_op_ready, pred_last_op_index, lr_prediction);
}

void
IQStation::iq_segmented_dump(int length)
{
    dump(length);

    cout << "\tfollows chain: ";
    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	if (!idep_ready[i]) {
	    if (idep_info[i].chained)
		cprintf("%d ", idep_info[i].follows_chain);
	    else
		cout << "ST ";
	}
    }

    cout << "\ttimer: ";
    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i)
	if (!idep_ready[i])
	    cprintf("%u ", idep_info[i].delay);
    cout << "\n";
    if (head_of_chain)
	cprintf("\thead of chain: %d\n", head_chain);
}

void
IQStation::iq_segmented_short_dump()
{
    cprintf("#%d %08x ", seq, inst->PC);

    if (head_of_chain)
	cprintf("\tH: %3d  ", head_chain);
    else
	cout << "\t        ";

    cout <<"C: ";
    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	if (idep_info[i].chained)
	    cprintf("%3d  ", idep_info[i].follows_chain);
	else
	    cout << "ST   ";
    }

    cout << "D: ";
    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i)
	cprintf("%2u ", idep_info[i].delay);

    cprintf("S: %3u", dest_seg);

    if (ops_ready())
	cout << "  *READY*";

    if (hm_prediction == MA_CACHE_MISS)
	cout << "  ==> Pred MISS <==";
    else if (hm_prediction == MA_HIT)
	cout << "  ==> Pred HIT <==";
    else if (inst->isLoad())
	cout <<"  ==> Load Inst <==";

    cout << "\n";
}

