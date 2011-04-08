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

#include "base/cprintf.hh"
#include "base/statistics.hh"
#include "cpu/exetrace.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/bpred.hh"
#include "encumbered/cpu/full/floss_reasons.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/rob_station.hh"
#include "encumbered/cpu/full/storebuffer.hh"
#include "encumbered/cpu/full/thread.hh"
#include "encumbered/cpu/full/writeback.hh"
#include "mem/mem_interface.hh"
#include "sim/sim_events.hh"
#include "sim/stats.hh"

#include "sim/stat_control.hh"

using namespace std;

#define IPC_TRACE_FREQUENCY 500000

/*======================================================================*/

/*
 *  IQ_COMMIT() - instruction retirement pipeline stage
 *
 */


/*  Number of cycles we are allowed to go without committing an instruction  */
#define CRASH_COUNT 500000

#define MEM_BLOCKED_TRACE_FREQUENCY 100000

class FaultHandlerDelayEvent : public Event
{
	FullCPU *cpu;
	int thread;

public:
	FaultHandlerDelayEvent(FullCPU *_cpu, int _thread, Tick tick)
	: Event(&mainEventQueue), cpu(_cpu), thread(_thread)
	{
		setFlags(AutoDelete);
		schedule(tick);
	}

	~FaultHandlerDelayEvent() {}

	virtual void process()
	{
		cpu->fetch_fault_count[thread]--;
	}
};


bool
FullCPU::requestInROB(MemReqPtr& req, int blockSize){

	assert(req->oldAddr != MemReq::inval_addr);

	Addr mask = ~((Addr)blockSize - 1);

	bool found = false;
	for(ROBStation *rs = ROB.head(); rs != NULL ; rs = ROB.next(rs)){
		if(rs->inst->isLoad() || rs->inst->isStore()){
			Addr robCacheAddr = rs->inst->eff_addr & mask;
			Addr reqCacheAddr = req->oldAddr & mask;

			if(robCacheAddr == reqCacheAddr){
				found = true;
			}

		}
	}

	return true;
}

int
FullCPU::getCommittedInstructions(){
	int tmpCommitted = committedSinceLast;
	committedSinceLast = 0;
	return tmpCommitted;
}

/* this function commits the results of the oldest completed entries from the
   IQ and LSQ to the architected reg file, stores in the LSQ will commit
   their store data to the data cache at this point as well */


