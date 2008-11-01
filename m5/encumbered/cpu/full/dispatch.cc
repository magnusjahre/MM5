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

#include <algorithm>

#include "base/cprintf.hh"
#include "base/predictor.hh"
#include "base/statistics.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/create_vector.hh"
#include "encumbered/cpu/full/dd_queue.hh"
#include "encumbered/cpu/full/dep_link.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/fetch.hh"
#include "encumbered/cpu/full/floss_reasons.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/iq/segmented/chain_info.hh"
#include "encumbered/cpu/full/iq/segmented/chain_wire.hh"
#include "encumbered/cpu/full/readyq.hh"
#include "encumbered/cpu/full/reg_info.hh"
#include "encumbered/cpu/full/rob_station.hh"
#include "encumbered/cpu/full/spec_state.hh"
#include "sim/stats.hh"


using namespace std;

extern const char *dispatch_policy_strings[];

//
//  Debugging break point
//
InstSeqNum dispatch_break = 0;
void
dispatch_breakpoint()
{
    cprintf("got to the DISP break_point!\n");
}


#define MODULO_VAL 4

//  Setting this to 1 will cause instructions w/ no pending ideps
//  to become chain heads (only if it produces a result)
#define CHAIN_HEAD_IND_INSTS 0

#define DUMP_CHAIN_INFO 0

//  Setting this will cause the contents of IQ[0] to be dumped every dispatch cycle
#define DUMP_IQ 0

//
//  For statistics...
//
const char *chain_cr_class_desc[] = {
    "     Inst has no outstanding IDEPS",
    "      IDEP chain reached max depth",
    "                    Inst is a load",
    "  Chain has multiple chained IDEPS"
};

enum DispatchInstClass{
    INSN_CLASS_ALL_RDY=0,       // All operands ready
    INSN_CLASS_ONE_NOT_RDY,     // All but one operand ready
    INSN_CLASS_MULT_CHAINS,     // None ready
    INSN_CLASS_ONE_CHAINED,     // None ready
    INSN_CLASS_ALL_SELF_TIMED,  // None ready
    NUM_INSN_CLASSES
};

const char *dispatchInstClassDesc[] = {
    "             All ops ready",
    "          One op not ready",
    "     None rdy, mult chains",
    "     None rdy, one chained",
    "  None rdy, all self-timed"
};



//
//  THE BIG PICTURE:
//
//  Move instructions from the fetch queue into the decode/dispatch queue.
//  Operation of the decodeQueue is "backwards" ("dispatch" end of pipe first)
//  so that we can grab instructions out of the tail end (dispatch end) of the
//  pipe during the same cycle we place insts into the head (decode end) of
//  the pipe.
//


/*

Notes regarding dispatch stage operation:

---

For the MULTI-THREADED Front-End:

 (1) Pick a destination IQ for each thread. If a thread can not dispatch
     to that IQ, then it can't dispatch.
       - Static allocation:   dest_iq = thread_number % numIQueues
       - Modulo-N allocation: dest_iq = ++dest_iq % numIQueues
                              (Only if dispatch_count % N is zero)
			      ==> Switch to the next IQ every N instructions,
			          but stop dispatch if select IQ is full.

 (2) Develop a "score" for each thread. The thread with the *largest* score
     will be selected for dispatch. The following situations can block
     dispatch for a thread. This is indicated to the final choice code by
     setting the score to INT_MIN (from <limits.h>):
       - IQ has met it's cap
       - ROB has met it's cap
       - Insufficient IQ slots
       - Insufficient LSQ slots
       - Insufficient ROB slots
       - Insufficient physical registers

 (3) Depending upon fetch policy, the following modifications are made:
       ICOUNT: the sum of the count of instructions in the IQ plus any
               bias value is *subtracted* from the score
       All Others: the number of cycles since a thread's last dispatch is
                   *added* to the score

 (4) Select the thread with the smallest score to dispatch.


For the SINGLE-THREADED Front-End:

  This is the same as for the MT Front-End, except we skip step (1).

---

Once a thread is chosen for dispatch, we know it is _possible_ for all
instructions in the packet to dispatch. But we also know that individual
instructions may not dispatch due to "objections" from the IQ, LSQ, or ROB.

==> If any of the instructions in the packet do not dispatch, then that pipe-
    stage DOES NOT advance (since there is still an inst in the last stage).

NOP's and squashed instructions are dropped, but do count against the dispatch
bandwidth.

Since it is possible that an instruction will not dispatch, we need to peek
into the dec_disp_queue, do the dispatch, and if the dispatch succeeds, only
then remove the instruction from the queue.



*/

//////////////////////////////////////////////////////////////
//
//  Determine which IQ this instruction should use
//
//////////////////////////////////////////////////////////////
int
FullCPU::choose_iqueue(unsigned thread)
{
    int iq_idx = 0;

    if (numIQueues > 1) {
	//
	//  Initial choice of IQ for this instruction
	//
	switch (dispatch_policy) {
	  case DEPENDENCE:
	    //
	    //  Access the register-info file and choose the destination
	    //  cluster based on dependence information for each instruction
	    //
	    //  that means that doing it here (by thread) makes no sense
	    //
	    break;
	  case MODULO_N:
	    //  MODULO-N algorithm puts 'n' inst blocks into the same
	    //  queue.
	    //
	    //  if the "correct" queue doesn't have enough spots, stall
	    //
	    iq_idx = mod_n_queue_idx;
	    break;

	  case THREAD_PER_QUEUE:
	    //  Allocate one thread per queue (wrap if there aren't
	    //  enough queues) --> static partitioning
	    iq_idx = thread % numIQueues;
	    break;

	  default:
	    panic("dispatch stage misconfigured");
	    break;
	}
    }

    return iq_idx;
}

//
//  This structure holds the information we need for each thread to
//  rank them for dispatching
//
struct ThreadList
{
    Tick score;
    Tick lastDispatchTime;
    unsigned thread_number;
    unsigned iq_idx;
    unsigned disp_insts;
    bool eligable;

    ThreadList() {
	score = 0;
	eligable = false;
    }
};

class ThreadListSortComp
{
  public:
    bool operator()(const ThreadList &l1, const ThreadList &l2) const {
	if (l1.eligable && !l2.eligable)
	    return true;  // we want l1 to go before (lower) than l2

	if (!l1.eligable && l2.eligable)
	    return false;   // we want l1 to go after (highter) than l2

	if (l1.score == l2.score) {
	    //  RR operation for these two threads...
	    return l1.lastDispatchTime < l2.lastDispatchTime;
	}

	return l1.score > l2.score;  // hopefully an ASCENDING sort
    }
};


//
//  Returns true if this cluster is good for dispatch
//
bool
FullCPU::checkClusterForDispatch(unsigned clust, bool chainHead) {
    bool rv = (IQ[clust]->free_slots() != 0);

    if (rv && chainHead)
	rv = (chainWires->freeWires(clust) != 0);

    return rv;
}


DispatchEndCause
FullCPU::checkThreadForDispatch(unsigned t, unsigned idx, unsigned needSlots)
{
    //  Check for free slots in the IQ
    if (IQ[idx]->free_slots() < needSlots) {
	IQ[idx]->markFull();
	return FLOSS_DIS_IQ_FULL;
    }

    //  Check for IQ cap
    if (IQ[idx]->cap_met(t)) {
	iq_cap_events[t]++;
	iq_cap_inst_count[t] += ifq[t].num_total();

	//  Indicate to fetch stage that the cap is active
	iq_cap_active[t] = true;
	return FLOSS_DIS_IQ_CAP;
    }

    //  Check for ROB Caps
    if ((rob_cap[t] < ROB_size) && (ROB.num_thread(t) >= rob_cap[t])) {
	rob_cap_events[t]++;
	rob_cap_inst_count[t] += ifq[t].num_total();

	//  Indicate to fetch stage that the cap is active
	rob_cap_active[t] = true;
	return FLOSS_DIS_ROB_CAP;
    }

    //
    //  Check for available IQ bandwidth
    //
    if (IQ[idx]->add_bw() < needSlots)
	return FLOSS_DIS_BW;

    DispatchEndCause c = checkGlobalResourcesForDispatch(needSlots);
    if (c != FLOSS_DIS_CAUSE_NOT_SET)
	return c;

    return FLOSS_DIS_CAUSE_NOT_SET;
}


