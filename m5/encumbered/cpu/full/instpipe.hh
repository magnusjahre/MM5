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

//
//  A simple pipeline of instructions...
//  The packets hold pointers to DynInst. Each packet knows how many
//  instructions it contains. The pointers are at indices: 0...n-1
//
//  Once all of the instructions in the tail-end packet are read, the
//  packet can be cleared. No provisions are made for tracking a subset
//  of the instructions
//

#ifndef __ENCUMBERED_CPU_FULL_INSNPIPE_HH__
#define __ENCUMBERED_CPU_FULL_INSNPIPE_HH__

#include <vector>

#include "cpu/smt.hh"

class DynInst;

class InstructionPacket
{
  private:
    unsigned packetWidth;

    std::vector<DynInst *> insts;

    unsigned thread;
    unsigned instsTotal;

  public:
    InstructionPacket(unsigned width);

    void add(DynInst *inst);

    void clear();

    void squash(unsigned idx);

    unsigned count() { return instsTotal; }
    unsigned threadNumber() { return thread; }

    DynInst * inst(unsigned idx) { return insts[idx]; }

    void dump(char *lead);
};

class InstructionPipe
{
  private:
    unsigned pipeLength;
    unsigned maxBW;

    //  storage for the instructions
    std::vector<InstructionPacket> packets;

    unsigned maxInsts;
    unsigned instsTotal;
    unsigned instsThread[SMT_MAX_THREADS];

    unsigned stagePtr;  //  always points to the "insert stage"

    unsigned packetIndex(unsigned stage) {
	return (pipeLength + stagePtr - stage) % pipeLength;
    }

    //  (pipeLength + stagePtr - (pipeLength - 1)) % pipeLength
    unsigned tailIndex() { return (stagePtr + 1) % pipeLength; }

    //  (pipeLength + stagePtr - (0)) % pipeLength
    unsigned headIndex() { return (pipeLength + stagePtr) % pipeLength; }


  public:
    InstructionPipe(unsigned length, unsigned bandwidth);

    void add(DynInst *inst);

    InstructionPacket & peek();

    void clearTail();

    unsigned squash();
    unsigned squash(unsigned thread_number);

    //  return the available add() bandwidth
    unsigned addBW() {
	return (maxBW - packets[stagePtr].count());
    }

    //  Advance the instruction packets
    bool tick();

    void dump();
    void dump(unsigned stage);
    void dumpThread(unsigned thread);

    unsigned stageOccupancy(unsigned stage) {
	return packets[packetIndex(stage)].count();
    }

    unsigned count() { return instsTotal; }
    unsigned count(unsigned thread) { return instsThread[thread]; }

    unsigned instsAvailable() { return stageOccupancy(pipeLength-1); }
    unsigned instsAvailable(unsigned thread);
};


#endif // __ENCUMBERED_CPU_FULL_INSNPIPE_HH__
