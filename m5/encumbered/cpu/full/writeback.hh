/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005
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

#ifndef __ENCUMBERED_CPU_FULL_WRITEBACK_HH__
#define __ENCUMBERED_CPU_FULL_WRITEBACK_HH__

#include "encumbered/cpu/full/cv_spec_state.hh"
#include "sim/eventq.hh"

class FullCPU;
class ROBStation;

// pending writeback events, sorted from soonest to latest event (in
// time).  Instance tag values (old SimpleScalar RS_LINK nodes) are
// used so that the list need not be cleaned out during squash events
struct WritebackEvent : public Event
{
    FullCPU *cpu;

    ROBStation *rob_entry;		// referenced ROB entry
    InstTag tag;			// instance sequence number
    MemReqPtr req;			// Memory request for loads and stores
    BaseIQ::iterator queue_entry;

    // constructors
    WritebackEvent(FullCPU *_cpu, ROBStation *target_rs);
    WritebackEvent(FullCPU *_cpu, ROBStation *target_rs, MemReqPtr &req);

    // event processing: perform writeback on event execution
    virtual void process();

    virtual const char *description();

    // debug tracing
    virtual void trace(const char *action);

    // inline destructor for FastAlloc
    ~WritebackEvent() { }
};


struct DelayedWritebackEvent : public Event
{
    FullCPU *cpu;
    ROBStation *rob_entry;
    InstTag tag;       //  for debugging

    // constructor
    DelayedWritebackEvent(FullCPU *_cpu, ROBStation *target_rs);

    // event processing: perform writeback on event execution
    virtual void process();

    // inline destructor for FastAlloc
    ~DelayedWritebackEvent() { }
};


class StaticInstBase;

// Schedulable event to invoke clear_fetch_stall at a particular cycle
class BranchRecoveryEvent : public Event
{
    // event data fields
    FullCPU * cpu;
    int thread_number;
    ROBStation *branch_entry;

    Addr correct_PC;
    Addr branch_PC;			// We have to save this info in case
    BPredUpdateRec dir_update;	// the ROB entry goes away...
    StaticInstBasePtr staticInst;

    int this_spec_mode;      // The spec_mode_value for this instruction

    //    CreateVecSpecState *spec_state;
    SpecStateList::ss_iterator spec_state;

    Tick issuedAt;

  public:
    // constructor
    BranchRecoveryEvent(FullCPU *_cpu, ROBStation *rs, int thread,
			SpecStateList::ss_iterator sstate, int spec_mode);

    // executed when the ROB entry is removed prior to recovery...
    void invalidate_branch_entry() { branch_entry = NULL; }

    // event execution function
    void process();

    virtual const char *description();

    ~BranchRecoveryEvent();
};

#endif // __ENCUMBERED_CPU_FULL_WRITEBACK_HH__