DispatchEndCause
FullCPU::checkGlobalResourcesForDispatch(unsigned needSlots)
{
    //  Check for free slots in the LSQ
    if (LSQ->free_slots() < needSlots) {
	++lsq_fcount;
	return FLOSS_DIS_LSQ_FULL;
    }

    //  Check for free slots in the ROB
    if (ROB.num_free() < needSlots) {
	++ROB_fcount;
	return FLOSS_DIS_ROB_FULL;
    }

    //  Check for sufficient INT physical registers
    if (free_int_physical_regs < needSlots) {
	++reg_int_full;
	return FLOSS_DIS_IREG_FULL;
    }

    //  Check for sufficient FP physical registers
    if (free_fp_physical_regs < needSlots) {
	++reg_fp_full;
	return FLOSS_DIS_FPREG_FULL;
    }

    return FLOSS_DIS_CAUSE_NOT_SET;
}



//============================================================================
//
//  This dispatch stage itself:
//    - Determine the (first) IQ that each thread should dispatch to
//    - Calculate a score for each thread
//    - Sort by score -- high score gets a chance to dispatch first
//    - Dispatch thread(s) to IQ(s) as appropriate for IQ configuration and
//         dispatch policy
//
void
FullCPU::dispatch()
{
    DispatchEndCause &endCause = floss_state.dispatch_end_cause;
    endCause = FLOSS_DIS_CAUSE_NOT_SET;

    //  We will sort this vector to determine which thread to attempt
    //  to dispatch first, second, etc...
    vector<ThreadList> tlist(SMT_MAX_THREADS);


    //
    //  bail early if no instructions to dispatch
    //
    if (decodeQueue->instsAvailable() == 0) {
	endCause = FLOSS_DIS_NO_INSN;
	return;
    }

    endCause = checkGlobalResourcesForDispatch(1);
    if (endCause != FLOSS_DIS_CAUSE_NOT_SET)
	return;

    m5_assert(chainWires == 0 || chainWires->sanityCheckOK());
    m5_assert(clusterSharedInfo->ci_table == 0 ||
	      clusterSharedInfo->ci_table->sanityCheckOK());


    //
    //  For each thread:
    //    - Populate ThreadList entry
    //    - Check for dispatch-able instrucitons
    //    - Calculate score
    //
    for (int t = 0; t < SMT_MAX_THREADS; ++t) {
	int idx = choose_iqueue(t);

	if (idx < 0) {
	    //  no available clusters
	    tlist[t].eligable = false;

	    SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_POLICY);

	    continue;   // try next thread
	}

	tlist[t].iq_idx = idx;
	tlist[t].thread_number = t;
	tlist[t].lastDispatchTime = lastDispatchTime[t];

	//  how many insturctions can we possibly dispatch?
	tlist[t].disp_insts = decodeQueue->instsAvailable(t);
	if (tlist[t].disp_insts > dispatch_width)
	    tlist[t].disp_insts = dispatch_width;

	if (tlist[t].disp_insts == 0) {
	    tlist[t].eligable = false;

	    SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_NO_INSN);

	    continue;   // try next thread
	}

	//
	//  modify the score based on the fetch policy
	//
	unsigned adj = 0;
	switch (fetch_policy) {
	  case IC:
	    //  The number of slots available to this thread without regard
	    //  to the cap...
	    tlist[t].score += IQNumSlots;

	    //
	    //  adjust the score by the number of instructions in the IQ
	    //  ==> Be careful not to underflow the unsigned score value
	    //
	    adj = IQNumInstructions(t) + static_icount_bias[t];

	    if (adj > tlist[t].score)
		tlist[t].score = 0;
	    else
		tlist[t].score -= adj;

	    break;
	  default:
	    //  The number of cycles since this thread last dispatched
	    tlist[t].score += (curTick - lastDispatchTime[t]);
	    break;
	}

	tlist[t].eligable = true;
    }

    //
    //  Now that the scores have been calculated... sort threads...
    //
    sort(tlist.begin(), tlist.end(), ThreadListSortComp());

    //
    //  If the first element isn't going to dispatch, none are...
    //  --> bail out early
    //
    if (tlist[0].eligable) {
	//  reset the end cause... we've got something to dispatch...
	endCause = FLOSS_DIS_CAUSE_NOT_SET;
    } else {
	// The only possible floss cause here is NO_INSN
	return;
    }


    //---------------------------------------------------------------------
    //
    //  We're finally ready to start dispatching. If there is only one
    //  IQ, then we simply dispatch the thread with the highest score to
    //  the sole IQ.
    //
    //  The clustered architecture is slightly more interesting...
    //
    unsigned dispatched_this_cycle = 0;

    if (numIQueues == 1) {
	//
	//  Non-clustered architecture:
	//
	for (int i = 0; i < number_of_threads; ++i) {
	    //  early exit...
	    if (!tlist[i].eligable)
		break;

	    unsigned thread = tlist[i].thread_number;
	    unsigned count = dispatch_thread(thread, 0, 0, endCause);

	    if (endCause == FLOSS_DIS_CAUSE_NOT_SET && count == dispatch_width)
		endCause = FLOSS_DIS_BW;

	    if (count) {
		lastDispatchTime[thread] = curTick;
		// exit after we dispatch from a thread
		break;
	    }
	}
#if DUMP_IQ
	IQ[0]->dump();
#endif
	return;
    }


    //------------------------------------------------------------------------
    //
    //  Clustered machine...
    //
    //   Dispatch behavior depends on dispatch policy
    //
    unsigned iq_idx = tlist[0].iq_idx;
    unsigned thread = tlist[0].thread_number;
    bool done = false;


    switch (dispatch_policy) {
      case DEPENDENCE:
	//
	//  We dispatch A SINGLE thread to as many IQ's as necessary.
	//
	//  rotate through all the IQ's until:
	//    (1) We run out of instructions to dispatch
	//    (2) We try to dispatch to an IQ, and fail
	//
	//  ==> This means that we have to check each IQ for caps, etc
	//      as we rotate through...
	//
	lastDispatchTime[thread] = curTick;

	do {
	    DispatchEndCause queue_endCause = FLOSS_DIS_CAUSE_NOT_SET;

	    //
	    //  Logic internal to dispatch_thread() will direct instructions
	    //  to the appropriate instruction queues
	    //
	    unsigned dispatched_this_queue =
		dispatch_thread(thread, iq_idx, dispatch_width,
				queue_endCause);

	    dispatched_this_cycle += dispatched_this_queue;

	    switch( queue_endCause ) {
		//
		// The following end-causes indicate that we can't dispatch
		// any more instructions this cycle
		//
	      case FLOSS_DIS_ROB_FULL:
	      case FLOSS_DIS_LSQ_FULL:
	      case FLOSS_DIS_IREG_FULL:
	      case FLOSS_DIS_FPREG_FULL:
		done = true;
		endCause = queue_endCause;
		break;

		//
		// The following end-causes indicate that we can't continue
		// dispatching this thread this cycle
		//
	      case FLOSS_DIS_ROB_CAP:
	      case FLOSS_DIS_NO_INSN:
		done = true;
		endCause = queue_endCause;
		break;

		//
		// The following end-causes indicate that we can't dispatch
		// the next instruction, so we should give up now...
		//
	      case FLOSS_DIS_IQ_FULL:
	      case FLOSS_DIS_IQ_CAP:
	      case FLOSS_DIS_BW:
		done = true;
		break;

		//
		//  If we run out of chains...
		//
	      case FLOSS_DIS_POLICY:
		done = true;
		break;

	      case FLOSS_DIS_CAUSE_NOT_SET:
		done = false;
		break;

	      default:
		warn("need to adjust endCauses for dispatch_thread()");
		done = false;
	    }

	    if (queue_endCause == FLOSS_DIS_CAUSE_NOT_SET) {
		if (decodeQueue->instsAvailable(thread) == 0) {
		    SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_NO_INSN);
		    done = true; // nothing left to dispatch
		}
	    }

	    if (dispatched_this_cycle == dispatch_width) {
		SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_BW);
		done = true;  // we used all available BW
	    }

	    if (done)
		SET_FIRST_FLOSS_CAUSE(endCause, queue_endCause);

	} while (!done);
	assert(endCause != FLOSS_DIS_CAUSE_NOT_SET);
	break;

      case MODULO_N:
	//
	//  We dispatch A SINGLE thread to as many IQ's as necessary.
	//
	//  rotate through all the IQ's until:
	//    (1) We run out of instructions to dispatch
	//    (2) We try to dispatch to an IQ, and fail
	//
	//  ==> This means that we have to check each IQ for caps, etc
	//      as we rotate through...
	//
	lastDispatchTime[thread] = curTick;

	do {
	    DispatchEndCause queue_endCause = FLOSS_DIS_CAUSE_NOT_SET;
	    unsigned dispatched_this_queue =
		dispatch_thread(thread, iq_idx, dispatch_width,
				queue_endCause);

	    dispatched_this_cycle += dispatched_this_queue;

	    switch (queue_endCause) {
		//
		// The following end-causes indicate that we can't dispatch
		// any more instructions this cycle
		//
	      case FLOSS_DIS_ROB_FULL:
	      case FLOSS_DIS_LSQ_FULL:
	      case FLOSS_DIS_IREG_FULL:
	      case FLOSS_DIS_FPREG_FULL:
		done = true;
		endCause = queue_endCause;
		break;

		//
		// The following end-causes indicate that we can't continue
		// dispatching this thread this cycle
		//
	      case FLOSS_DIS_ROB_CAP:
	      case FLOSS_DIS_NO_INSN:
		done = true;
		endCause = queue_endCause;
		break;

		//
		// The following end-causes indicate that we can't continue
		// dispatching to this Queue, but should try the next one
		//
	      case FLOSS_DIS_IQ_FULL:
	      case FLOSS_DIS_IQ_CAP:
	      case FLOSS_DIS_BW:
	      case FLOSS_DIS_POLICY:
		endCause = queue_endCause;
		//		done = false;
		done = true;
		break;

	      case FLOSS_DIS_CAUSE_NOT_SET:
		done = false;
		break;

	      default:
		warn("need to adjust endCauses for dispatch_thread()");
		done = false;
	    }

	    if (queue_endCause == FLOSS_DIS_CAUSE_NOT_SET) {
		if (decodeQueue->instsAvailable(thread) == 0) {
		    SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_NO_INSN);
		    done = true; // nothing left to dispatch
		}
	    }

	    if (dispatched_this_cycle == dispatch_width) {
		SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_BW);
		done = true;  // we used all available BW
	    }

	    if (!done) {
#if 0
		//  Rotate to the next IQ
		iq_idx = choose_iqueue(thread);

		if ((iq_idx < 0) || (iq_idx == first)) {
		    done = true;

		    // we call this policy...
		    SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_POLICY);
		}
