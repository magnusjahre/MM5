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

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "base/statistics.hh"
#include "encumbered/cpu/full/create_vector.hh"
#include "encumbered/cpu/full/dd_queue.hh"
#include "encumbered/cpu/full/dep_link.hh"
#include "encumbered/cpu/full/fetch.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/ls_queue.hh"
#include "encumbered/cpu/full/readyq.hh"
#include "encumbered/cpu/full/rob_station.hh"
#include "encumbered/cpu/full/spec_state.hh"
#include "encumbered/cpu/full/thread.hh"
#include "encumbered/cpu/full/writeback.hh"
#include "sim/eventq.hh"
#include "sim/stats.hh"

using namespace std;

InstSeqNum writeback_break = 0;
void
writeback_breakpoint()
{
    cout << "got to WB break point!\n";
}

//
// simulate the writeback stage of the pipeline
//
void
FullCPU::writeback()
{
    writebackEventQueue.serviceEvents();
}

//
//  IQ_WRITEBACK() - instruction result writeback pipeline stage
//
// writeback completed operation results from the functional units to IQ,
// at this point, the output dependency chains of completing instructions
// are also walked to determine if any dependent instruction now has all
// of its register operands, if so the (nearly) ready instruction is inserted
// into the ready instruction queue
//
// The writeback queue holds pointers to the ROB entry for the
//   completed instructions The instruction entries in the IQ were
//   removed when the instruction was issued.

