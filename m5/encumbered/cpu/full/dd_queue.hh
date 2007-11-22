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

#ifndef __ENCUMBERED_CPU_FULL_DD_QUEUE_HH__
#define __ENCUMBERED_CPU_FULL_DD_QUEUE_HH__

#include "base/statistics.hh"
#include "encumbered/cpu/full/instpipe.hh"
#include "encumbered/cpu/full/inst_fifo.hh"

class FullCPU;
class fetch_instr_rec_t;
class DynInst;

class DecodeDispatchQueue
{
  private:
    bool nonBlocking;
    unsigned bandwidth;
    unsigned maxInst;

    // this pipeline will be n-1 stages long... the final queue
    // stage will actually be a skid buffer object (the dispatchBuffer)
    InstructionPipe *pipeline;

    InstructionFifo *dispatchBuffer;

    InstructionFifo *skidBuffers[SMT_MAX_THREADS];

    unsigned instsTotal;
    unsigned instsThread[SMT_MAX_THREADS];

    Stats::Vector<> cum_insts;
    Stats::Formula ddq_rate;

    FullCPU *cpu;

  public:
    DecodeDispatchQueue(FullCPU *cpu,
			unsigned length, unsigned bw, bool nonblocking);

    void regStats();
    void regFormulas();

    unsigned addBW(unsigned thread);

    //  add instructions one at a time
    void add(DynInst *inst);

    //  return a pointer to the dispatch-able instruction
    DynInst * peek(unsigned thread);

    void remove(unsigned thread);

    //  Call this between Dispatch and Decode to advance the pipeline
    void tick();

    //  Call this sometime _other_ than between Dispatch & Decode
    void tick_stats();

    //  Do we have an empty segment at the head for this thread
    bool loadable();

    unsigned squash();
    unsigned squash(unsigned thread);

    unsigned count();
    unsigned count(unsigned thread);

    unsigned instsAvailable();
    unsigned instsAvailable(unsigned thread);

    void dump();
    void dump(unsigned t);
};

#endif // __ENCUMBERED_CPU_FULL_DD_QUEUE_HH__