#endif
	    } else
		SET_FIRST_FLOSS_CAUSE(endCause, queue_endCause);
	} while (!done);
	assert(endCause != FLOSS_DIS_CAUSE_NOT_SET);
	break;

      case THREAD_PER_QUEUE:
	//
	//  Walk the sorted list of threads, dispatching as many insts
	//  from each as possible.
	//
	for (unsigned t = 0; t < SMT_MAX_THREADS; ++t) {
	    if (tlist[t].eligable) {
		unsigned dispatched_this_thread = 0;
		unsigned iq_idx = tlist[t].iq_idx;
		unsigned thread = tlist[t].thread_number;

		unsigned possible_dispatches =
		    dispatch_width - dispatched_this_cycle;

		dispatched_this_thread =
		    dispatch_thread(thread, iq_idx, possible_dispatches,
				    endCause);

		dispatched_this_cycle += dispatched_this_thread;

		bool done;
		switch (endCause) {
		    //
		    // The following end-causes indicate that we can't dispatch
		    // any more instructions this cycle
		    //
		  case FLOSS_DIS_ROB_FULL:
		  case FLOSS_DIS_LSQ_FULL:
		  case FLOSS_DIS_IREG_FULL:
		  case FLOSS_DIS_FPREG_FULL:
		    done = true;  // stop dispatching
		    break;

		    //
		    // The following end-causes indicate that we can't
		    // continue dispatching this thread this cycle
		    //
		  case FLOSS_DIS_ROB_CAP:
		  case FLOSS_DIS_NO_INSN:
		  case FLOSS_DIS_POLICY:
		    done = false; // continue dispatching
		    break;

		    //
		    // The following end-causes indicate that we can't continue
		    // dispatching to this Queue, but should try the next one
		    //
		  case FLOSS_DIS_IQ_FULL:
		  case FLOSS_DIS_IQ_CAP:
		  case FLOSS_DIS_BW:
		    done = false; // continue dispatching
		    break;

		  case FLOSS_DIS_CAUSE_NOT_SET:
		    done = false;
		    break;

		  default:
		    warn("need to adjust endCauses for dispatch_thread()");
		    done = false;
		}

		if (done)
		    break;

		if (dispatched_this_cycle == dispatch_width) {
		    SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_BW);
		    break;  // we used _all_ available BW
		}

		//
		//  If we dispatched all available instructions
		//  for this thread, and the "next" thread is going
		//  to try to dispatch, then reset the endCause
		//
		if (dispatched_this_thread == tlist[t].disp_insts &&
		    t < (SMT_MAX_THREADS - 1) && tlist[t + 1].eligable)
		    endCause = FLOSS_DIS_CAUSE_NOT_SET;
	    } else {
		//  endCause will have been set earlier, when we assigned
		//  scores
		break;  // no other threads will be eligible
	    }
	}
	break;
    }

    //
    //  Anything that we didn't set a cause for before this point, MUST
    //  be a thread that we _could_ have dispatched, but chose not to
    //
    if (floss_state.dispatch_end_cause == FLOSS_DIS_CAUSE_NOT_SET)
	floss_state.dispatch_end_cause = FLOSS_DIS_BROKEN;

#if DUMP_IQ
    IQ[0]->dump(0);
#endif
}


//
//  Return the desired cluster
//
unsigned
FullCPU::choose_dependence_cluster(DynInst *inst)
{
    unsigned thread = inst->thread_number;
    RegInfoTable * rit = clusterSharedInfo->ri_table;


    bool chained = false;
    Tick pred_ready = 0;
    unsigned pred_cluster = IQLeastFull();  // default if no chained idep

    for (int i = 0; i < inst->numSrcRegs(); ++i) {
	unsigned ireg = inst->srcRegIdx(i);

	//
	//  If we have an input register that is not yet ready and is chained
	//
	chained = (*rit)[thread][ireg].isChained();
	if (create_vector[thread].entry(ireg).rs != 0) {
	    if ((*rit)[thread][ireg].predReady() > pred_ready) {
		pred_ready = (*rit)[thread][ireg].predReady();
		pred_cluster = (*rit)[thread][ireg].cluster();
	    }
	} else {
	    // sanity check: if the input reg does not have a creator,
	    // it had better not be chained...
	    assert(!chained);
	}
    }

    return pred_cluster;
}