void
FullCPU::commit()
{
	static int crash_counter = 0;
	unsigned committed = 0;
	unsigned committed_thread[SMT_MAX_THREADS];
	int finished_thread[SMT_MAX_THREADS];
	int num_finished_threads;

	CommitEndCause reason;
	CommitEndCause reason_overall = COMMIT_CAUSE_NOT_SET;;
	CommitEndCause reason_thread[SMT_MAX_THREADS];

	int detail;
	int detail_overall = 0;
	int detail_thread[SMT_MAX_THREADS];

	// in case there are NO unfinished instructions
	InstSeqNum seq_overall = InstSeqNum(-1);
	InstSeqNum seq_thread[SMT_MAX_THREADS];

	unsigned blame = 0;   // thread numbers
	unsigned blame_overall = 0;

	//
	//  This code causes the simulator to halt if it doesn't commit
	//  an instruction in CRACH_COUNT cycles
	//
	if (++crash_counter > CRASH_COUNT) {
		ccprintf(cerr, "DEADLOCK at CPU %s\n", name());

		//FIXME: uncomment these lines
		dumpIQ();
		//         cout << "iq dump fin\n";
		LSQ->dump();
		//         cout << "lsq dump fin\n";
		ROB.dump();
		//         cout << "rob dump fin\n";
		panic("We stopped committing instructions!!!");
	}

	if(curTick % MEM_BLOCKED_TRACE_FREQUENCY == 0){
		string tracename = name() + "BlockedTrace.txt";
		ofstream tracefile(tracename.c_str(), ofstream::app);
		tracefile << curTick << "; " <<  ((double) ((double) noCommitCycles /  (double) MEM_BLOCKED_TRACE_FREQUENCY)) << "\n";
		tracefile.flush();
		tracefile.close();
		noCommitCycles = 0;
	}

	//
	//  Determine which threads we don't need to worry about
	//
	num_finished_threads = 0;
	int num_inactive_threads = 0;
	for (int i = 0; i < number_of_threads; i++) {
		// if thread has no instructions in ROB then we can skip it
		if (!thread_info[i].active || ROB.num_thread(i) == 0) {
			finished_thread[i] = true;
			num_finished_threads++;
			if(ROB.num_thread(i) == 0) commit_cycles_empty_ROB++;
			if (!thread_info[i].active
					|| execContexts[i]->status() != ExecContext::Active) {
				num_inactive_threads++;
			}
		} else {
			finished_thread[i] = false;
		}
	}

	if (num_finished_threads == number_of_threads) {
		// If we're not committing because all the threads are
		// inactive, don't consider this a microarchitectural
		// deadlock... it can happen e.g. in an MP where there is only
		// one runnable thread.
		if (num_inactive_threads == number_of_threads) {
			crash_counter = 0;
		}
		return;
	}

	//
	//  Initialize & allocate per-thread data structs...
	//
	//  FIXME:  we don't really want to do all this allocation every cycle
	//
	ROBStation **commit_list[SMT_MAX_THREADS];
	unsigned clist_num[SMT_MAX_THREADS];
	unsigned clist_idx[SMT_MAX_THREADS];

	bool list_done[SMT_MAX_THREADS];
	unsigned num_list_done = num_finished_threads;

	unsigned completed[SMT_MAX_THREADS];
	unsigned total_completed = 0;


	for (int i = 0; i < number_of_threads; ++i) {
		clist_num[i] = 0;
		list_done[i] = finished_thread[i];

		clist_idx[i] = 0;

		reason_thread[i] = COMMIT_CAUSE_NOT_SET;
		detail_thread[i] = 0;
		seq_thread[i] = 0;

		completed[i] = 0;
		committed_thread[i] = 0;

		if (!finished_thread[i]) {

			// allocate storage for the max number of insts we could
			// commit in a cycle
			commit_list[i] = new ROBStation *[commit_width];
		} else {
			commit_list[i] = 0;
		}
	}

	unsigned num_eligible = 0;

	//
	//  put commitable instructions into the lists...
	//
	//  We walk the ROB, filling each per-thread list
	//
	//  We also keep track of the first non-commitable inst for each thread
	//  and overall, and also squash instructions we encounter along the way
	//
	bool done = false;
	for (ROBStation *rs = ROB.head(); (rs != NULL) && !done; rs = ROB.next(rs))
	{
		unsigned thread = rs->thread_number;

		//
		//  count the number of instruction ready to commit
		//
		if (!finished_thread[thread] && rs->completed) {
			++completed[thread];
			++total_completed;
		}


		reason = COMMIT_CAUSE_NOT_SET;

		//
		//  ignore instructions from threads that we're done listing...
		//
		if (!list_done[thread]) {

			//
			//  If we're still looking at a thread and run across a squashed
			//  instruction, then blow it away...
			//
			if (rs->squashed) {
				if (ptrace)
					ptrace->deleteInst(rs->inst);

				remove_ROB_element(rs);

				// go look at next instruction
				continue;
			}

			//
			//  Add potentially-eligible instructions to the list
			//  because things change as we commit, we'll double-check each
			//  instruction as it comes up...
			//
			if (eligible_to_commit(rs, &reason)) {
				commit_list[thread][clist_num[thread]++] = rs;
				++num_eligible;

				if (clist_num[thread] == commit_width) {
					list_done[thread] = true;
					++num_list_done;

					if (num_list_done == number_of_threads)
						done = true;
				}
			} else {
				DynInst *inst = rs->inst;

				//
				//  An ineligible instruction means that we're done
				//  looking at this thread... determine the commit-end
				//  cause for _this_thread_ in case we need it later
				//
				if (rs->completed) {
					//
					//  For completed but not eligible instructions,
					//  we'll use the "reason" determined by the
					//  eligible_to_commit() function
					//
					reason_thread[thread] = reason;
				} else if (inst->isLoad() && !rs->eaCompPending
						&& rs->issued) {
					// It's a load that's been issued from the LSQ,
					// so it's a memory stall... (not necessarily a miss,
					// despite the name)
					reason_thread[thread] = COMMIT_DMISS;
					detail_thread[thread] = rs->mem_result;
				} else if (inst->isMemBarrier()) {
					reason_thread[thread] = COMMIT_MEMBAR;
				} else {
					// anything else: blame it on the function unit
					reason_thread[thread] = COMMIT_FU;
					detail_thread[thread] = inst->opClass();
				}

				if (clist_num[thread] == 0) {
					// Special checks when the oldest instruction on
					// the thread is not committable: it might be an
					// instruction that has to wait until this point
					// to issue.

					// Uncached loads must wait to guarantee they're
					// non-speculative.  Memory barriers must wait to
					// guarantee that all previous loads have
					// completed.  (MBs also must wait for the store
					// buffer to empty to guarantee all previous
					// stores have completed as well.)

					bool uc_load = (inst->isLoad()
							&& (inst->mem_req_flags & UNCACHEABLE));
					bool ready_mb = (inst->isMemBarrier()
							&& storebuffer->count(thread) == 0);
					if ((uc_load || ready_mb)
							&& rs->lsq_entry.notnull()
							&& !rs->lsq_entry->queued
							&& rs->lsq_entry->ops_ready()) {
						LSQ->ready_list_enqueue(rs->lsq_entry);
					}
				}

				//
				//  Make a note of the first uncommitable inst...
				//
				if ((seq_overall == 0) || (seq_overall > rs->seq)) {
					seq_overall = rs->seq;

					reason_overall = reason_thread[thread];
					blame_overall  = thread;
					detail_overall = detail_thread[thread];
				}

				list_done[thread] = true;
				++num_list_done;

				if (clist_num[thread] == 0) {
					finished_thread[thread] = true;
					++num_finished_threads;
				}

				if (num_list_done == number_of_threads)
					done = true;
			}
		}
	}

	//
	//  We'll blame the oldest uncommitted instruction by default
	//
	reason = reason_overall;
	blame  = blame_overall;
	detail = detail_overall;

	if (num_eligible == 0) {
		//
		//  Assign blame
		//
		switch(reason) {
		case COMMIT_BW:
		case COMMIT_NO_INSN:
		case COMMIT_STOREBUF:
		case COMMIT_MEMBAR:
			break;
		case COMMIT_FU:
			floss_state.commit_fu[0][0] = OpClass(detail);
			break;
		case COMMIT_DMISS:
			commit_total_mem_stall_time++;

			tmpBlockedCycles++;

			//HACK: the hit latency should be retrived from the L1 cache
			if(tmpBlockedCycles > 3) l1MissStallCycles++;

			floss_state.commit_mem_result[0] = MemAccessResult(detail);
			break;
		case COMMIT_CAUSE_NOT_SET:
			done = true;  // dummy
			break;
		default:
			fatal("commit causes screwed up");
		}
		floss_state.commit_end_cause[0] = reason;

		issueStallMessageCounter++;
		int stallDetectionDelay = 35;
		if(issueStallMessageCounter > stallDetectionDelay && !stallMessageIssued){
			stallMessageIssued = true;
			interferenceManager->setStalledForMemory(CPUParamsCpuID, stallDetectionDelay);

			stallCycleTraceCounter += stallDetectionDelay;
			noCommitCycles++;
		}

		if(stallMessageIssued){
			stallCycleTraceCounter++;
			noCommitCycles++;
		}

		//
		//  De-allocate memory
		//
		for (int i = 0; i < number_of_threads; ++i)
			if (commit_list[i])
				delete [] commit_list[i];

		return;
	}

	//
	//
	//
	if (prioritized_commit) {
		//
		//  Choose the order of threads to commit
		//

		fatal("prioritized commit isn't implemented, yet...");
	}


	//
	//  Prepare to enter the commit loop...
	//
	//  ... do commit-model specific tasks
	//

	//
	//  Choose the thread we're commiting by looking at the oldest
	//  eligible instruction
	//
	unsigned pt_thread = 0;
	if (commit_model == COMMIT_MODEL_PERTHREAD)
		pt_thread = oldest_inst(commit_list, clist_num, clist_idx);


	//
	//  Increment the RR value, looking for a thread we can commit from
	//
	if (commit_model == COMMIT_MODEL_RR) {
		do {
			rr_commit_last_thread
			= (rr_commit_last_thread +1) % number_of_threads;
		} while (finished_thread[rr_commit_last_thread]);

		//
		//  Mark all remaining threads as done...
		//
		for (int i = 0; i < number_of_threads; ++i)
			finished_thread[i] = true;

		num_finished_threads = number_of_threads - 1;
		finished_thread[rr_commit_last_thread] = false;
	}


	// entering main commit loop, reset tmp blocked cycle counter
	tmpBlockedCycles = 0;
	issueStallMessageCounter = 0;
	if(stallMessageIssued){
		stallMessageIssued = false;
		interferenceManager->clearStalledForMemory(CPUParamsCpuID);
	}

	//
	//  Main commit loop
	//
	done = false;
	do {
		ROBStation *rs = 0;
		unsigned thread = 0;

		//
		//  Choose the instruction to commit
		//
		switch(commit_model) {
		case COMMIT_MODEL_SMT:
			thread = oldest_inst(commit_list, clist_num, clist_idx);
			rs = commit_list[thread][clist_idx[thread]++];
			break;

		case COMMIT_MODEL_SSCALAR:
			thread = oldest_inst(commit_list, clist_num, clist_idx);
			rs = commit_list[thread][clist_idx[thread]++];

			// if this inst is younger than the oldest non-commitable
			// inst, we are done.
			if (rs->seq > seq_overall) {
				rs = 0;
				done = true;
				reason_thread[thread] = COMMIT_NO_INSN;
			}
			break;

		case COMMIT_MODEL_PERTHREAD:
			thread = pt_thread;
			if (clist_num[thread] - clist_idx[thread]) {
				rs = commit_list[thread][clist_idx[thread]++];
			} else {
				rs = 0;
				done = true;
				reason_thread[thread] = COMMIT_NO_INSN;
			}
			break;

		case COMMIT_MODEL_RR:
			thread = rr_commit_last_thread;
			if (clist_num[thread] - clist_idx[thread])
				rs = commit_list[thread][clist_idx[thread]++];
			else
				reason_thread[thread] = COMMIT_NO_INSN;
			break;

		default:
			fatal("commit model screwed up");
			break;
		};


		//
		//  If we have an instruction to commit, do it...
		//
		if (rs) {
			--num_eligible;

			if (eligible_to_commit(rs, &reason)) {
				if (rs->inst->spec_mode == 0) {

					commit_one_inst(rs);

					++committed;
					++committed_thread[thread];

					crash_counter = 0;
				} else {
					//
					//  It is possible for completed, mis-speculated
					//  instructions to arrive here between the time
					//  a mispredicted branch is written-back and the time
					//  the recovery event occurs.  In this case, a mis-
					//  speculated instruction would not have been
					//  squashed...  if this happens, squash it now...
					//  --> this doesn't count as a committed instruction
					//

					rs->squash();

					if (ptrace)
						ptrace->deleteInst(rs->inst);

					remove_ROB_element(rs);
				}

				//
				//  Check ending conditions
				//
				if (committed == commit_width) {
					reason = COMMIT_BW;
					blame = thread;
					done = true;
				} else if (num_eligible == 0) {
					reason = COMMIT_NO_INSN;
					blame = thread;
					done = true;
				}
			} else {
				//  We can't commit this instruction... reason is set in
				//  eligible_to_commit(), so just set thread
				blame = thread;

				// we're done with this thread
				clist_idx[thread] = clist_num[thread];
			}
		} else {
			//  use the default blame info...

			finished_thread[thread] = true;

			++num_finished_threads;

			if (num_finished_threads == number_of_threads)
				done = true;
		}


		//
		//  Check to see if we've examined all eligible instructions
		//  in this thread...
		//
		if (clist_idx[thread] == clist_num[thread]) {
			finished_thread[thread] = true;

			++num_finished_threads;

			if (num_finished_threads == number_of_threads) {
				done = true;
			}
		}

	} while (!done);


	//
	//  Assign blame
	//
	switch(reason) {
	case COMMIT_BW:
		if (total_completed > commit_width) {
			//  we want to count the number of instructions that could
			//  have committed if we hadn't run out of bandwidth
			++commit_eligible_samples;

			for (int t = 0; t < number_of_threads; ++t) {
				assert(completed[t] >= committed_thread[t]);
				unsigned uncommitted = completed[t] - committed_thread[t];
				commit_eligible[t] += uncommitted;

				commit_bwlimit_stat[t].sample(uncommitted);
			}
		}
		break;
	case COMMIT_NO_INSN:
	case COMMIT_STOREBUF:
	case COMMIT_MEMBAR:
		break;
	case COMMIT_FU:
		floss_state.commit_fu[0][0] = OpClass(detail);
		break;
	case COMMIT_DMISS:
		floss_state.commit_mem_result[0] = MemAccessResult(detail);
		break;
	case COMMIT_CAUSE_NOT_SET:
		done = true;  // dummy
		break;
	default:
		fatal("commit causes screwed up");
	}
	floss_state.commit_end_cause[0] = reason;

	//
	//  De-allocate memory
	//
	for (int i = 0; i < number_of_threads; ++i)
		if (commit_list[i])
			delete [] commit_list[i];

	n_committed_dist.sample(committed);

	if (floss_state.commit_end_cause[0] == COMMIT_CAUSE_NOT_SET) {
		// very rarely we can have a queue-full fetch problem even when
		// we committed the full B/W of instructions, or all of the
		// entries in the IQ... maybe because LSQ is full??
		floss_state.commit_end_cause[0] =
			(committed == commit_width) ? COMMIT_BW : COMMIT_NO_INSN;

		// we arbitrarily attribute these to thread 0; should be factored out
		// when interpreting results
		floss_state.commit_end_thread = 0;
	}
}

