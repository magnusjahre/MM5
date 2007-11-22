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

#include <iostream>

#include "encumbered/cpu/full/inst_fifo.hh"

InstructionFifo::InstructionFifo(unsigned sz)
{
    instructions = new FifoBuffer<InstRecord>(sz);

    size = sz;

    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	insts[i] = 0;
}


unsigned
InstructionFifo::add(DynInst *inst)
{
    assert(instructions->free_slots());

    //
    //  If this instruction is not squashed, add it.
    //  ... otherwise, quietly drop it on the floor...
    //
    if (inst != 0) {
	InstRecord i(inst);
	instructions->add(i);

	++insts[inst->thread_number];
    }

    return instructions->count();
}


DynInst *
InstructionFifo::peek()
{
    InstRecord * rv = instructions->peek();
    return rv ? rv->inst : 0;
}


DynInst *
InstructionFifo::remove(unsigned *thread)
{
    InstRecord *ir = instructions->peek();
    DynInst *rv = 0;

    assert(ir != 0);

    if (thread != 0)
	*thread = ir->thread;

    --insts[ir->thread];

    rv = instructions->remove().inst;

    return rv;
}


unsigned
InstructionFifo::squash()
{
    unsigned cnt = 0;

    iterator i = instructions->head();
    for (; i.notnull(); i = i.next()) {
	if (i->inst != 0) {
	    i->squash();
	    ++cnt;
	}
    }

    return cnt;
}


unsigned
InstructionFifo::squash(unsigned thread)
{
    unsigned cnt = 0;

    iterator i = instructions->head();
    for (; i.notnull(); i = i.next()) {
	if (i->inst != 0 && i->thread == thread) {
	    i->squash();
	    ++cnt;
	}
    }

    return cnt;
}


void
InstructionFifo::dump()
{
    using namespace std;

    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cout << "  Thread " << i << ": " << insts[i]
	     << " instructions" << endl;

    //  traverse the list backwards so that elements added FIRST are
    //  at the TOP
    iterator i = instructions->tail();
    for (; i.notnull(); i = i.prev())
	if (i->inst == 0)
	    cout << "(squashed)" << endl;
	else
	    i->inst->dump();
}


void
InstructionFifo::dump(unsigned thread)
{
    using namespace std;

    cout << "  Thread " << thread << ": " << insts[thread]
	 << " instructions" << endl;

    //  traverse the list backwards so that elements added FIRST are
    //  at the TOP
    iterator i = instructions->tail();
    for (; i.notnull(); i = i.prev()) {
	if (i->thread == thread) {
	    if (i->inst == 0)
		cout << "(squashed)" << endl;
	    else
		i->inst->dump();
	}
    }
}