//
//  Dispatch the next 'max' instructions for thread number 'thread' to
//  Instruction queue 'iq_idx'.
//
//  Returns:
//     - The number of instructions actually dispatched
//       (squashed instructions _do_ count)
//     - The reason we stopped dispatching
//       (this value will NOT be set if we stop due to reaching 'max')
//
//  Dispatch End Causes Returned:
//     FLOSS_DIS_NO_INSN   --> No more instructions to dispatch for this thread
//     FLOSS_DIS_BW        --> No BW left for this queue
//
//     FLOSS_DIS_IQ_FULL     {via checkThreadForDispatch()}
//     FLOSS_DIS_IQ_CAP      {via checkThreadForDispatch()}
//     FLOSS_DIS_LSQ_FULL    {via checkThreadForDispatch()}
//     FLOSS_DIS_ROB_FULL    {via checkThreadForDispatch()}
//     FLOSS_DIS_ROB_CAP     {via checkThreadForDispatch()}
//     FLOSS_DIS_IREG_FULL   {via checkThreadForDispatch()}
//     FLOSS_DIS_FPREG_FULL  {via checkThreadForDispatch()}
//
unsigned
FullCPU::dispatch_thread(unsigned thread, unsigned iq_idx, unsigned max,
		     DispatchEndCause &endCause)
{
    ROBStation *rob;
    unsigned dCount = 0;

    unsigned first_idx;
    bool using_loose_mod_n = false;

    DPRINTF(Pipeline, "DISP:     dispatch_thread (thread %d) (clust %d)\n", thread, iq_idx);

    //  Default case... no maximum value
    if (max == 0) {
	max = dispatch_width;
    }

    first_idx = iq_idx;


    //
    //  Dispatch until:
    //    (1) An instruction fails to dispatch
    //    (2) We dispatch the "max" number of instructions
    //    (3) We run out of decodeQueue bandwidth
    //
    do {
	DynInst * inst = decodeQueue->peek(thread);

	//
	//  If this inst hasn't been squashed...
	//
	if (inst != 0) {

	    // If it's a serializing instruction, flush the pipeline
	    // by stalling here until all older instructions have
	    // committed.  Do this before checking for No_OpClass,
	    // since instructions that only serialize (e.g. Alpha
	    // trapb) will have No_OpClass and thus get discarded
	    // before being dispatched once the serialization is
	    // complete.
	    if (inst->isSerializing() && ROB.num_thread(thread) != 0) {
		SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_SERIALIZING);
		++dispatch_serialize_stall_cycles[thread];
		break;
	    } else if (inst->fault == No_Fault
		       && (inst->opClass() == No_OpClass || inst->isNop()
			   || inst->isInstPrefetch())) {
		// Drop things that don't need to go any farther in the
		// pipeline.  This includes all instructions that do not
		// use a function unit (i.e. have No_OpClass for their op
		// class).  This should include all instructions that
		// declare themselves as no-ops (isNop()), but just to be
		// sure we'll check that too.  We're also dropping
		// instruction prefetches here for historical reasons (in
		// practice we don't have any, I believe).
		if (ptrace) {
		    ptrace->moveInst(inst, PipeTrace::Dispatch, 0, 0, 0);
		    ptrace->deleteInst(inst);
		}

		// Copied this here from dispatch_one_inst() since
		// insts that get discarded here never make it to that
		// function.  Causes minor inconsistencies since these
		// don't get counted in some other stats, but oh well.
		if (inst->isSerializing())
		    ++dispatched_serializing[thread];

		delete inst;
		inst = 0;

		// not going to need this one...
		decodeQueue->remove(thread);
		++dCount;
	    } else {
		DispatchEndCause cause;
		unsigned output_reg = 0;
		bool output_reg_valid = false;

		if (numIQueues == 1) {
		    cause = checkThreadForDispatch(thread, iq_idx, 1);
		} else {
		    //
		    //  Some dispatch policies require decisions to be made
		    //  for each instruction...
		    //
		    switch (dispatch_policy) {
		      case DEPENDENCE:
			//
			//  Find the output register
			//  (--> assumes only one output register per
			//  instruction)
			//
			for (int i = 0; i < inst->numDestRegs(); ++i) {
			    output_reg = inst->destRegIdx(i);
			    output_reg_valid = true;
			    break;
			}

			//
			//  Now pick the Cluster...
			//
			iq_idx = choose_dependence_cluster(inst);

			//
			//  Check for available resources
			//
			cause = checkThreadForDispatch(thread, iq_idx, 1);

			//
			//  Just because we can't put the instruction where
			//  we want it doesn't mean that we shouldn't dispatch
			//  it...
			//
			if (cause == FLOSS_DIS_BW) {
			    //  No IQ bandwidth for our preferred IQ...
			    //  --> try to dispatch to the least full IQ, but
			    //      if that also fails, blame the original IQ
			    //      BW problem

			    unsigned new_idx = IQLeastFull();

			    DispatchEndCause new_cause =
				checkThreadForDispatch(thread, new_idx, 1);

			    if (new_cause == FLOSS_DIS_CAUSE_NOT_SET) {
				// go with the new IQ
				cause = new_cause;
				iq_idx = new_idx;
			    }
			}
			break;


		      case MODULO_N:
			cause = checkThreadForDispatch(thread, iq_idx, 1);
			if (cause == FLOSS_DIS_IQ_FULL ||
			    cause == FLOSS_DIS_IQ_CAP ||
			    cause == FLOSS_DIS_BW)
			{
			    DispatchEndCause temp = cause;
			    unsigned n = IQFreeSlotsX(iq_idx);
			    if (n) {
				++mod_n_disp_stalls[thread];
				mod_n_disp_stall_free[thread] += n;

				//
				//  Loose Mod-N:
				//
				//  if we'd normally just quit, try to find a
				//  different cluster for these instructions
				//
				if (loose_mod_n_policy) {
				    do {
					iq_idx = (iq_idx+1) % numIQueues;
					if (iq_idx == first_idx) {
					    cause = temp;
					    break;
					}

					cause = checkThreadForDispatch
					    (thread, iq_idx, 1);
				    } while (cause != FLOSS_DIS_CAUSE_NOT_SET);

				    if (cause == FLOSS_DIS_CAUSE_NOT_SET)
					using_loose_mod_n = true;
				}
			    }
			}
			break;


		      default:
			//
			//  "normal case": Just check for available resources
			//
			cause = checkThreadForDispatch(thread, iq_idx, 1);
			break;
		    }
		}


		if (cause != FLOSS_DIS_CAUSE_NOT_SET) {
		    SET_FIRST_FLOSS_CAUSE(endCause, cause);
		    break;
		}


		//
		//  Dispatch this instruction
		//
		rob = dispatch_one_inst(inst, iq_idx);


		//
		//  Policy-Dependant post-processing
		//
		if (numIQueues > 1) {
		    switch (dispatch_policy) {
		      case MODULO_N:
			if ((rob != 0) &&
			    ((dispatch_count[thread]) % MODULO_VAL))
			{
			    mod_n_queue_idx = ++mod_n_queue_idx % numIQueues;
			    if (!using_loose_mod_n) {
				iq_idx = mod_n_queue_idx;
				first_idx = iq_idx;
			    }
			}
			break;
		      default:
			break;
		    }
		}


		//
		//  If we dispatched the instruction...
		//
		if (rob != 0) {
		    //  remove it from the decode queue
		    decodeQueue->remove(thread);
		    ++dCount;

		    if (dispatch_policy == DEPENDENCE) {
			if (output_reg_valid) {
			    rob->output_reg = output_reg;
			    (*clusterSharedInfo->ri_table)[thread][output_reg].
				setCluster(iq_idx);
			}
		    }

		    if (decodeQueue->instsAvailable(thread) == 0) {
			SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_NO_INSN);
			break;
		    }

		} else {
		    //  this should really never happen...
		    //  ==> It can if the segmented IQ enters deadlock
		    //      recovery mode!

		    // warn("dispatch_one_inst() failed @%d!", curTick);
		    endCause = FLOSS_DIS_POLICY;
		    break;
		}
	    }
	} else {
	    //
	    //  Squashed instruction
	    //
	    decodeQueue->remove(thread);
	    ++dCount;
	}

	//
	//  We must have instructions to continue...
	//
	if (decodeQueue->instsAvailable(thread) == 0) {
	    SET_FIRST_FLOSS_CAUSE(endCause, FLOSS_DIS_NO_INSN);
	    break;
	}

	DispatchEndCause c = checkGlobalResourcesForDispatch(1);
	if (c != FLOSS_DIS_CAUSE_NOT_SET) {
	    SET_FIRST_FLOSS_CAUSE(endCause, c);
	    break;
	}

	//  Check to see if we've dispatched the specified maximum number of
	//  instructions
	if (dCount >= max)
	    break;
    } while (true);

    return dCount;
}