unsigned
ROBStation::writeback(FullCPU *cpu, unsigned wb_iqueue)
{
	int i;
	unsigned wb_events = 0;

	DPRINTF(IQ, "%s: Writing back instruction #%d (#%d, PC %d)\n", cpu->name(), seq, inst->fetch_seq, inst->PC);

	if (writeback_break && (writeback_break == seq)) {
		writeback_breakpoint();
	}

	/* RS has completed execution and (possibly) produced a result */
	if (!issued || completed)
		panic("writeback: inst completed and !issued or completed");

	/*  Squashed instructions need to be removed  */
	if (squashed) {
		DPRINTF(IQ, "%s: Instruction # %d (PC %d) is squashed, returning\n", cpu->name(), seq, inst->PC);
		/*  We need to remove associated LSQ entries also  */
		if (lsq_entry.notnull()) {
			cpu->LSQ->squash(lsq_entry);
			if (cpu->ptrace)
				cpu->ptrace->deleteInst(lsq_entry->inst);
		}

		cpu->remove_ROB_element(this);

		return 0;
	}

	Tick pred_cycle = (eaCompPending ? pred_wb_cycle - 1 : pred_wb_cycle);

	cpu->pred_wb_error_dist.sample(curTick - pred_cycle);

	//
	//  MISPREDICTED Branch
	//
	//  Clean up if this is the FIRST writeback event for this instruction
	//
	if (inst->recover_inst) {
		wb_events = PipeTrace::MisPredict;

		//
		//  The idea here is to order recovery events such that
		//  once an event of spec-level "n" is scheduled, we do not
		//  sechedule an event for spec-level of "m" if m < n
		//
		//  This means that once an event is scheduled, we don't
		//  schedule events for younger recovery-instructions
		//  (since these should be blown away by the existing
		//  event, anyway)
		//
		ThreadInfo *threadInfo = &cpu->thread_info[thread_number];

		DPRINTF(IQ, "%s: inst %#d (#%d) is mispredict and needs recovery, %s, thread info spec level %d, inst spec mode %d\n",
				cpu->name(),
				seq,
				inst->fetch_seq,
				(threadInfo->recovery_event_pending ? "recovery event pending" : "no recovery event pending"),
				threadInfo->recovery_spec_level,
				inst->spec_mode);

		if (!threadInfo->recovery_event_pending ||
				threadInfo->recovery_spec_level > inst->spec_mode) {
			Tick sched_time = curTick + cpu->cycles(cpu->mispred_fixup_penalty);

			//
			//  If we are using multiple IQ's, we schedule a _single_ event
			//  for all of them, but we delay this event by the
			//  IQ communication latency
			//
			if (cpu->numIQueues > 1)
				sched_time += cpu->cycles(cpu->iq_comm_latency);

			DPRINTF(IQ, "%s: Instruction #%d (#%d, PC %d) resolves branch mispredict, scheduling recovery for %d\n",
					cpu->name(), seq, inst->fetch_seq, inst->PC, sched_time);

			//  Schedule the event that will fix up the Fetch stage, IFQ,
			//  FTDQ, and IQ/LSQ
			BranchRecoveryEvent *ev =
					new BranchRecoveryEvent(cpu, this, thread_number,
							spec_state, inst->spec_mode);
			ev->schedule(sched_time);

			//
			//  The recovery event will take responsibility for deleting the
			// spec_state information
			//
			spec_state = 0;

			//  Grab a pointer to this event in case commit needs it.
			recovery_event = ev;

			//  Make a note of this event...
			cpu->thread_info[thread_number].recovery_event_pending = true;
			cpu->thread_info[thread_number].recovery_spec_level
			= inst->spec_mode;
		}
	}


	//
	//  Writeback the results of this instrution to the specified IQ
	//
	//
	int eff_num_outputs = num_outputs;
	unsigned num_consumers = 0;

	if (!eaCompPending) {

		CreateVector *cv = &cpu->create_vector[thread_number];

		//
		//  This instruction has written its results back to the register
		//  file... update the create vectors to indicate this fact
		//
		for (i = 0; i < num_outputs; i++) {
			if (inst->spec_mode) {

				// update the speculative create vector:
				// future operations get value from later creator or
				// architected reg file
				CVLink link = cv->spec_cv[onames[i]];

				if (link.rs == this && link.odep_num == i) {

					// the result can now be read from a physical
					// register, indicate this as so
					cv->spec_cv[onames[i]] = CVLINK_NULL;
					cv->spec_timestamp[onames[i]] = curTick;
				}
				// else, creator invalidated or there is another
				// creator
			} else {

				// update the non-speculative create vector, future
				// operations get value from later creator or
				// architected register file
				CVLink link = cv->cv[onames[i]];
				if (link.rs == this && link.odep_num == i) {
					// the result can now be read from a physical
					// register, indicate this as so
					cv->cv[onames[i]] = CVLINK_NULL;
					cv->timestamp[onames[i]] = curTick;
				}
				// else, creator invalidated or there is another
				// creator
			}
		}		//   for all outputs

		DPRINTF(IQ, "%s: Instruction #%d (#%d ,PC %d) has been executed, notifying consumers in the IQ and LSQ\n", cpu->name(), seq, inst->fetch_seq, inst->PC);

		//
		//  Tell IQ & LSQ that this instruction is complete
		//
		//  Every non-EA_Comp instruction must walk its output-dependence
		//  chains to broadcast its results to other instructions
		//
		num_consumers =  cpu->IQ[wb_iqueue]->writeback(this, wb_iqueue);

		//
		//  It's ok to call the LSQ multiple times, since the odep link gets
		//  removed once it has been used.
		//
		//  The queue number argument is unused (ignored) for the LSQ
		num_consumers += cpu->LSQ->writeback(this, 0);
	} else {
		//
		//  Address-generation portion of load/store gets special handling
		//
		//  --> only one op receives the result data
		//  --> no create-vectors to mess with
		//

		DPRINTF(IQ, "%s: Address generation complete for instruction #%d (#%d, PC %d)\n", cpu->name(), seq, inst->fetch_seq, inst->PC);

		assert(seq == lsq_entry->seq);

		eff_num_outputs = 1;
		num_consumers = 1;

		//  Indicate that we've finished with the address generation portion
		eaCompPending = false;

		lsq_entry->idep_ready[MEM_ADDR_INDEX] = true;

		issued = false;
		completed = false;

		// LSQ operation will be placed on LSQ's ready list in
		// lsq_refresh(), depending on things like address
		// disambiguation and memory barriers.
	}

	if (cpu->ptrace)
		cpu->ptrace->moveInst(inst, PipeTrace::Writeback, wb_events, 0, 0);

	//
	//  Statistics...
	//
	if (eff_num_outputs > 0) {
		++cpu->producer_inst[thread_number];
		cpu->consumer_inst[thread_number] += num_consumers;
	}

	return num_consumers;
}



//////////////////////////////////////////////////////////////////