bool
FullCPU::eligible_to_commit(ROBStation *rs,
		enum CommitEndCause *reason)
{
	bool storebuf_stall = false;


	// To be ready to commit:
	//  - ROB entry must be complete
	//  - for loads/stores, LSQ entry must be complete
	//  - for stores, a store buffer entry must be available
	//  - for "leading" thread of reg-file-checking redundant pair,
	//      reg check buffer entry must be available

	if (!rs->completed)
		return false;

	if (rs->inst->isStore()) {
		storebuf_stall = storebuffer->full();

		if (*reason == COMMIT_CAUSE_NOT_SET && storebuf_stall)
			*reason = COMMIT_STOREBUF;
	}

	//
	//  If everything is OK for committing this instruction...
	//
	if (!storebuf_stall)
		return true;

	return false;
}


unsigned
FullCPU::oldest_inst(ROBStation ***clist, unsigned *cnum, unsigned *cidx)
{
	unsigned rv = 0;
	InstSeqNum oldest_seq = 0;
	bool not_set = true;

	for (int t = 0; t < number_of_threads; ++t) {
		//
		//  Look at this thread if:
		//    (1)  The thread has a commit list
		//    (2)  There are still instructions in the list
		//
		if (clist[t] != 0 && cnum[t] > 0 && cidx[t] < cnum[t]
		                                                   && (oldest_seq > clist[t][cidx[t]]->seq || not_set))
		{
			rv = t;
			oldest_seq = clist[t][cidx[t]]->seq;

			not_set = false;
		}
	}

	return rv;
}