//
//
//  Dispatch this instruction to the specified queue
//
//
ROBStation *
FullCPU::dispatch_one_inst(DynInst *inst, unsigned iq_idx)
{
    if (dispatch_break && (dispatch_break == dispatch_seq))
	dispatch_breakpoint();

    unsigned thread = inst->thread_number;

    if (DTRACE(Pipeline)) {
	string s;
	inst->dump(s);
	DPRINTF(Pipeline, "Dispatch %s\n", s);
    }

    //
    // for load/stores:
    // idep #0     - store operand (value that is store'ed)
    // idep #1, #2 - eff addr computation inputs (addr of access)
    //
    // resulting IQ/LSQ operation pair:
    // IQ (effective address computation operation):
    // idep #0, #1 - eff addr computation inputs (addr of access)
    // LSQ (memory access operation):
    // idep #0     - operand input (value that is store'd)
    // idep #1     - eff addr computation result (from IQ op)
    //
    // effective address computation is transfered via the reserved
    // name DTMP
    //

    RegInfoElement reginfo[TheISA::MaxInstDestRegs];

    ////////////////////////////////////////////////////////////////
    //
    //   Allocate an ROB entry for this instruction
    //
    //////////////////////////////////////////////////////////////////
    ROBStation *rob = ROB.new_tail(thread);
    rob->init(inst, dispatch_seq, numIQueues);

    //////////////////////////////////////////////////////////////////
    //
    //  Determine the chaining information for this instruction
    //
    //
    //
    //////////////////////////////////////////////////////////////////

    NewChainInfo new_chain;
    if (clusterSharedInfo->ci_table != 0) {
	new_chain = choose_chain(inst, iq_idx);

	if (new_chain.suggested_cluster >= 0)
	    iq_idx = new_chain.suggested_cluster;

	// DPRINTF(Dispatch, "DISP:     chain_info: (clust %d) (head %d)\n",
	//	iq_idx, new_chain.head_of_chain);
    } else {
	//  if we're not using chains, don't let the lack of them be
        //  a problem...
	new_chain.out_of_chains = false;
    }


    if (new_chain.out_of_chains) {
	ROB.remove(rob);
	++chains_insuf[thread];
	return 0;
    }

    //////////////////////////////////////////////////////////////////
    //
    //  Place the instruction into the IQ
    //
    //  The ROB has been (almost) completely initialized
    //
    //////////////////////////////////////////////////////////////////
    BaseIQ::iterator rs = 0;

    // Send the instruction to the Instruction Queue
    // (memory barriers excepted: they go to LSQ only)
    if (!inst->isMemBarrier()) {
	rs = IQ[iq_idx]->add(inst, dispatch_seq, rob, reginfo, &new_chain);
	rs->dispatch_timestamp = curTick;

	if (rs.isnull()) {
	    // de-allocate the ROB & LSQ entries...
	    ROB.remove(rob);

	    //  we're done for this cycle
	    return 0;
	}
    }

    rob->iq_entry = rs;

    //////////////////////////////////////////////////////////////////
    //
    //  Add this instruction to the LSQ, as necessary
    //
    //////////////////////////////////////////////////////////////////
    if (inst->isMemRef() || inst->isMemBarrier()) {
	//  Remember to link in the iq_entry!!
	//
	BaseIQ::iterator lsq = LSQ->add(inst, dispatch_seq + 1, rob, 0, 0);

	//  Check for resource allocation failure
	if (lsq.isnull()) {
	    if (rs.notnull()) {
		// we have to clean-up dep-links...
		for (int i = 0; i < rs->num_ideps; ++i) {
		    if (rs->idep_ptr[i]) {
			delete rs->idep_ptr[i];
			rs->idep_ptr[i] = 0;
		    }
		}
	    }

	    //  de-allocate the ROB entry
	    ROB.remove(rob);

	    //  de-allocate the IQ entry
	    if (rs.notnull())
		IQ[iq_idx]->squash(rs);

	    //  We're done for this cycle
	    return 0;
	}

	lsq->dispatch_timestamp = curTick;
	lsq->iq_entry = rs;

	if (rs.notnull()) {
	    IQ[iq_idx]->registerLSQ(rs, lsq);
	    //	rs->lsq_entry = lsq;
	}

	//  Mark this ROB entry as being a memory operation
	//  (changes the ROB-entry sequence number to match the LSQ entry)
	rob->setMemOp(lsq);
	// memory barriers don't require an EA computatiaon
	if (inst->isMemBarrier()) {
	    rob->eaCompPending = false;
	}

	//  We know this instruction has dispatched... add one to
	//  the sequence counter (for the LSQ entry)
	++dispatch_seq;
    }

    //  We've dispatched... count one for the IQ
    ++dispatch_seq;

    //---------------------------------------------------------
    //
    //  Now that we know that we're going to USE the specified
    //  chain...
    //
    if (new_chain.head_of_chain) {
	++chain_heads[thread];
	++chain_heads_in_rob;

	clusterSharedInfo->ci_table->claim(new_chain.head_chain,
					   thread, rob->seq);

	if (chainWires != 0) {
	    chainWires->allocateWire(iq_idx, new_chain.head_chain);
	}
    }

    //  Annotate the ROB entry
    rob->queue_num = iq_idx;

    //
    //  Inform all other clusters that an instruction has dispatched
    //
    for (unsigned i = 0; i < numIQueues; ++i)
	if (i != iq_idx)
	    IQ[i]->inform_dispatch(rs);

    //
    //  1) install outputs after inputs to prevent self reference
    //  2) Update the register information table
    //
    rob->num_outputs = inst->numDestRegs();
    for (int i = 0; i < rob->num_outputs; ++i) {
	TheISA::RegIndex reg = inst->destRegIdx(i);
	rob->onames[i] = reg;
	create_vector[thread].set_entry(reg, rob, i, inst->spec_mode);
	reginfo[i].setCluster(iq_idx);
	(*clusterSharedInfo->ri_table)[thread][reg] = reginfo[i];
    }


    //
    //  Store off the use_spec_cv bitmap and the spec_create
    //  vector entries
    //
    if (inst->recover_inst) {
	// rob->spec_state = new CreateVecSpecState(thread);
	rob->spec_state = state_list.get(&create_vector[thread]);
    }


    ////////////////////////////////////////////////////////////
    //
    //  Now that we know that this instruction has made
    //  it into the IQ/LSQ/ROB... count it as dispatched
    //  NOTE: we include EA-comp instructions in the distribution
    //
    ++dispatch_count[thread];
    ++dispatch_count_stat[thread];
    ++dispatched_ops[thread];

    if (inst->isMemRef())
	++dispatched_ops[thread];

    if (inst->isSerializing())
	++dispatched_serializing[thread];

    //
    //  Add to the pipetrace...
    //
    if (ptrace)
	ptrace->moveInst(inst, PipeTrace::Dispatch, 0, 0, 0);

    /*
     *  Physical registers...
     */
    unsigned num_fp_regs = inst->numFPDestRegs();
    unsigned num_int_regs = inst->numIntDestRegs();

    free_fp_physical_regs -= num_fp_regs;
    free_int_physical_regs -= num_int_regs;

    used_fp_physical_regs[thread] += num_fp_regs;
    used_int_physical_regs[thread] += num_int_regs;

    return rob;
}

//
//  Return the number of a thread which can decode instructions into the
//  Decode/Dispatch queue. This requires that the thread have instructions
//  in the fetch queue and that there is space available for these
//  instructions in the decode queue
//
int
FullCPU::choose_decode_thread()
{
    int rv = -1;

    //  Use a Round-Robin approach to decide where to start
    unsigned t = first_decode_thread;
    first_decode_thread = ++first_decode_thread % number_of_threads;

    unsigned first = t;

    unsigned low_count = UINT_MAX;

    switch (fetch_policy) {
      case IC:
	first = 0;
	t = 0;
	do {
	    unsigned cnt = decodeQueue->count(t) + IQNumInstructions(t);
	    if (ifq[t].num_available()) {
		if (cnt < low_count) {
		    low_count = cnt;
		    rv = t;
		}
	    }
	    t = (t+1) % number_of_threads;
	} while (first != t);
	break;
      default:
	do {
	    if (ifq[t].num_available()) {
		rv = t;
		break;
	    }
	    else {
		t = (t+1) % number_of_threads;
	    }
	} while (first != t);
	break;
    }

    return rv;
}

void
FullCPU::start_decode()
{
    //  if we don't have a place to put new instructions, bail
    if (!decodeQueue->loadable())
	return;

    int thread = choose_decode_thread();

    if (thread < 0) {
	// if we can't decode anything this cycle...
        return;
    }

    FetchQueue *fq = &(ifq[thread]);

    //  as long as there are instructions, and we have bandwidth
    while (decodeQueue->addBW(thread)
	   && (fq->num_valid + fq->num_squashed) > 0)
    {
	DynInst *inst = fq->pull();
	if (inst) {
	    decodeQueue->add(inst);

	    if (inst->btb_miss() && inst->recover_inst)
		fixup_btb_miss(inst);
	} else {
	    //  instruction was squashed earlier...
	    //  drop it on the floor
	}
    }
}