//
//  recover processor microarchitecture state back to point of the
//  mis-predicted branch at IQ[BRANCH_INDEX]
//
//  This routine is called from the BranchRecoveryEvent
//
void
FullCPU::recover(ROBStation *ROB_branch_entry, int branch_thread)
{
    BaseIQ::iterator IQ_entry;
    BaseIQ::iterator LSQ_entry;
    ROBStation *ROB_entry;

    // recover from the tail of the ROB towards the head until the branch index
    // is reached, this direction ensures that the LSQ can be synchronized with
    // the IQ

    // traverse to older insts until the mispredicted branch is encountered
    // Mark all instructions (from the same thread) FOLLOWING the branch as
    // squashed (the branch itself commits as a normal instruction)

    //  go to first element to squash (ie. the last element in the list)
    for (ROB_entry = ROB.tail();
	 (ROB_entry != ROB_branch_entry) && (ROB_entry != NULL);
         ROB_entry = ROB.prev(ROB_entry))
    {

	// the IQ should not drain since the mispredicted branch will remain
	assert(ROB.num_active());

	// should meet up with the branch index first
	assert(ROB_entry);

	if (!ROB_entry->squashed &&
	    (ROB_entry->thread_number == branch_thread)) {

	    if (ptrace) {
		ptrace->deleteInst(ROB_entry->inst);
	    }

#if 0
	    if (chainGenerator) {
		chainGenerator->squashInstruction(ROB_entry);
	    }
#endif

	    DPRINTF(IQ, "Branch recovery event causes squash of inst #%d\n", ROB_entry->seq);

	    //
	    //  Inform the IQ and LSQ that an instruction is being squashed
	    //
	    for (int i=0; i<numIQueues; ++i) {
		IQ[i]->inform_squash(ROB_entry);
	    }
	    LSQ->inform_squash(ROB_entry);

	    //
	    //  Remove any associated IQ or LSQ entries
	    //
	    if (ROB_entry->lsq_entry.notnull()) {
		LSQ_entry = ROB_entry->lsq_entry;

		// squash this LSQ entry
		LSQ_entry->tag++;
		LSQ_entry->squashed = true;

		LSQ->squash(LSQ_entry);
	    }

	    if (ROB_entry->iq_entry.notnull()) {
		IQ_entry = ROB_entry->iq_entry;

		// squash this IQ entry
		IQ_entry->tag++;
		IQ_entry->squashed = true;

		//  remove the instruction from it's home IQ
		IQ[ROB_entry->queue_num]->squash(IQ_entry);
		ROB_entry->iq_entry = 0;

		//  tell the remaining clusters that it's gone
		for (int q = 0; q < numIQueues; ++q)
		    if (q != ROB_entry->queue_num)
			IQ[q]->inform_squash(ROB_entry);
	    }


	    //
	    //   Recover RS_LINKs used by this instruction
	    //
	    for (int i = 0; i < TheISA::MaxInstDestRegs; i++) {
		// blow away the consuming op list
		DepLink *dep_node, *dep_node_next;

		for (int q = 0; q < numIQueues; ++q) {
		    for (dep_node = ROB_entry->odep_list[i][q];
			 dep_node != NULL;
			 dep_node = dep_node_next) {

			dep_node->iq_consumer->idep_ptr[dep_node->idep_num]
			    = 0;

			dep_node_next = dep_node->next();
			delete dep_node;
		    }
		}
	    }

	    if(ROB_entry->seq == stalledOnInstSeqNum){
	    	assert(isStalled);
	    	assert(ROB_entry->inst->isLoad());
			assert(getCacheAddr(ROB_entry->inst->phys_eff_addr) == stalledOnAddr);
			overlapEstimator->executionResumed(true);
			isStalled = false;
			stalledOnAddr = MemReq::inval_addr;
			stalledOnInstSeqNum = 0;
	    }

	    ROB_entry->squash();

	    remove_ROB_element(ROB_entry);
	}
    }

    // FIXME: could reset functional units at squash time

    //  squash any instructions in the fetch_to_decode queue
    decodeQueue->squash(branch_thread);

    // squash instructions and restart execution at the recover PC
    fetch_squash(branch_thread);
}


//==========================================================================
//
//  Branch-Recovery Event...
//

