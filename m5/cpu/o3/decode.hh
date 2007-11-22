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

#ifndef __CPU_O3_CPU_SIMPLE_DECODE_HH__
#define __CPU_O3_CPU_SIMPLE_DECODE_HH__

#include <queue>

#include "base/statistics.hh"
#include "base/timebuf.hh"

template<class Impl>
class SimpleDecode
{
  private:
    // Typedefs from the Impl.
    typedef typename Impl::ISA ISA;
    typedef typename Impl::FullCPU FullCPU;
    typedef typename Impl::DynInstPtr DynInstPtr;
    typedef typename Impl::Params Params;
    typedef typename Impl::CPUPol CPUPol;

    // Typedefs from the CPU policy.
    typedef typename CPUPol::FetchStruct FetchStruct;
    typedef typename CPUPol::DecodeStruct DecodeStruct;
    typedef typename CPUPol::TimeStruct TimeStruct;

    // Typedefs from the ISA.
    typedef typename ISA::Addr Addr;

  public:
    // The only time decode will become blocked is if dispatch becomes
    // blocked, which means IQ or ROB is probably full.
    enum Status {
        Running,
        Idle,
        Squashing,
        Blocked,
        Unblocking
    };

  private:
    // May eventually need statuses on a per thread basis.
    Status _status;

  public:
    SimpleDecode(Params &params);

    void regStats();

    void setCPU(FullCPU *cpu_ptr);

    void setTimeBuffer(TimeBuffer<TimeStruct> *tb_ptr);

    void setDecodeQueue(TimeBuffer<DecodeStruct> *dq_ptr);

    void setFetchQueue(TimeBuffer<FetchStruct> *fq_ptr);

    void tick();

    void decode();

  private:
    inline bool fetchInstsValid();

    void block();
    
    inline void unblock();

    void squash(DynInstPtr &inst);

  public:
    // Might want to make squash a friend function.
    void squash();

  private:
    // Interfaces to objects outside of decode.
    /** CPU interface. */
    FullCPU *cpu;

    /** Time buffer interface. */
    TimeBuffer<TimeStruct> *timeBuffer;

    /** Wire to get rename's output from backwards time buffer. */
    typename TimeBuffer<TimeStruct>::wire fromRename;
    
    /** Wire to get iew's information from backwards time buffer. */
    typename TimeBuffer<TimeStruct>::wire fromIEW;
    
    /** Wire to get commit's information from backwards time buffer. */
    typename TimeBuffer<TimeStruct>::wire fromCommit;
    
    /** Wire to write information heading to previous stages. */
    // Might not be the best name as not only fetch will read it.
    typename TimeBuffer<TimeStruct>::wire toFetch;

    /** Decode instruction queue. */
    TimeBuffer<DecodeStruct> *decodeQueue;

    /** Wire used to write any information heading to rename. */
    typename TimeBuffer<DecodeStruct>::wire toRename;

    /** Fetch instruction queue interface. */
    TimeBuffer<FetchStruct> *fetchQueue;

    /** Wire to get fetch's output from fetch queue. */
    typename TimeBuffer<FetchStruct>::wire fromFetch;

    /** Skid buffer between fetch and decode. */
    std::queue<FetchStruct> skidBuffer;

    //Consider making these unsigned to avoid any confusion.
    /** Rename to decode delay, in ticks. */
    unsigned renameToDecodeDelay;
    
    /** IEW to decode delay, in ticks. */
    unsigned iewToDecodeDelay;
    
    /** Commit to decode delay, in ticks. */
    unsigned commitToDecodeDelay;

    /** Fetch to decode delay, in ticks. */
    unsigned fetchToDecodeDelay;

    /** The width of decode, in instructions. */
    unsigned decodeWidth;

    /** The instruction that decode is currently on.  It needs to have
     *  persistent state so that when a stall occurs in the middle of a
     *  group of instructions, it can restart at the proper instruction.
     */
    unsigned numInst;

    Stats::Scalar<> decodeIdleCycles;
    Stats::Scalar<> decodeBlockedCycles;
    Stats::Scalar<> decodeUnblockCycles;
    Stats::Scalar<> decodeSquashCycles;
    Stats::Scalar<> decodeBranchMispred;
    Stats::Scalar<> decodeControlMispred;
    Stats::Scalar<> decodeDecodedInsts;
    Stats::Scalar<> decodeSquashedInsts;
};

#endif // __CPU_O3_CPU_SIMPLE_DECODE_HH__