void
FullCPU::fixup_btb_miss(DynInst *inst)
{
    //  For absolute and PC-relative (i.e. direct, not indirect)
    //  control instructions that were predicted taken, the BTB
    //  may have predicted the target address incorrectly or not
    //  at all.  Since these addresses by definition can be
    //  calculated without executing the instruction, fix that up
    //  here.
    //
    //  Note that indirect jumps (that jump to addresses stored in
    //  registers) need to be executed to get the target, so we
    //  can't fix those up yet.
    //
    //  The F_DIRJMP flag indicates a direct control transfer instruction.
    //
    int thread_number = inst->thread_number;

    //  if we had a BTB miss that put us onto the wrong path
    if (inst->isDirectCtrl() && inst->btb_miss() && inst->recover_inst) {
	assert(inst->xc->spec_mode > 0);

	fetch_squash(thread_number);

	inst->recover_inst = false;
	inst->xc->spec_mode--;

	//  Correct the PC for the BTB miss
	inst->xc->regs.pc = inst->branchTarget();

	//
	//  If we've trasferred completely out of spec-mode...
	//
	if (inst->xc->spec_mode == 0) {
	    // reset use_spec_? reg maps and speculative memory state
	    inst->xc->reset_spec_state();
	}

	// Make sure that we don't apply the fixup on THIS cycle:
	// have to schedule event since fetch is simulated after dispatch
	// within each cycle
	fetch_stall[thread_number] |= BRANCH_STALL;
	Event *ev =
	    new ClearFetchStallEvent(this, thread_number, BRANCH_STALL);
	ev->schedule(curTick + cycles(1));
	fid_cause[thread_number] = FLOSS_FETCH_BRANCH_RECOVERY;
    }
}

void
FullCPU::dispatch_init()
{
    if (IQ[0]->type() == BaseIQ::Segmented)
	chainWires = new ChainWireInfo(max_chains, max_wires, numIQueues,
				       chainWirePolicy);
    else
	chainWires = 0;
}

void
FullCPU::dispatchRegStats()
{

    using namespace Stats;

    dispatch_count.resize(number_of_threads);
    for (int i = 0; i < number_of_threads; ++i)
	dispatch_count[i] = 0;
    dispatch_count_stat
	.init(number_of_threads)
	.name(name() + ".DIS:count")
	.desc("cumulative count of dispatched insts")
	.flags(total)
	;

    dispatched_serializing
	.init(number_of_threads)
	.name(name() + ".DIS:serializing_insts")
	.desc("count of serializing insts dispatched")
	.flags(total)
	;

    dispatch_serialize_stall_cycles
	.init(number_of_threads)
	.name(name() + ".DIS:serialize_stall_cycles")
	.desc("count of cycles dispatch stalled for serializing inst")
	.flags(total)
	;

    //
    //  Chaining stats
    //
    chain_heads
	.init(number_of_threads)
	.name(name() + ".DIS:chain_heads")
	.desc("number insts that are chain heads")
	.flags(total)
	;

    chains_insuf
	.init(number_of_threads)
	.name(name() + ".DIS:chains_insuf")
	.desc("number of times thread had insuf chains")
	.flags(total)
	;


    dispatched_ops
	.init(number_of_threads)
	.name(name() + ".DIS:op_count")
	.desc("number of operations dispatched")
	.flags(total)
	;

    rob_cap_events
	.init(number_of_threads)
	.name(name() + ".ROB:cap_events")
	.desc("number of cycles where ROB cap was active")
	.flags(total)
	;

    rob_cap_inst_count
	.init(number_of_threads)
	.name(name() + ".ROB:cap_inst")
	.desc("number of instructions held up by ROB cap")
	.flags(total)
	;

    iq_cap_events
	.init(number_of_threads)
	.name(name() +".IQ:cap_events" )
	.desc("number of cycles where IQ cap was active")
	.flags(total)
	;

    iq_cap_inst_count
	.init(number_of_threads)
	.name(name() + ".IQ:cap_inst")
	.desc("number of instructions held up by IQ cap")
	.flags(total)
	;

    mod_n_disp_stalls.init(number_of_threads);
    mod_n_disp_stall_free.init(number_of_threads);

    if (dispatch_policy == MODULO_N) {
	mod_n_disp_stalls
	    .name(name() + ".DIS:mod_n_stalls")
	    .desc("cycles where dispatch stalled due to mod-n")
	    .flags(total)
	    ;

	mod_n_disp_stall_free
	    .name(name() + ".DIS:mod_n_stall_free")
	    .desc("free slots when dispatch stalled due to mod-n")
	    .flags(total)
	    ;
    }

    reg_int_full
	.name(name() + ".REG:int:full")
	.desc("number of cycles where there were no INT registers")
	;

    reg_fp_full
	.name(name() + ".REG:fp:full")
	.desc("number of cycles where there were no FP registers")
	;

    insufficient_chains
	.name(name() + ".DIS:insufficient_chains")
	.desc("Number of instances where dispatch stopped")
	;

    secondChoiceCluster
	.name(name() + ".DIS:second_choice_clust")
	.desc("Number of instructions dispatched to second-choice cluster");

    secondChoiceStall
	.name(name() + ".DIS:second_choice_stall")
	.desc("Number of instructions stalled when first choice not available");

    //
    //  Two input instruction stats
    //
    two_op_inst_count
	.init(number_of_threads)
	.name(name() + ".DIS:two_input_insts")
	.desc("Number of two input instructions queued")
	.flags(total)
	;

    one_rdy_inst_count
	.init(number_of_threads)
	.name(name() + ".DIS:one_rdy_insts")
	.desc("number of 2-op insts w/ one rdy op")
	.flags(total)
	;

    chain_create_dist
	.init(NUM_CHAIN_CR_CLASSES)
	.name(name() + ".DIS:chain_creation")
	.desc("Reason that chain head was created")
	.flags(pdf | dist)
	;
    for (int i=0; i < NUM_CHAIN_CR_CLASSES; ++i) {
	chain_create_dist.subname(i, chain_cr_class_desc[i]);
    }

    inst_class_dist
	.init(NUM_INSN_CLASSES)
	.name(name() + "inst_class_dist")
	.desc("Operand status at dispatch")
	.flags(pdf | dist)
	;
    for (int i=0; i < NUM_INSN_CLASSES; ++i) {
	inst_class_dist.subname(i, dispatchInstClassDesc[i]);
    }
}

void
FullCPU::dispatchRegFormulas()
{
    using namespace Stats;

    chain_head_frac
	.name(name() + ".DIS:chain_head_frac")
	.desc("fraction of insts that are chain heads")
	.flags(total)
	;
    chain_head_frac = 100 * chain_heads / dispatch_count_stat;

    chains_insuf_rate
	.name(name() + ".DIS:chains_insuf_rate")
	.desc("rate that thread had insuf chains")
	.flags(total)
	;
    chains_insuf_rate = chains_insuf / numCycles;

    dispatched_op_rate
	.name(name() + ".DIS:op_rate")
	.desc("dispatched operations per cycle")
	.flags(total)
	;
    dispatched_op_rate = dispatched_ops / numCycles;

    dispatch_rate
	.name(name() + ".DIS:rate")
	.desc("dispatched_insts per cycle")
	.flags(total)
	;
    dispatch_rate = dispatch_count_stat / numCycles;

    if (dispatch_policy == MODULO_N) {

	mod_n_stall_avg_free
	    .name(name() + ".DIS:mod_n_stall_avg_free")
	    .desc("avg free slots per cycle")
	    .flags(total)
	    ;
	mod_n_stall_avg_free = mod_n_disp_stall_free / mod_n_disp_stalls;

	mod_n_stall_frac
	    .name(name() + ".DIS:mod_n_stall_frac")
	    .desc("avg stalls per cycle")
	    .flags(total)
	    ;
	mod_n_stall_frac = mod_n_disp_stalls / numCycles;
    }

    reg_int_occ_rate
	.name(name() + ".REG:int:occ_rate")
	.desc("Average INT register usage")
	.flags(total)
	.precision(0)
	;
    reg_int_occ_rate = reg_int_thrd_occ / numCycles;

    reg_fp_occ_rate
	.name(name() + ".REG:fp:occ_rate")
	.desc("Average FP register usage")
	.flags(total)
	.precision(0)
	;
    reg_fp_occ_rate = reg_fp_thrd_occ / numCycles;

    two_input_ratio
	.name(name() + ".DIS:two_input_ratio")
	.desc("fraction of all insts having 2 inputs")
	.flags(total)
	;
    two_input_ratio = two_op_inst_count / dispatch_count_stat;

    one_rdy_ratio
	.name(name() + ".DIS:one_rdy_ratio")
	.desc("fraction of 2-op insts w/ one ready op")
	.flags(total)
	;
    one_rdy_ratio = one_rdy_inst_count / two_op_inst_count;

}