void
FullCPU::commit_one_inst(ROBStation *rs)
{
	DynInst *inst = rs->inst;
	bool store_inst = false;
	unsigned thread = rs->thread_number;

	//
	// Stores: commit to store buffer if an entry is available.
	// Skip stores that faulted and write prefetches that didn't
	// translate to a valid physical address..
	//
	if (inst->isStore() && inst->fault == No_Fault &&
			!(inst->isDataPrefetch() &&
					inst->phys_eff_addr == MemReq::inval_addr)) {

		assert(inst->phys_eff_addr != MemReq::inval_addr);

		if (inst->isCopy()) {
			storebuffer->addCopy(thread, inst->asid,
					dcacheInterface->getBlockSize(), inst->xc,
					inst->eff_addr, inst->phys_eff_addr,
					inst->copySrcEffAddr,
					inst->copySrcPhysEffAddr,
					inst->mem_req_flags,
					inst->PC, rs->seq,
					inst->fetch_seq, rs->queue_num);
		} else {
			storebuffer->add(thread, inst->asid, inst->store_size,
					inst->store_data,
					inst->xc,
					inst->eff_addr, inst->phys_eff_addr,
					inst->mem_req_flags,
					inst->PC, rs->seq,
					inst->fetch_seq, rs->queue_num);
		}
		// remember to remove LSQ entry
		store_inst = true;

		//  check for bogus store size
		assert(inst->store_size <= 64);
	}

	if (rs->inst->isWriteBarrier()) {
		storebuffer->addWriteBarrier(thread);
	}

	// Faulting instruction: we are holding off dispatch of the fault
	// handler waiting for this to commit.  Notify dispatch that we've
	// committed the instruction so it can continue.
	if (inst->fault != No_Fault) {
		assert(fetch_fault_count[thread] == 1);
		new FaultHandlerDelayEvent(this, thread,
				curTick + cycles(fault_handler_delay));
	}

	// if we're committing a branch, update predictor state...
	// if we're using leading-thread prediction, put the
	// outcome in the queue too
	if (rs->inst->isControl()) {
		branch_pred->update(thread, inst->PC, inst->Next_PC,
				inst->Next_PC != inst->PC + sizeof(MachInst),
				/* pred taken? */
				inst->Pred_PC != inst->PC + sizeof(MachInst),
				/* correct pred? */
				inst->Pred_PC == inst->Next_PC,
				rs->inst->staticInst, &inst->dir_update);
	}

	thread_info[thread].commit_counter++;

	// track last committed PC for sampling stats
	commitPC[thread] = inst->PC;

	traceFunctions(inst->PC);

	update_com_inst_stats(inst);

	// invalidate ROB operation instance
	rs->tag++;

	if (DTRACE(Pipeline)) {
		string s;
		inst->dump(s);
		DPRINTF(Pipeline, "Commit %s\n", s);
	}

	//
	//  Special Handling: When instruction commits
	//  before branch recovery is done...
	//
	//  We need to tell the event handler not to try
	//  to update the now non-existant ROB entry.
	//
	//  Note that we're OK if there is no event here,
	//  as long as there is _some_ event pending
	//
	if (rs->inst->recover_inst) {
		assert ((rs->recovery_event != NULL) ||
				thread_info[thread].recovery_event_pending);

		if (rs->recovery_event) {
			//  This recovery event will still happen...
			//  we just have to tell it that it doesn't need to worry
			//  about updating this ROB entry
			rs->recovery_event->invalidate_branch_entry();
			rs->recovery_event = 0;  // to make remove_ROB_entry() happy
		}
	}

	//
	//  Store Instructions: Remove LSQ portion of store
	//
	if (store_inst)
		LSQ->squash(rs->lsq_entry);

	if (ptrace) {
		ptrace->moveInst(rs->inst, PipeTrace::Commit, 0, 0, 0);
		ptrace->deleteInst(rs->inst);
	}

	// update head entry of IQ
	remove_ROB_element(rs);

	//
	// check for instruction-count-based events
	//

	/**
	 *@todo com_inst is used as a Stat && in other ways, like here. needs fix
	 *in case com_inst becomes binned...
	 */
	comInstEventQueue[thread]->serviceEvents(com_inst[thread]);
	comLoadEventQueue[thread]->serviceEvents(com_loads[thread]);
}