BranchRecoveryEvent::BranchRecoveryEvent(FullCPU *_cpu, ROBStation *rs,
					 int thread,
					 SpecStateList::ss_iterator sstate,
					 int spec_mode)
        : Event(&mainEventQueue),
	  cpu(_cpu), thread_number(thread), branch_entry(rs),
	  correct_PC(rs->inst->Next_PC), branch_PC(rs->inst->PC),
	  dir_update(rs->inst->dir_update), staticInst(rs->inst->staticInst),
	  this_spec_mode(spec_mode), spec_state(sstate)
{
    setFlags(AutoDelete);
}


BranchRecoveryEvent::~BranchRecoveryEvent()
{
    if (spec_state.notnull())
	cpu->state_list.dump(spec_state);
}

void
BranchRecoveryEvent::process()
{
    SpecExecContext *xc = cpu->thread[thread_number];

	DPRINTF(IQ, "%s: Processing branch recovery event for inst #%d, setting PC to %d, setting speculative mode to %d (was %d)\n",
			cpu->name(),
			(branch_entry != NULL ? branch_entry->seq : 0),
			correct_PC,
			this_spec_mode,
			xc->spec_mode);

    cpu->recover(branch_entry, thread_number);

    //
    //  If the ROB entry has not committed yet...
    //    (1) mark that we have already recovered for this instruction
    //    (2)
    //
    if (branch_entry != NULL) {
	branch_entry->inst->recover_inst = false;
	branch_entry->recovery_event = 0;
    }

    /*  Put the PC back on track  */
    xc->regs.pc = correct_PC;
    xc->regs.npc = correct_PC + sizeof(MachInst);

    if (staticInst->isControl() && cpu->branch_pred)
	cpu->branch_pred->recover(thread_number, branch_PC, &dir_update);

    //
    //  Only clear the pending flag if this event is the lowest
    //  spec-level event
    //
    if (cpu->thread_info[thread_number].recovery_spec_level ==
	this_spec_mode)
    {
	cpu->thread_info[thread_number].recovery_event_pending = false;
	cpu->thread_info[thread_number].recovery_spec_level = 0;
    }

    //  Set the global spec_mode[] value...
    xc->spec_mode = this_spec_mode;

    //
    //  Clear out speculative state if this was the last level of
    //  misspeculation.
    //
    //  This CLEAR_MAP step SHOULD be unnecessary given that the
    //  COPY step should always be correct.
    //

    cpu->state_list.release(spec_state);
    //	delete spec_state;
    spec_state = 0;    // don't try to delete this more than once...


    if (xc->spec_mode == 0) {
	/* reset use_spec_? reg maps and speculative memory state */
	xc->reset_spec_state();

	// since reset_spec_state() doesn't do this...
	cpu->create_vector[thread_number].clear_spec();
    }
}


const char *
BranchRecoveryEvent::description()
{
    return "branch recovery";
}

//==========================================================================
//
//  Writeback Event...
//
//  This event is scheduled for the first (possibly ONLY) writeback
//  for an instruction
//
WritebackEvent::WritebackEvent(FullCPU *_cpu, ROBStation *target_rs)
    : Event(&_cpu->writebackEventQueue),
      cpu(_cpu), rob_entry(target_rs), tag(target_rs->seq),
      req(NULL)
{
    setFlags(AutoDelete);
}


WritebackEvent::WritebackEvent(FullCPU *_cpu, ROBStation *target_rs,
			       MemReqPtr &_req)
    : Event(&_cpu->writebackEventQueue),
      cpu(_cpu), rob_entry(target_rs), tag(target_rs->seq),
      req(_req)
{
    setFlags(AutoDelete);
}


void
WritebackEvent::trace(const char *action)
{
    DPRINTFN("WritebackEvent for inst %d %s @ %d\n", rob_entry->seq,
	     action, when());
}

const char *
WritebackEvent::description()
{
    return "writeback";
}