//
//  Determine which chain this instruction should belong to
//
NewChainInfo
FullCPU::choose_chain(DynInst *inst, unsigned cluster)
{
    NewChainInfo rv;
    unsigned chained_ideps = 0,
    st_ideps = 0;
    int head_num = -1;

    int suggested_cluster = -1;

    bool inst_is_load = false;
    unsigned thread = inst->thread_number;
    		//    unsigned max_latency_depth = 0;
    unsigned ops_pred_ready_time = 0;

    //
    //  Shared info...
    //
    RegInfoTable * rit = clusterSharedInfo->ri_table;
    ChainInfoTableBase *chain_info = clusterSharedInfo->ci_table;
    GenericPredictor *hm_predictor = clusterSharedInfo->hm_predictor;
    GenericPredictor *lr_predictor = clusterSharedInfo->lr_predictor;

    if (DTRACE(Chains)) {
	string s;
	inst->dump(s);
	DPRINTF(Chains,"Chains: fetch_seq %d : %s\n", inst->fetch_seq, s);
    }

    if (inst->isLoad()) {
	inst_is_load = true;

	if (use_hm_predictor) {
	    //  If we predict a hit
	    if (hm_predictor->predict(inst->PC >> 2) == 1) {
		rv.hm_prediction = MA_HIT;
		DPRINTF(Chains, "Chains:   load predicted HIT\n");
	    } else {
		rv.hm_prediction = MA_CACHE_MISS;
		DPRINTF(Chains, "Chains:   load predicted MISS\n");
	    }
	} else {
	    rv.hm_prediction = MA_NOT_PREDICTED;
	}


	if (hmp_func == HMP_BOTH && rv.hm_prediction == MA_HIT) {
	    inst_is_load = false;  // actually, just don't create HEAD
	    DPRINTF(Chains, "Chains:   HMP-Both: load isn't head\n");
	}

	if (hmp_func == HMP_HEAD_SEL && rv.hm_prediction == MA_HIT
	     && chain_heads_in_rob > (clusterSharedInfo->total_chains * 0.75))
	{
	    inst_is_load = false;  // actually, just don't create HEAD
	    DPRINTF(Chains, "Chains:   HMP-Head-Sel: load isn't head\n");
	}
    } else
	rv.hm_prediction = MA_NOT_PREDICTED;


    //
    //  Check all IDEPS
    //
    //  We want to find the register with the largest latency value (chained
    //  or unchained) and treat this instruction as following that register
    //
    for (int i = 0; i < inst->numSrcRegs(); ++i) {
	unsigned reg = inst->srcRegIdx(i);

	//  Get the predicted ready time for this operand
	unsigned cmp_time = (*rit)[thread][reg].predReady();

	//  Earliest an op can become ready is _this_ cycle
	if (cmp_time == 0)
	    cmp_time = curTick;

	//  Is this operand going to arrive later than the others?
	if (ops_pred_ready_time < cmp_time) {
	    ops_pred_ready_time = cmp_time;
	    rv.pred_last_op_index = i;
	}


	//  We need these regardless... we may over-write them below
	rv.idep_info[i].chained = (*rit)[thread][reg].isChained();
	rv.idep_info[i].delay = (*rit)[thread][reg].latency();
	rv.idep_info[i].op_pred_ready_time = cmp_time;

	//  Get cluster where this operand is being produced
	rv.idep_info[i].source_cluster = (*rit)[thread][reg].cluster();


	//
	//  Registers can be in one of two states:
	//   (1) Chained -- this inst should follow another
	//   (2) Self-Timed -- this inst should find its own way based
	//                     on the delay value. Note that this delay
	//                     value will be zeroed when the producing
	//                     instruction writes-back
	//
	if (rv.idep_info[i].chained) {
	    rv.idep_info[i].follows_chain = (*rit)[thread][reg].chainNum();
	    rv.idep_info[i].chain_depth = (*rit)[thread][reg].chainDepth();

	    ++chained_ideps;
	} else {
	    //  we should only count this value as pending if it
	    //  hasn't written back
	    if (rv.idep_info[i].delay)
		++st_ideps;
	}
    }


    //
    //  Check for and remove duplicate chain entries in rv...
    //
    //  We only want to connect this instruction to a chain once
    //
    for (int i = 0; i < TheISA::MaxInstSrcRegs - 1; ++i) {
	if (!rv.idep_info[i].chained)
	    continue;

	//
	//  If we have duplicate input operands, treat all but the first
	//  as if they were ready
	//
	for (int j = i + 1; j < TheISA::MaxInstSrcRegs; ++j) {
	    //
	    //  if both of these i-deps follow the same chain
	    //
	    if (rv.idep_info[j].chained &&
		rv.idep_info[i].follows_chain == rv.idep_info[j].follows_chain)
	    {

		//  we only want to follow _one_ of these (and the
		//  i-th one is easier) // make sure we follow at the
		//  longer delay value
	        if (rv.idep_info[i].delay < rv.idep_info[j].delay)
		    rv.idep_info[i].delay = rv.idep_info[j].delay;

	        //  remove the later entry
	        rv.idep_info[j].chained = false;
	        rv.idep_info[j].delay = 0;

		//  fix this counter
		--chained_ideps;
	    }
	}
    }


    int pending_ideps = chained_ideps + st_ideps;
    DPRINTF(Chains, "Chains:   %d pending ideps (%d self-timed)\n",
	    pending_ideps, st_ideps);

    //
    //  We always assume that a self-timed instruction will finish before a
    //  chained instruction
    //

    //  if less than two operands are pending, it's not really a prediction!
    unsigned chained_idep_index = rv.pred_last_op_index;
    if (pending_ideps < 2) {
	// we use this flag in SegmentedIQ::writeback()
	rv.pred_last_op_index = -1;
    }

    //
    //  Last-Op-Prediction:
    //
    //      Only try to predict the last op if we have more than one
    //      instruction chained
    //

    //  FIXME: This code assumes that there are only TWO input deps!

    rv.lr_prediction = -1;

    if (chained_ideps > 1) {
	if (use_lat_predictor) {
	    int other = 1 - rv.pred_last_op_index;

	    // This input op will appear to the scheduling logic as
	    // if the dependence has already been met...
	    //  -> The real dependence mechanism is unaffected by this
	    rv.idep_info[other].chained = false;
	    rv.idep_info[other].delay   = 0;

	    --chained_ideps;
	}

	if (use_lr_predictor) {
	    rv.pred_last_op_index = lr_predictor->predict(inst->PC >> 2);
	    rv.lr_prediction = rv.pred_last_op_index;

	    for (int other = 0; other < TheISA::MaxInstSrcRegs; ++other) {
		/**
		 *  @todo This code only works correctly for 2-input
		 *  instructions 3-input instructions never chain
		 *  their 3rd input...
		 */
		if (other != rv.pred_last_op_index) {
		    // This input op will appear to the scheduling logic as
		    // if the dependence has already been met...
		    //  -> The real dependence mechanism is unaffected by this
		    rv.idep_info[other].chained = false;
		    rv.idep_info[other].delay   = 0;

		    --chained_ideps;
		}
	    }
	}
    }

    //  FIXME: We don't do chaining among clusters for more than one
    //         chained ideps!!!
//    assert(chained_ideps < 2);

    unsigned producing_cluster;

    if (chained_ideps > 1) {
	//
	//  Special case...
	//
	//  FIXME: only looks at the first two ideps!
	//
	producing_cluster = rv.idep_info[0].source_cluster;
	unsigned c = rv.idep_info[1].source_cluster;

	//  if the two cluster id's don't match, we have to pick one...
	if (producing_cluster != c) {
	    bool g0 = ((IQ[producing_cluster]->free_slots() > 0) && 
		       !IQ[producing_cluster]->cap_met(thread));
	    bool g1 = ((IQ[c]->free_slots() > 0) && 
		       !IQ[c]->cap_met(thread));

	    if (g0 && g1) {
		//  both good...
		//  use the "other" cluster if it has lower occupancy...
		if (IQ[c]->free_slots() < IQ[producing_cluster]->free_slots()) {
		    producing_cluster = c;
		}
	    }
	    else if (g0) {
		// use 'producing_cluster'
	    }
	    else {
		// only one choice, actually
		producing_cluster = c;

		// we actually get here for the !g0 && !g1 case, but
		// the result doesn't actually matter, since the instruction
		// won't dispatch in this case.
	    }
	}
	else {
	    //  they are the same... good to go!
	}
    }
    else {
	if ((chained_ideps == 1) || pending_ideps) {
	    //
	    //  We have an input in the queue
	    //
	    producing_cluster = rv.idep_info[chained_idep_index].source_cluster;
	} 
	else {
	    //
	    //  We don't have a producing cluster... choose the least-full cluster
	    //
	    producing_cluster = IQLeastFull();
	}
    }

    //
    //  Look for instructions that have to make a decision about which
    //  chain to follow
    //

    if (inst->numSrcRegs() > 1) {
	++two_op_inst_count[inst->thread_number];

	//  count the number of these with exactly one ready idep
	if (pending_ideps == 1) {
	    ++one_rdy_inst_count[inst->thread_number];
	}
    }


    if (pending_ideps == 0)
	inst_class_dist[INSN_CLASS_ALL_RDY] += 1;
    else if (pending_ideps == 1)
	inst_class_dist[INSN_CLASS_ONE_NOT_RDY] += 1;
    else if (chained_ideps > 1)
	inst_class_dist[INSN_CLASS_MULT_CHAINS] += 1;
    else if (chained_ideps == 1)
	inst_class_dist[INSN_CLASS_ONE_CHAINED] += 1;
    else
	inst_class_dist[INSN_CLASS_ALL_SELF_TIMED] += 1;

    //
    //  This instruction is the head of a chain if:
    //   (1)  It is self-timed (and generates an output)
    //   (2)  It is following more than 1 chain
    //   (3)  It is a load
    //   (4)  The chain depth reported is greater than max_chain_depth
    //
    if ((CHAIN_HEAD_IND_INSTS && pending_ideps == 0 && inst->numDestRegs() > 0)
	|| chained_ideps > 1 || inst_is_load
#if 0
	|| max_latency_depth > max_chain_depth
#endif
	)
    {
	rv.head_of_chain = true;

	rv.out_of_chains = chain_info->chainsFree() == 0;

	//
	//  Bail out if we don't have any free chains...
	//
	if (rv.out_of_chains) {
	    ++insufficient_chains;

	    DPRINTF(Chains, "Chains:   out of Chain Wires\n");
	    return rv;
	}
    }


    //////////////////////////////////////////////////////////////
    //
    //  Chain Wire Policies don't matter unless we have more than
    //  one cluster
    //
    if (!rv.out_of_chains && (numIQueues > 1) && (chainWires != 0)) {

	//
	//  Chain Wire Policies:
	//
	//     OneToOne:  Each cluster has enough wires for all chains
	//                --> use originally-specified cluster
	//     Static:    Each cluster handles a subset of chains
	//                --> change iq_idx as appropriate
	//     Dynamic:   Each cluster can allocate wires to chains
	//                --> if cluster hosting chain is not available,
	//                    assign instruction to cluster that can
	//                    allocate a new chain
	//
	bool stall = false;

	switch (chainWirePolicy) {
	  case ChainWireInfo::OneToOne:
	    //  Use the originally specified cluster
	    if (checkClusterForDispatch(cluster, rv.head_of_chain)) {
		//  we're good to go...
		suggested_cluster = cluster;

		head_num = chain_info->find_free();
	    }
	    else {
		++secondChoiceStall;
		stall = true;
	    }
	    break;

	  case ChainWireInfo::Static:
	    if (checkClusterForDispatch(producing_cluster, rv.head_of_chain)) {
		//  we're good to go...
		suggested_cluster = producing_cluster;

		head_num = chainWires->findFreeWire(suggested_cluster);
	    }
	    else {
		suggested_cluster = IQLeastFull();

		//  we must mark this instruction as a chain-head if
		//  we dispatch it to a second-choice cluster

		if (producing_cluster != suggested_cluster) {

		    //  if our second-choice is ok...
		    if (checkClusterForDispatch(suggested_cluster, true)) {

			rv.head_of_chain = true;

			// unchain inputs that aren't in this cluster
			for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
			    if (rv.idep_info[i].chained) {
				if (!chainWires->chainMapped(suggested_cluster,
							     rv.idep_info[i].follows_chain))
				{
				    rv.idep_info[i].chained = false;
				}
			    }
			}

			head_num = chainWires->findFreeWire(suggested_cluster);
			++secondChoiceCluster;
		    }
		    else {
			++secondChoiceStall;
			stall = true;
		    }
		}
		else {
		    ++secondChoiceStall;
		    stall = true;
		}
	    }
	    break;

	  case ChainWireInfo::StaticStall:
	    if (checkClusterForDispatch(producing_cluster, rv.head_of_chain)) {
		//  we're good to go...
		suggested_cluster = producing_cluster;

		head_num = chainWires->findFreeWire(suggested_cluster);
	    } else {
		++secondChoiceStall;
		stall = true;
	    }
	    break;


	    //
            //  this may or may not actually work...
	    //
	  case ChainWireInfo::Dynamic:
	    if (checkClusterForDispatch(producing_cluster, rv.head_of_chain)) {
		//  we're good to go...
		suggested_cluster = producing_cluster;

		//  dynamic policy can use ANY free chain number
		head_num = chain_info->find_free();
	    }
	    else {
		suggested_cluster = IQLeastFull();

		//  we must mark this instruction as a chain-head if
		//  we dispatch it to a second-choice cluster

		if (producing_cluster != suggested_cluster) {
		    //  if our second-choice is ok...
		    if (checkClusterForDispatch(suggested_cluster, true)) {

			rv.head_of_chain = true;

			// unchain inputs that aren't in this cluster
			for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
			    if (rv.idep_info[i].chained) {
				if (!chainWires->chainMapped(suggested_cluster,
							     rv.idep_info[i].follows_chain))
				{
				    rv.idep_info[i].chained = false;
				}
			    }
			}

			head_num = chain_info->find_free();
			++secondChoiceCluster;
		    }
		    else {
			++secondChoiceStall;
			stall = true;
		    }
		}
		else {
		    ++secondChoiceStall;
		    stall = true;
		}
	    }
	    break;
	}


	//
	//  bail out...
	//
	if (stall) {
	    rv.out_of_chains = true;
	    ++insufficient_chains;

	    DPRINTF(Chains, "Chains:   out of Chain Wires\n");
	    return rv;
	}
    }
    else {
	//
	//  Single cluster...
	//
	if (rv.head_of_chain) {
	    //
	    //  Find a free chain
	    //  (returns -1 for no chains)
	    //
	    head_num = chain_info->find_free();
	}
    }

    rv.suggested_cluster = suggested_cluster;

    if (rv.head_of_chain) {

	//
	//  Did we find a free chain?
	//
	if (head_num >= 0) {
	    DPRINTF(Chains, "Chains:   head of chain %d\n", head_num);

	    //
	    //  Yup... We collect stats for this chain in writeback,
	    //         just before we init() the chain
	    //

#if 0
	    if (max_latency_depth > max_chain_depth)
		chain_create_dist[CHAIN_CR_DEPTH] += 1;
	    else
#endif
	    if (inst_is_load)
		chain_create_dist[CHAIN_CR_LOAD] += 1;
	    else if (pending_ideps==0)
		chain_create_dist[CHAIN_CR_NO_IDEPS] += 1;
	    else
		chain_create_dist[CHAIN_CR_MULT_IDEPS] += 1;

	    rv.head_chain = head_num;
	} else {
	    //
	    //  Nope. We can't dispatch this inst...
	    //
	    rv.out_of_chains = true;
	    ++insufficient_chains;

	    DPRINTF(Chains, "Chains:   insufficient chains\n");
	}
    }

    DPRINTF(Chains, "Chains:   %s", rv.str_dump());

#if DUMP_CHAIN_INFO
    cout << "@" << curTick << endl;
    inst->dump();
    rv.dump();
#endif

    return rv;
}


