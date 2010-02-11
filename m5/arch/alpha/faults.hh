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

#ifndef __FAULTS_HH__
#define __FAULTS_HH__

enum Fault {
    No_Fault,
    Reset_Fault,		// processor reset
    Machine_Check_Fault,	// machine check (also internal S/W fault)
    Arithmetic_Fault,		// FP exception
    Interrupt_Fault,		// external interrupt
    Ndtb_Miss_Fault,		// DTB miss
    Pdtb_Miss_Fault,		// nested DTB miss
    Alignment_Fault,		// unaligned access
    DTB_Fault_Fault,		// DTB page fault
    DTB_Acv_Fault,		// DTB access violation
    ITB_Miss_Fault,		// ITB miss
    ITB_Fault_Fault,		// ITB page fault
    ITB_Acv_Fault,		// ITB access violation
    Unimplemented_Opcode_Fault,	// invalid/unimplemented instruction
    Fen_Fault,			// FP not-enabled fault
    Pal_Fault,			// call_pal S/W interrupt
    Integer_Overflow_Fault,
    Fake_Mem_Fault,
    Process_Halt_Fault, // hack to enable restart of processes
    Num_Faults			// number of faults
};

const char *
FaultName(int index);

#endif // __FAULTS_HH__
