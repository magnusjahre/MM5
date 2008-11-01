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
#include <iomanip>

#include "base/cprintf.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/instpipe.hh"

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
//  NOTE:  The count of instructions in this queue is for the number of actual
//         instructions (SQUASHED and not-SQUASHED)...
//
//         -->  The pipeline doesn't care if an inst is squashed!!!
//
//////////////////////////////////////////////////////////////////////////////


//============================================================================
//
//
//  The packets that travel through the pipe
//
//

InstructionPacket::InstructionPacket(unsigned width)
{
    packetWidth = width;

    for (int i = 0; i < packetWidth; ++i)
	insts.push_back(* (new DynInst *));

    instsTotal = 0;
}


void
InstructionPacket::add(DynInst *inst)
{
    // make sure we haven't reached packet size limit yet
    assert(instsTotal < packetWidth);

    // we don't add squashed instructions to a pipe!
    assert(inst != 0);

    if (instsTotal == 0) {
	thread = inst->thread_number;
    } else {
	//  each packet should hold instructions for only ONE thread
	assert(thread == inst->thread_number);
    }

    insts[instsTotal++] = inst;
}


void
InstructionPacket::clear()
{
    instsTotal = 0;

    for (int i = 0; i < packetWidth; ++i)
	insts[i] = 0;
}


void
InstructionPacket::squash(unsigned idx)
{
    if (insts[idx] == 0)
	return;

    insts[idx]->squash();

    delete insts[idx];
    insts[idx] = 0;
}


void
InstructionPacket::dump(char *leader)
{
    for (int i = 0; i < instsTotal; ++i) {
	cprintf("%s : #%d : ", leader, i);

	if (insts[i] == 0)
	    cout << "(squashed)\n";
	else
	    insts[i]->dump();
    }
}



//============================================================================
//
//
//  The pipe itself
//
//

InstructionPipe::InstructionPipe(unsigned length, unsigned bandwidth)
{
    pipeLength = length;
    maxBW = bandwidth;
    maxInsts = length * bandwidth;

    //
    //  Allocate instruction packets
    //
    for (int i = 0; i < length; ++i)
	packets.push_back(* (new InstructionPacket(bandwidth)) );

    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	instsThread[i] = 0;

    instsTotal = 0;

    stagePtr = 0;
}



void
InstructionPipe::add(DynInst *inst)
{
    //  Sanity check the total number of instructions
    assert (instsTotal < maxInsts);

    assert (addBW() > 0);

    packets[stagePtr].add(inst);

    ++instsTotal;
    ++instsThread[inst->thread_number];
}


//
//  Return value indicates whether there is room at the top of the
//  pipe for new instructions
//
bool
InstructionPipe::tick()
{
    //  Advance the pipeline if the tail packet is empty
    if (stageOccupancy(pipeLength - 1) == 0) {
	stagePtr = (stagePtr + 1) % pipeLength;

	return true;
    }

    return false;
}

InstructionPacket &
InstructionPipe::peek()
{
    //  Check for available instructions
    assert(instsAvailable() > 0);

    return packets[(stagePtr + 1) % pipeLength];
}


void
InstructionPipe::clearTail()
{
    if (stageOccupancy(pipeLength - 1) != 0) {
	unsigned cnt = packets[tailIndex()].count();
	unsigned thread = packets[tailIndex()].threadNumber();

	instsTotal -= cnt;
	instsThread[thread] -= cnt;

	packets[tailIndex()].clear();
    }
}


unsigned
InstructionPipe::instsAvailable(unsigned thread)
{
    if (stageOccupancy(pipeLength - 1) > 0 &&
	packets[tailIndex()].inst(0)->thread_number == thread)
    {
	return instsAvailable();
    }

    return 0;
}


unsigned
InstructionPipe::squash()
{
    for (int s = 0; s < pipeLength; ++s)
	for (int i = 0; i < packets[s].count(); ++i)
	    packets[s].squash(i);

    return instsTotal;
}


unsigned
InstructionPipe::squash(unsigned thread)
{
    unsigned cnt = 0;

    for (int s = 0; s < pipeLength; ++s) {
	for (int i = 0; i < packets[s].count(); ++i) {
	    DynInst *inst = packets[s].inst(i);

	    if (inst != 0 && inst->thread_number == thread) {
		packets[s].squash(i);
		++cnt;
	    }
	}
    }

    return cnt;
}


void
InstructionPipe::dump(unsigned stage)
{
    unsigned s = packetIndex(stage);

    cout << "Stage: " << stage << "(" << packets[s].count()
	 << "instructions)" << endl;

    packets[s].dump((char*) "  ");
}


void
InstructionPipe::dumpThread(unsigned thread)
{
    for (int stage = 0; stage < pipeLength; ++stage)
	if (stageOccupancy(stage) > 0
	    && packets[stage].inst(0)->thread_number == thread)
	    dump(stage);
	else
	    cout << "Stage: " << stage << "(0 instructions)" << endl;
}


void
InstructionPipe::dump()
{
    for (int stage = 0; stage < pipeLength; ++stage)
	dump(stage);
}


