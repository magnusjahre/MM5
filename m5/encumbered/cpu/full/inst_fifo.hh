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

#ifndef __ENCUMBERED_CPU_FULL_INSN_FIFO_HH__
#define __ENCUMBERED_CPU_FULL_INSN_FIFO_HH__

#include "base/fifo_buffer.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/dyn_inst.hh"

class InstructionFifo
{
  private:
    struct InstRecord
    {
	DynInst *inst;
	unsigned    thread;

	// constructor
	InstRecord() : inst(0) {}
	InstRecord(DynInst *i) : inst(i), thread(i->thread_number) {}

	void squash() {
	    inst->squash();
	    delete inst;
	    inst = 0;
	}

	void dump() {
	    assert(inst != 0);
	    inst->dump();
	}
    };

    typedef FifoBuffer<InstRecord>::iterator iterator;
    FifoBuffer<InstRecord> *instructions;

    unsigned insts[SMT_MAX_THREADS];
    unsigned size;

  public:
    InstructionFifo(unsigned size);

    unsigned add(DynInst *inst);

    unsigned count() { return instructions->count(); }
    unsigned count(unsigned t) { return insts[t]; }

    DynInst *peek();
    DynInst *remove(unsigned *thread = 0);

    unsigned squash();
    unsigned squash(unsigned t);

    void dump();
    void dump(unsigned thread);

    ~InstructionFifo() { delete instructions; }
};

#endif // __INSN_FIFO_HH__