//
//  Process a writeback event to the IQ indicated
//
//  This event will schedule DelayedWritebackEvent's if necessary
//
void
WritebackEvent::process()
{
    //  grab this here, since writeback() may change it
    bool ea_comp_inst = rob_entry->eaCompPending;

    cpu->writeback_count[rob_entry->inst->thread_number]++;

    if (DTRACE(Pipeline)) {
	string s;
	rob_entry->inst->dump(s);
	DPRINTF(Pipeline, "Writeback %s\n", s);
    }

    //
    //  Recover instructions do not generate two writeback events.
    //  They write-back to all IQ's during the same cycle (ie. their
    //  "home" queue is written-back late)
    //
    if (rob_entry->inst->recover_inst) {
	unsigned wb_to_other = 0;

	for (unsigned q = 0; q < cpu->numIQueues; ++q) {
	    unsigned c = rob_entry->writeback(cpu, q);

	    if (q != rob_entry->queue_num)
		wb_to_other += c;
	}

	//  update stats
	cpu->wb_penalized[rob_entry->inst->thread_number] += wb_to_other;
    } else {
	//
	//  "Normal" instructions use the two-phase writeback
	//
	rob_entry->writeback(cpu, rob_entry->queue_num);
    }

    //
    //  If we are writing back to more than one IQ, we have to pay a
    //  penalty for all but the IQ/FU where this instruction executed
    //
    //  here we schedule a writeback event for the OTHER queues...
    //
    //  Again, note that recover instructions are DONE after this event
    //
    if (!ea_comp_inst) {
	if (cpu->numIQueues > 1 && !rob_entry->inst->recover_inst) {

	    DelayedWritebackEvent *ev =
		new DelayedWritebackEvent(cpu, rob_entry);

	    rob_entry->delayed_wb_event = ev;
	    ev->schedule(curTick + cpu->cycles(cpu->iq_comm_latency));

	} else
	    rob_entry->completed = true;
    }

    //  indicate that this pointer is becoming invalid
    rob_entry->wb_event = 0;

    if (req)
	req = NULL; // EGH Fix Me: do I need this
}



//==========================================================================
//
//  Delayed Writeback Event...
//
//  This event is scheduled to writeback values to clusters OTHER than the
//  cluster in which the instruction executed. It is only used when
//  modeling a multi-clustered machine
//
DelayedWritebackEvent::DelayedWritebackEvent(FullCPU *_cpu,
					     ROBStation *target_rs)
    : Event(&_cpu->writebackEventQueue, Delayed_Writeback_Pri),
    cpu(_cpu), rob_entry(target_rs), tag(target_rs->seq)
{
    setFlags(AutoDelete);
}

//
//  Process a delayed writeback event
//
//  TO ALL BUT THE INDICATED Queue!!!
//
void
DelayedWritebackEvent::process()
{
    unsigned wb_to_other = 0;

    //  writeback to all remote IQ's
    for (unsigned c = 0; c < cpu->numIQueues; ++c)
	if (rob_entry->queue_num != c)
	    wb_to_other += rob_entry->writeback(cpu, c);

    //  update stats
    cpu->wb_penalized[rob_entry->inst->thread_number] += wb_to_other;

    //  EA-Comp instructions should never get here...
    rob_entry->completed = true;

    //  indicate that this pointer is becoming invalid
    rob_entry->delayed_wb_event = 0;
}


//////////////////////////////////////////////////////////////////
void
FullCPU::writebackRegStats()
{
    using namespace Stats;
    writeback_count
	.init(number_of_threads)
	.name(name() + ".WB:count")
	.desc("cumulative count of insts written-back")
	.flags(total)
	;

    producer_inst
	.init(number_of_threads)
	.name(name() + ".WB:producers")
	.desc("num instructions producing a value")
	.flags(total)
	;

    consumer_inst
	.init(number_of_threads)
	.name(name() + ".WB:consumers")
	.desc("num instructions consuming a value")
	.flags(total)
	;

    wb_penalized
	.init(number_of_threads)
	.name(name() + ".WB:penalized")
	.desc("number of instrctions required to write to 'other' IQ")
	.flags(total)
	;

    pred_wb_error_dist
	.init(-10,100,1)
	.name(name() + ".WB:pred_error")
	.desc("error in predicted writeback times")
	.flags(pdf | cdf)
	;

}

void
FullCPU::writebackRegFormulas()
{
    using namespace Stats;

    wb_penalized_rate
	.name(name() + ".WB:penalized_rate")
	.desc ("fraction of instructions written-back that wrote to 'other' IQ")
	.flags(total)
	;

    wb_penalized_rate = wb_penalized / writeback_count;

    wb_fanout
	.name(name() + ".WB:fanout")
	.desc("average fanout of values written-back")
	.flags(total)
	;

    wb_fanout = producer_inst / consumer_inst;

    wb_rate
	.name(name() + ".WB:rate")
	.desc("insts written-back per cycle")
	.flags(total)
	;
    wb_rate = writeback_count / numCycles;

}









