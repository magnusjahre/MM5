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

//Todo: Update with statuses.
//Need to handle delaying writes to the writeback bus if it's full at the
//given time.

#ifndef __CPU_O3_CPU_SIMPLE_IEW_HH__
#define __CPU_O3_CPU_SIMPLE_IEW_HH__

#include <queue>

#include "config/full_system.hh"
#include "base/statistics.hh"
#include "base/timebuf.hh"
#include "cpu/o3/comm.hh"

template<class Impl>
class SimpleIEW
{
  private:
    //Typedefs from Impl
    typedef typename Impl::ISA ISA;
    typedef typename Impl::CPUPol CPUPol;
    typedef typename Impl::DynInstPtr DynInstPtr;
    typedef typename Impl::FullCPU FullCPU;
    typedef typename Impl::Params Params;

    typedef typename CPUPol::IQ IQ;
    typedef typename CPUPol::RenameMap RenameMap;
    typedef typename CPUPol::LDSTQ_TYPE LDSTQ;

    typedef typename CPUPol::TimeStruct TimeStruct;
    typedef typename CPUPol::IEWStruct IEWStruct;
    typedef typename CPUPol::RenameStruct RenameStruct;
    typedef typename CPUPol::IssueStruct_TYPE IssueStruct;

    friend class Impl::FullCPU;
  public:
    enum Status {
        Running,
        Blocked,
        Idle,
        Squashing,
        Unblocking
    };

  private:
    Status _status;
    Status _issueStatus;
    Status _exeStatus;
    Status _wbStatus;

  public:
    class WritebackEvent : public Event {
      private:
        DynInstPtr inst;
        SimpleIEW<Impl> *iewStage;

      public:
	WritebackEvent(DynInstPtr &_inst, SimpleIEW<Impl> *_iew);

	virtual void process();
	virtual const char *description();
    };

  public:
    SimpleIEW(Params &params);

    void regStats();

    void setCPU(FullCPU *cpu_ptr);

    void setTimeBuffer(TimeBuffer<TimeStruct> *tb_ptr);

    void setRenameQueue(TimeBuffer<RenameStruct> *rq_ptr);

    void setIEWQueue(TimeBuffer<IEWStruct> *iq_ptr);

    void setRenameMap(RenameMap *rm_ptr);

    void squash();

    void squashDueToBranch(DynInstPtr &inst);

    void squashDueToMem(DynInstPtr &inst);

    void block();

    inline void unblock();

    void wakeDependents(DynInstPtr &inst);

    void instToCommit(DynInstPtr &inst);

  private:
    void dispatchInsts();

    void executeInsts();

  public:
    void tick();

    void iew();

    //Interfaces to objects inside and outside of IEW.
    /** Time buffer interface. */
    TimeBuffer<TimeStruct> *timeBuffer;

    /** Wire to get commit's output from backwards time buffer. */
    typename TimeBuffer<TimeStruct>::wire fromCommit;

    /** Wire to write information heading to previous stages. */
    typename TimeBuffer<TimeStruct>::wire toRename;

    /** Rename instruction queue interface. */
    TimeBuffer<RenameStruct> *renameQueue;

    /** Wire to get rename's output from rename queue. */
    typename TimeBuffer<RenameStruct>::wire fromRename;

    /** Issue stage queue. */
    TimeBuffer<IssueStruct> issueToExecQueue;

    /** Wire to read information from the issue stage time queue. */
    typename TimeBuffer<IssueStruct>::wire fromIssue;

    /** 
     * IEW stage time buffer.  Holds ROB indices of instructions that
     * can be marked as completed.
     */
    TimeBuffer<IEWStruct> *iewQueue;

    /** Wire to write infromation heading to commit. */
    typename TimeBuffer<IEWStruct>::wire toCommit;

    //Will need internal queue to hold onto instructions coming from
    //the rename stage in case of a stall.
    /** Skid buffer between rename and IEW. */
    std::queue<RenameStruct> skidBuffer;

  protected:
    /** Instruction queue. */
    IQ instQueue;

    LDSTQ ldstQueue;

#if !FULL_SYSTEM
  public:
    void lsqWriteback();
#endif

  private:
    /** Pointer to rename map.  Might not want this stage to directly 
     *  access this though... 
     */
    RenameMap *renameMap;

    /** CPU interface. */
    FullCPU *cpu;

  private:
    /** Commit to IEW delay, in ticks. */
    unsigned commitToIEWDelay;

    /** Rename to IEW delay, in ticks. */
    unsigned renameToIEWDelay;

    /** 
     * Issue to execute delay, in ticks.  What this actually represents is
     * the amount of time it takes for an instruction to wake up, be
     * scheduled, and sent to a FU for execution.
     */
    unsigned issueToExecuteDelay;

    /** Width of issue's read path, in instructions.  The read path is both
     *  the skid buffer and the rename instruction queue.
     *  Note to self: is this really different than issueWidth?
     */
    unsigned issueReadWidth;

    /** Width of issue, in instructions. */
    unsigned issueWidth;

    /** Width of execute, in instructions.  Might make more sense to break
     *  down into FP vs int.
     */
    unsigned executeWidth;

    /** Number of cycles stage has been squashing.  Used so that the stage
     *  knows when it can start unblocking, which is when the previous stage
     *  has received the stall signal and clears up its outputs.
     */
    unsigned cyclesSquashing;

    Stats::Scalar<> iewIdleCycles;
    Stats::Scalar<> iewSquashCycles;
    Stats::Scalar<> iewBlockCycles;
    Stats::Scalar<> iewUnblockCycles;
//    Stats::Scalar<> iewWBInsts;
    Stats::Scalar<> iewDispatchedInsts;
    Stats::Scalar<> iewDispSquashedInsts;
    Stats::Scalar<> iewDispLoadInsts;
    Stats::Scalar<> iewDispStoreInsts;
    Stats::Scalar<> iewDispNonSpecInsts;
    Stats::Scalar<> iewIQFullEvents;
    Stats::Scalar<> iewExecutedInsts;
    Stats::Scalar<> iewExecLoadInsts;
    Stats::Scalar<> iewExecStoreInsts;
    Stats::Scalar<> iewExecSquashedInsts;
    Stats::Scalar<> memOrderViolationEvents;
    Stats::Scalar<> predictedTakenIncorrect;
};

#endif // __CPU_O3_CPU_IEW_HH__