void
FullCPU::update_com_inst_stats(DynInst *inst)
{
	unsigned thread = inst->thread_number;

	//
	//  Pick off the software prefetches
	//
#ifdef TARGET_ALPHA
	if (inst->isDataPrefetch()) {
		stat_com_swp[thread]++;
	} else {
		com_inst[thread]++;
		// profiling only supports one thread per core
		assert(thread == 0);
		commitedInstructionSample++;
		stat_com_inst[thread]++;
		amha->coreCommittedInstruction(CPUParamsCpuID);
		committedSinceLast++;

		committedTraceCounter++;
		if(committedTraceCounter == IPC_TRACE_FREQUENCY){
			Tick ticksInSample = curTick - lastDumpTick;
			double ipc = (double) IPC_TRACE_FREQUENCY / (double) ticksInSample;

			vector<RequestTraceEntry> data;
			assert(thread == 0);
			data.push_back(stat_com_inst[0].value());
			data.push_back(ipc);
			committedInstTrace.addTrace(data);

			interferenceManager->doCommitTrace(CPUParamsCpuID, IPC_TRACE_FREQUENCY, stallCycleTraceCounter, ticksInSample);
			stallCycleTraceCounter = 0;

			lastDumpTick = curTick;
			committedTraceCounter = 0;
		}
		assert(committedTraceCounter <= IPC_TRACE_FREQUENCY);
	}
#else
	fatal("IPC profiling for non-alpha not implemented");
	com_inst[thread]++;
	stat_com_inst[thread]++;
#endif

	//
	//  Control Instructions
	//
	if (inst->isControl())
		stat_com_branches[thread]++;

	//
	//  Memory references
	//
	if (inst->isMemRef()) {
		stat_com_refs[thread]++;

		if (inst->isLoad()) {
			com_loads[thread]++;
			stat_com_loads[thread]++;
		}
	}

	if (inst->isMemBarrier()) {
		stat_com_membars[thread]++;
	}

	if(stat_com_inst[thread].value() == minInstructionsAllCPUs && !hasDumpedStats){
		canExit = true;
		hasDumpedStats = true;

		ofstream statDumpFile(statsOrderFileName.c_str(), ios::app);
		statDumpFile << curTick << ";" << name() << ";" << CPUParamsCpuID << ";" << stat_com_inst[thread].value() << "\n";
		statDumpFile.flush();
		statDumpFile.close();


		if(issueExitEvent()){
			new SimExitEvent("all CPUs have reached their instruction limit");
		}
		else{
			Stats::SetupEvent(Stats::Dump, curTick);
		}
	}

	if(stat_com_inst[thread].value() == minInstructionsAllCPUs && CPUParamsCpuID == quitOnCPUID){
		new SimExitEvent("The indicated cpu has committed the required number of instructions");
	}
}

// register commit-stage statistics
void
FullCPU::commitRegStats()
{
	using namespace Stats;

	n_committed_dist
	.init(0,commit_width,1)
	.name(name() + ".COM:committed_per_cycle")
	.desc("Number of insts commited each cycle")
	.flags(pdf)
	;

	//
	//  Commit-Eligible instructions...
	//
	//  -> The number of instructions eligible to commit in those
	//  cycles where we reached our commit BW limit (less the number
	//  actually committed)
	//
	//  -> The average value is computed over ALL CYCLES... not just
	//  the BW limited cycles
	//
	//  -> The standard deviation is computed only over cycles where
	//  we reached the BW limit
	//
	commit_eligible
	.init(number_of_threads)
	.name(name() + ".COM:bw_limited")
	.desc("number of insts not committed due to BW limits")
	.flags(total)
	;

	commit_eligible_samples
	.name(name() + ".COM:bw_lim_events")
	.desc("number cycles where commit BW limit reached")
	;

	commit_bwlimit_stat
	.init(number_of_threads)
	.name(name() + ".COM:bw_lim_stdev")
	.desc("standard deviation of bw_lim_avg value")
	.precision(4)
	.flags(total)
	;

	// Magnus
	commit_total_mem_stall_time
	.name(name() + ".COM:total_ticks_stalled_for_memory")
	.desc("Number of ticks the processor was stalled due to memory")
	;

	commit_cycles_empty_ROB
	.name(name() + ".COM:commit_cycles_empty_ROB")
	.desc("Number of ticks the processor could not commit instructions because the ROB was empty")
	;

}

void
FullCPU::commitRegFormulas()
{
	using namespace Stats;

	bw_lim_avg
	.name(name() + ".COM:bw_lim_avg")
	.desc("Avg number not committed in cycles BW limited")
	.precision(4)
	.flags(total)
	;
	bw_lim_avg = commit_eligible / commit_eligible_samples;

	bw_lim_rate
	.name(name() + ".COM:bw_lim_rate")
	.desc("Average number not committed due to BW (over all cycles)")
	.precision(4)
	.flags(total)
	;
	bw_lim_rate = commit_eligible / numCycles;
}
