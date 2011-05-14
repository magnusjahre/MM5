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

#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>

#include "base/cprintf.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/dep_link.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/issue.hh"
#include "encumbered/cpu/full/readyq.hh"
#include "encumbered/cpu/full/storebuffer.hh"
#include "encumbered/cpu/full/thread.hh"
#include "encumbered/cpu/full/writeback.hh"
#include "mem/cache/cache.hh"
#include "mem/functional/memory_control.hh"
#include "mem/mem_interface.hh"
#include "sim/eventq.hh"

using namespace std;


//
//  Make sure you disable this for "normal" operation!!!
//
#define REMOVE_WP_LOADS 0


//
//  Local declarations
//

InstSeqNum issue_break = 0;

void
issue_breakpoint()
{
    ccprintf(cerr,"Reached issue breakpoint @ %d\n", curTick);
}

struct issue_info
{
    InstSeqNum seq;
    unsigned      clust;
    string        inst;

    issue_info(InstSeqNum s, unsigned c, string &i) {
	seq = s;
	clust = c;
	inst = i;
    }

};

bool
operator<(const issue_info &l, const issue_info &r)
{
    return l.seq < r.seq;
}


static const char *unissued_names[Num_OpClasses + 2];

#define UNISSUED_CACHE_BLOCKED (Num_OpClasses)
#define UNISSUED_TOO_YOUNG     (Num_OpClasses + 1)




//#define DEBUG_ISSUE

#define ISSUE_OLDEST 0
#define DEBUG_FLOSS 1
#define DUMP_LOADS 0

#define DUMP_ISSUE 0

#define DUMP_REFRESH 0


/*
 *  LSQ_REFRESH() - memory access dependence checker/scheduler
 */

/*
 * load/store queue (LSQ): holds loads and stores in program order, indicating
 * status of load/store access:
 *
 *   - issued: address computation complete, memory access in progress
 *   - completed: memory access has completed, stored value available
 *   - squashed: memory access was squashed, ignore this entry
 *
 * loads may execute when:
 *   1) register operands are ready, and
 *   2) memory operands are ready (no earlier unresolved store)
 *
 * loads are serviced by:
 *   1) previous store at same address in LSQ (hit latency), or
 *   2) data cache (hit latency + miss latency)
 *
 * stores may execute when:
 *   1) register operands are ready
 *
 * stores are serviced by:
 *   1) depositing store value into the load/store queue
 *   2) writing store value to the store buffer (plus tag check) at commit
 *   3) writing store buffer entry to data cache when cache is free
 *
 * NOTE: the load/store queue can bypass a store value to a load in the same
 *   cycle the store executes (using a bypass network), thus stores complete
 *   in effective zero time after their effective address is known
 */


struct StoreInfoRecord {
    Addr addr;
    bool addr_known;
    bool data_known;
};


//
//  Memory Address Disambiguation
//
//  This function walks the LSQ, taking note of stores, and looks for load
//  instructions that might conflict with earlier stores.
//
//  NOTE: This function is the *only* place were loads get placed on the
//        ready list. All other instructions are enqueued when their last
//        operand is marked as ready. This means that a load can be "ops-ready"
//        but *not* on the ready list.
//
//  We walk the LSQ from oldest to youngest looking for stores, and when we
//  encounter a load, we walk the list of earlier stores from youngest to
//  oldest looking for the youngest store that matches the load.
//
void
FullCPU::lsq_refresh()
{
    load_store_queue *ls_queue = (load_store_queue *)LSQ;

    // The list of all "earlier" stores
    list<StoreInfoRecord> store_list[SMT_MAX_THREADS];

    typedef list<StoreInfoRecord>::reverse_iterator r_it;

    // mark threads that have no chance of issuing anything else, for
    // various reasons:
    // - any store under DISAMBIG_CONSERVATIVE
    // - memory barrier under any model
    bool thread_done[SMT_MAX_THREADS];
    unsigned num_threads_done = 0;

    for (int i = 0; i < number_of_threads; ++i)
	thread_done[i] = false;

#if DUMP_REFRESH
    cout << "@" << curTick << ":\n";
#endif

    //
    //  Scan the LSQ:
    //
    BaseIQ::iterator lsq = ls_queue->head();
    for (; lsq.notnull(); lsq = lsq.next()) {
	//  Skip any LSQ entries that have been squashed
	if (lsq->squashed)
	    continue;

	unsigned thread = lsq->thread_number();

	if (thread_done[thread])
	    continue;

	DynInst *inst = lsq->inst;

	if (inst->isMemBarrier()) {
	    // Memory barrier: total blocking condition.  We're done
	    // with this thread.
	    thread_done[thread] = true;
	    if (++num_threads_done == number_of_threads)
		break; // out of lsq loop
	} else if (inst->isStore()) {
	    // Stores...

	    // If it's ready to go, put it on the ready list.
	    // (This used to be done unconditionally in writeback, but
	    // was moved here so we could suppress it when a memory
	    // barrier is pending.)
	    if (!lsq->queued && lsq->ops_ready()) {
		ls_queue->ready_list_enqueue(lsq);
	    }

	    if (disambig_mode == DISAMBIG_CONSERVATIVE) {
		// force stores to go completely in order.
		thread_done[thread] = true;
		if (++num_threads_done == number_of_threads)
		    break; // out of lsq loop
	    } else {
		// Store (and not conservative disambiguation).  Just
		// record for now.  Note that under normal
		// disambiguation, an unknown-address store doesn't
		// end the thread, since a younger store with known
		// address & data could forward data to an even
		// younger load.
		StoreInfoRecord sir;

		sir.addr = inst->eff_addr;
		// Under oracle disambiguation, we treat all addresses as
		// known even if the pipeline hasn't calculated them yet.
		sir.addr_known = (STORE_ADDR_READY(lsq)
				  || disambig_mode == DISAMBIG_ORACLE);
		sir.data_known = lsq->ops_ready();

		store_list[thread].push_back(sir);
	    }
	} else {
	    // must be a load, or it wouldn't be in LSQ
	    assert(inst->isLoad());
	    //
	    //  If this is a cacheable load that has calculated its
	    //  effective address, but is not yet on the ready-list,
	    //  check it against earlier stores:
	    //     (1) Earlier stores that haven't calculated their address
	    //           --> can't issue the load
	    //     (2) Earlier stores that *have* calculated their address
	    //          (a) If the address matches:
	    //              --> if the store data is available: issue the load
	    //              --> if the store data is *not* avaialable:
	    //                  the load must wait
	    //          (b) If the address doesn't match, issue the load
	    //
	    //  Uncacheable loads can't be issued until they become
	    //  non-speculative, so we ignore them here.  They will be
	    //  added to the ready list in commit.

	    if (!lsq->queued && lsq->ops_ready()
		&& !(lsq->inst->mem_req_flags & UNCACHEABLE)) {
		bool enqueue = true;

		//
		//  Check for conflicts with earlier stores
		//
		//  We walk *backwards* through the list of stores so that
		//  we match up this load with the most recent store
		//
		for (r_it store=store_list[thread].rbegin();
		     store != store_list[thread].rend();
		     ++store)
		{
		    if (store->addr_known) {
			//
			//  Store *HAS* calculated its effective address
			//
			if (store->addr == inst->eff_addr) {
			    //
			    //  Load & Store *ADDRESSES MATCH*
			    //    --> this is the most-recent store
			    //
			    if (store->data_known) {
				//
				// FIXME! We disregard the size of the
				// operations and forward from the store
				// to the load
				//
			    }
			    else {
				//
				//  The store doesn't have its data yet..
				//  --> we need that data... this load must
				//      wait
				enqueue = false;
			    }

			    //  this is the only store we really care about,
			    //  so we don't want to look any earlier
			    break;
			} else {
			    //
			    //  Load & Store addresses do NOT match
			    //
			}
		    } else {
			//
			//  Store has not calculated its effective address
			//

			//  we can't issue this load, since we don't know if
			//  this store conflicts

			enqueue = false;
			break;
		    }
		}

		if (enqueue) {
		    ls_queue->ready_list_enqueue(lsq);
#if DUMP_REFRESH
		    cout << "  " << lsq->seq << endl;
#endif
		} else {
		    ++lsq_blocked_loads[thread];
		}
	    }
	}
    }
}

//

/* Try to issue the LSQ portion of a load.  Returns flag indicating
 * whether instruction was actually issued.  If load is issued,
 * completion latency (if known) is returned via latency pointer
 * argument, and caller is responsible for scheduling writeback event.
 * If latency is unknown, returned latency value is set to -1, and a
 * future callback from the memory system will schedule the writeback.
 */
bool
FullCPU::issue_load(BaseIQ::iterator lsq, int *latency)
{
	int load_lat;
	bool addr_is_valid;
	DynInst *inst;
	int thread_number;
	bool issued;

	inst = lsq->inst;
	thread_number = lsq->thread_number();
	addr_is_valid = inst->xc->validDataAddr(inst->eff_addr) &&
	inst->fault == No_Fault;

#if FULL_SYSTEM
	if (inst->xc->misspeculating() &&
			inst->xc->memctrl->badaddr(inst->phys_eff_addr))
		addr_is_valid = false;
#endif

	/* reset lsq->mem_result value */
	lsq->mem_result = MA_HIT;

	/* Assume it's issued for now. If neccessary, unset later */
	issued = true;

	/* for loads, determine cache access latency:
	 * first scan LSQ to see if a store forward is
	 * possible, if not, access the data cache */
	load_lat = 0;

	for (BaseIQ::iterator earlier_lsq = lsq.prev();
	earlier_lsq.notnull(); earlier_lsq = earlier_lsq.prev()) {

		/* FIXME: not dealing with partials! */
		if (earlier_lsq->inst->isStore() &&
				earlier_lsq->inst->eff_addr == inst->eff_addr &&
				earlier_lsq->inst->asid == inst->asid) {
			/* hit in the LSQ */
			lsq_forw_loads[thread_number]++;
			load_lat = cycles(1);
			break;
		}
	}

	/* was the value store forward from the LSQ? */
	if (!load_lat) {
		if (!inst->spec_mode && !addr_is_valid)
			sim_invalid_addrs++;

		/* no! go to the data cache if addr is valid */
		if (addr_is_valid) {

#if DUMP_LOADS
			if (!inst->spec_mode) {
				cerr << "T" << inst->thread_number << " : Load from 0x" << hex
				<< inst->eff_addr << " issued" << endl;
			}
#endif

			//
			//  Prepare memory request...
			//
			MemReqPtr req = new MemReq();
			req->thread_num = thread_number;
			req->asid = inst->asid;
			req->cmd = Read;
			req->vaddr = inst->eff_addr;
			req->paddr = inst->phys_eff_addr;
			req->flags = inst->mem_req_flags;
			req->size = 1;
			req->time = curTick;
			req->data = new uint8_t[1];
			req->xc = inst->xc;
			req->pc = inst->PC;

			WritebackEvent *wb_event
			= new WritebackEvent(this, lsq->rob_entry, req);

			req->completionEvent = wb_event;
			req->expectCompletionEvent = true;

			// so we can invalidate on a squash
			lsq->rob_entry->wb_event = wb_event;

			BaseIQ::CacheMissEvent *cm_event
			= IQ[0]->new_cm_event(lsq->rob_entry);

			lsq->rob_entry->cache_event_ptr = cm_event;

#if REMOVE_WP_LOADS
			//
			//  Treat spec-mode loads as if they are hits...
			//   --> don't actually send them to the cache!
			//
			if (inst->spec_mode) {
				lsq->mem_result = MA_HIT;
				wb_event->schedule(curTick + cycles(3));
				if (cm_event != 0)
					cm_event->schedule(curTick + cycles(3));
			} else {
#endif
				lsq->mem_result = dcacheInterface->access(req);

#if REMOVE_WP_LOADS
			}
#endif

			//
			//  We schedule this event ourselves... not required to be
			//  part of the cache...
			//
			if (cm_event != NULL) {
				cm_event->annotate(lsq->mem_result);
				Tick latency = dcacheInterface->getHitLatency();
				assert(latency >= clock);
				cm_event->schedule(curTick + latency);
			}

			// negative load latency prevents issue()
			// from scheduling a writeback event... this
			// will come from the memory system
			// instead.
			load_lat = -1;
		} else {
			load_lat = *latency;
			if (inst->xc->misspeculating()) {
				// invalid addr is misspeculation, just use op latency
				inv_addr_loads[thread_number]++;
			} else if (inst->fault == No_Fault) {
				// invalid addr is a bad address, panic
				panic("invalid addr 0x%x accessed and not misspeculating",
						inst->eff_addr);
			}
		}
	}

	*latency = load_lat;  // return a latency value back to lsq_issue()
	lsq->rob_entry->mem_result = lsq->mem_result;

	return issued;
}

//

/* Try to issue the LSQ portion of a prefetch instruction.  Currently,
 * this always succeeds, as we just throw away the prefetch if it
 * can't be issued immediately.  Instruction is also marked as
 * completed.  Returned latency should be ignored.
 */
bool
FullCPU::issue_prefetch(BaseIQ::iterator rs, int *latency)
{
    bool addr_is_valid;
    int pf_size = 0;
    DynInst *inst;
    int thread_number;

    /* Prefetches always issue & complete right away: they are just
     * discarded (treated like no-ops) if there aren't enough
     * resources for them.
     */
    /* reset rs->mem_result value */
    rs->mem_result = MA_HIT;
    rs->rob_entry->completed = true;

    if (softwarePrefetchPolicy == SWP_DISABLE) {
	// prefetching disabled: nothing to do
	return true;
    }

    if (softwarePrefetchPolicy == SWP_SQUASH) {
	cout << "# " << rs->seq << endl;
	rs->dump();
	fatal("The swp instruction should be squashed earlier.\n");
    }

    inst = rs->inst;
    thread_number = rs->thread_number();
    addr_is_valid = (inst->eff_addr != MemReq::inval_addr
		     && inst->xc->validDataAddr(inst->eff_addr)
		     && inst->fault == No_Fault);

#if FULL_SYSTEM
    if (inst->xc->misspeculating() &&
	inst->xc->memctrl->badaddr(inst->phys_eff_addr))
	addr_is_valid = false;
#endif

    /* no! go to the data cache if addr is valid */
    if (addr_is_valid) {
	MemReqPtr req = new MemReq();
	req->thread_num = thread_number;
	req->asid = inst->asid;
	req->cmd = Soft_Prefetch;
	req->vaddr = inst->eff_addr;
	req->paddr = inst->phys_eff_addr;
	req->flags = inst->mem_req_flags;
	req->size = pf_size;
	req->completionEvent = NULL;
        req->expectCompletionEvent = false;
	req->time = curTick;
	req->data = new uint8_t[pf_size];
	req->xc = inst->xc;
	req->pc = inst->PC;

	dcacheInterface->access(req);
    } else {
	/* invalid addr */
	inv_addr_swpfs[thread_number]++;
    }

    //  We "return" the original FU op-latency through the "latency" pointer
    return true;
}


//
//  Issue an instruction from the Store-Ready Queue
//
bool
FullCPU::sb_issue(StoreBuffer::iterator i, unsigned pool_num)
{
    /* wrport should be available */
    int fu_lat = FUPools[pool_num]->getUnit(MemWriteOp);

    if (fu_lat == -2)
	fatal("Function unit type not available!");

    if (!dcacheInterface->isBlocked()) {
	if (fu_lat >= 0) {

	    stat_issued_inst_type[i->thread_number()][MemWriteOp]++;

	    MemReqPtr req = new MemReq();
            req->thread_num = i->thread_number();
	    req->asid = i->asid;
	    if (i->isCopy) {
		req->cmd = Copy;
		req->vaddr = i->srcVirtAddr;
		req->paddr = i->srcPhysAddr;
		req->dest = i->phys_addr;
                req->expectCompletionEvent = false;
	    } else {
		req->cmd = Write;
		req->vaddr = i->virt_addr;
		req->paddr = i->phys_addr;
		Event *completion_event =
		    new SimCompleteStoreEvent(storebuffer, i, req);

		req->completionEvent = completion_event;
                req->expectCompletionEvent = true;
	    }
	    req->flags = i->mem_req_flags;
	    req->size = i->size;
	    req->time = curTick;
            req->data = new uint8_t[i->size];
	    if (i->data) {
		memcpy(req->data, i->data, i->size);
	    }
	    req->xc = i->xc;

	    assert((req->paddr & ((Addr)req->size-1)) == 0);

	    dcacheInterface->access(req);

	    i->issued = true;
	} else {
	    //
	    //  This problem is accounted for when the store-buffer
	    //  fills up.
	    //
	    stat_fu_busy[MemWriteOp]++;
	    stat_fuBusy[pool_num][MemWriteOp]++;
	    fu_busy[i->thread_number()]++;
	}
    }

    return i->issued;
}

//

//
//  Issue an instruction from the IQ
//
bool
FullCPU::iq_issue(BaseIQ::iterator i, unsigned fu_pool_num)
{
    bool issued = false;
    int fu_lat;

    /*
     *  check to make sure that the instruction has spent enough time
     *  in the "issue stage" to be issued
     */
    if (curTick < (i->dispatch_timestamp + dispatch_to_issue_latency)) {

	if (floss_state.issue_end_cause[0] == ISSUE_CAUSE_NOT_SET)
	    floss_state.issue_end_cause[0] = ISSUE_AGE;

 	//  note the reason this inst didn't issue
	dist_unissued[UNISSUED_TOO_YOUNG]++;

	return false;
    }

    OpClass fu_type = i->opClass();

    // instructions that don't need a function unit usually don't make
    // it out of dispatch... the "faulting no-op" introduced on an
    // ifetch TLB miss is the exception.  Route it through an integer
    // ALU just to get it to WB.
    if (fu_type == No_OpClass) {
	assert(i->inst->fault != No_Fault);
	fu_type = IntAluOp;
    }

    unsigned thread = i->thread_number();

    fu_lat = FUPools[fu_pool_num]->getUnit(fu_type);


    assert(!i->inst->isInstPrefetch() &&
	   "Issued instruction prefetch! (should never do so)");

    if (fu_lat == -2)
	panic("Function unit type not available!");

    if (fu_lat >= 0) {
	issued = true;


	stat_issued_inst_type[i->thread_number()][i->opClass()]++;

	assert(!i->inst->isInstPrefetch() &&
	       "Instruction prefetch in issue stage!");
    } else {
	//
	//  No FU available
	//

	// Indicate that we couldn't issue because no fu available
	stat_fu_busy[fu_type]++;
	stat_fuBusy[fu_pool_num][fu_type]++;
	fu_busy[thread]++;

	if (floss_state.issue_end_cause[0] == ISSUE_CAUSE_NOT_SET) {
	    floss_state.issue_end_cause[0] = ISSUE_FU;
	    // only use element zero for ISSUE_FU
	    floss_state.issue_fu[0][0] = fu_type;
	}

	//  note the reason this inst didn't issue
	dist_unissued[fu_type]++;
    }

    if (issued) {
 	//  Inform the LSQ that this instruction is issuing
	LSQ->inform_issue(i);

	i->rob_entry->issued = true;

	/* Schedule writeback event after i->latency cycles unless:
	 * - it's a store (which completes immediately), indicated by
	 * i->rob_entry->completed already being set
	 * or
	 * - it's a cache miss load (for which the writeback event will
	 * be scheduled on a later callback from the memory system),
	 * indicated by a negative value for latency
	 */

	/* queue up the rob entry, since we're going to delete
	 *  the iq entry
	 *
	 *  even stores need to go through writeback...
	 *  (they need to have their entry removed from the lsq
	 *   and mark the rob entry as ready for the storebuffer)
	 */

	if (!i->rob_entry->completed && (fu_lat >= 0)) {
	    WritebackEvent *wb_event =
		new WritebackEvent(this, i->rob_entry);

	    // so we can invalidate on a squash
	    i->rob_entry->wb_event = wb_event;
	    wb_event->schedule(curTick + cycles(fu_lat));
	}

	(i->rob_entry)->iq_entry = NULL;	/* ROB ptr to IQ */
    }


    return issued;
}



//

bool
FullCPU::lsq_issue(BaseIQ::iterator i, unsigned fu_pool_num)
{
    bool issued = false;
    int fu_lat = 0;
    int latency = 0;
    bool store_inst = false;


    /*
     *  check to make sure that the instruction has spent enough time
     *  in the "issue stage" to be issued
     */
    if (curTick < (i->dispatch_timestamp + dispatch_to_issue_latency)) {

	if (floss_state.issue_end_cause[0] == ISSUE_CAUSE_NOT_SET)
	    floss_state.issue_end_cause[0] = ISSUE_AGE;

 	//  note the reason this inst didn't issue
	dist_unissued[UNISSUED_TOO_YOUNG]++;

	return false;
    }

    unsigned thread = i->thread_number();
    OpClass fu_type = i->opClass();

    //
    //  If this is the LSQ portion of a STORE.
    //
    //  (All stores pass through this code)
    //
    if (i->in_LSQ && i->inst->isStore()) {
	issued = true;
	latency = cycles(1);

	store_inst = true;
    } else if (fu_type != No_OpClass) {  // this "if" is probably a waste...

	//
	//  Load instructions require a functional unit
	//

	fu_type = i->opClass();
	fu_lat = FUPools[fu_pool_num]->getUnit(fu_type);

	if (fu_lat == -2) {
	    fatal("Function unit type not available!");
	}

	if (fu_lat >= 0) {

	    //  the default...
	    latency = cycles(fu_lat);

	    if (i->inst->isLoad()) {

		//
		//  Don't do anything if the cache is blocked
		//
		bool isPrefetch = i->inst->isDataPrefetch();
		if (!dcacheInterface->isBlocked()) {

		    //  Actually, unless the ALAT is being used, this op
		    //  will ALWAYS issue...

		    // LSQ part of load: May or may not issue.
		    // "latency" is returned via arg list... negative value
		    //    indicates that issue should not generate a WB event
		    if (isPrefetch)
			issued = issue_prefetch(i, &latency);
		    else
			issued = issue_load(i, &latency);
		} else {
		    //
		    //  Set cause the first time through only
		    //
		    if (floss_state.issue_end_cause[0] == ISSUE_CAUSE_NOT_SET)
		    {
			floss_state.issue_end_cause[0] = ISSUE_MEM_BLOCKED;

			//  FIXME!
			// This information needs to come out of the cache
			floss_state.issue_mem_result[0] = MA_MSHRS_FULL;
		    }

		    //  note the reason this inst didn't issue
		    dist_unissued[UNISSUED_CACHE_BLOCKED]++;
		}
	    } else {
		//
		//  Store instruction... Issue to FU
		//
		issued = true;
	    }

	    if (issued) {
		// reserve the functional unit
		stat_issued_inst_type[i->thread_number()][i->opClass()]++;
	    }

	}
	else {
	    //
	    //  No FU available
	    //

	    // Indicate that we couldn't issue because no fu available
	    stat_fu_busy[fu_type]++;
	    stat_fuBusy[fu_pool_num][fu_type]++;
	    fu_busy[thread]++;


	    if (floss_state.issue_end_cause[0] == ISSUE_CAUSE_NOT_SET) {
		floss_state.issue_end_cause[0] = ISSUE_FU;
		// only use element zero for ISSUE_FU
		floss_state.issue_fu[0][0] = fu_type;
	    }

	    //  note the reason this inst didn't issue
	    dist_unissued[fu_type]++;
	}
    }
    else {
	issued = true;
	latency = cycles(1);
    }


    //
    //  If the instruction was issued...
    //
    if (issued) {

	//  Inform the IQ that this instruction is issuing
	//
	//  NOTE: Store instructions whose data value is ready prior to
	//        the EA will NOT pass through here!
	//
	//  we do this later, when we inform all clusters
	//  that this inst issued...
	//      IQ[i->rob_entry->queue_num]->inform_issue(i);

 	i->rob_entry->issued = true;


	// Schedule writeback event after i->latency cycles unless:
	//   - it's a cache miss load
	//     (for which the writeback event will be scheduled on a later
	//      callback from the memory system)
	//     ==> indicated by a negative value for latency
	//   - it's a load prefetch (these get marked completed when issued)
	//     (note that store prefetches that potentially modify cache data,
	//      e.g. wh64, still get writeback events, as they need to be
	//      ordered w.r.t. other stores)
	//
	//  even stores need to go through writeback...
	//  (they need to have their entry removed from the lsq
	//   and mark the rob entry as ready for the storebuffer)
	//

	if (latency < 0 || (i->inst->isDataPrefetch() && i->inst->isLoad())) {
	    //
	    //  Do not create a writeback event
	    //
	} else {
	    WritebackEvent *wb_event =
		new WritebackEvent(this, i->rob_entry);

	    // so we can invalidate on a squash
	    i->rob_entry->wb_event = wb_event;
            assert(latency >= clock);
	    wb_event->schedule(curTick + latency);
	}

	if (DTRACE(Pipeline)) {
	    string s;
	    i->inst->dump(s);
	    DPRINTF(Pipeline, "Issue %s\n", s);
	}
    }

    return issued;
}

//


//
//  Find the Input Dependants that are not ready... not the type of
//  function unit that produces the operand
//
bool
FullCPU::find_idep_to_blame(BaseIQ::iterator inst, int thread)
{
    bool found_fu = false;

    if (!inst->in_LSQ) {
	for (int i = 0; i < inst->num_ideps; ++i) {
	    if (!inst->idep_ready[i]) {
		found_fu = true;
		ROBStation *rob = inst->idep_ptr[i]->rob_producer;

		OpClass op_class = rob->inst->opClass();

		assert(op_class != No_OpClass);
		floss_state.issue_fu[thread][i] = op_class;
	    }
	    else {
		floss_state.issue_fu[thread][i] = No_OpClass;
	    }
	}
    } else {
	//
	//  If the problem is an instruction from the LSQ
	//
	if (inst->inst->isStore() && !inst->idep_ready[STORE_DATA_INDEX]) {

	    //  store is waiting for its data...
	    ROBStation *rob
		= inst->idep_ptr[STORE_DATA_INDEX]->rob_producer;
	    floss_state.issue_fu[thread][STORE_DATA_INDEX]
		= rob->inst->opClass();

	    found_fu = true;
	}

	//  This works for both loads & stores
	if (!inst->idep_ready[MEM_ADDR_INDEX]) {
	    floss_state.issue_fu[thread][MEM_ADDR_INDEX] = IntAluOp;

	    found_fu = true;
	}
    }

    return found_fu;
}


//

class IQList
{
  private:
    unsigned number_of_queues;
    vector<bool> done_with_queue;
    unsigned num_done;

  public:
    IQList(unsigned num_iq) {
	number_of_queues = num_iq;
	done_with_queue.resize(number_of_queues, false);
	num_done = 0;
    }

    bool allDone() {return num_done == number_of_queues;}

    bool markDone(unsigned q) {
	assert(q < number_of_queues);

	if (!done_with_queue[q]) {
	    done_with_queue[q] = true;
	    ++num_done;
	}

	return allDone();
    }

    bool done(unsigned q) {
	assert(q < number_of_queues);
	return done_with_queue[q];
    }

    //  find the next NOT Done queue
    //  return false if there's a problem
    bool next(unsigned &q) {
	assert(q < number_of_queues);
	assert(num_done < number_of_queues);
	if (number_of_queues == 1) {
	    return false;
	}

	bool done = false;
	bool rv = true;
	unsigned next_queue = q;
	do {
	    //  increment to the NEXT index (w/ wrap)
	    next_queue = (next_queue + 1) % number_of_queues;

	    //  if we've wrapped all the way around, deal with it
	    if (next_queue == q) {
		done = true;
		rv = false;
	    }
	    else {
		//  haven't wrapped yet ...

		//  if this queue is not done, return it...
		if (!done_with_queue[next_queue]) {
		    q = next_queue;
		    done = true;
		}
	    }
	} while (!done);

	return rv;
    }
};



//   attempt to issue all operations in the ready queue; insts in the
//   ready instruction queue have all register dependencies satisfied,
//   this function must then 1) ensure the instructions memory
//   dependencies have been satisfied (see lsq_refresh() for details
//   on this process) and 2) a function unit is available in this
//   cycle to commence execution of the operation; if all goes well,
//   the function unit is allocated, a writeback event is scheduled,
//   and the instruction begins execution

void
FullCPU::issue()
{
    int n_issued;
    bool done_with_sb = false;
    bool done_with_iq = false;
    bool done_with_lsq = false;

    unsigned issued_by_thread[SMT_MAX_THREADS];

    unsigned iq_count = 0;
    unsigned lsq_count = 0;
    unsigned sb_count = 0;

    enum inst_source {none, iq, lsq, sb};

    inst_source source;

    BaseIQ::iterator evil_inst = 0;

    BaseIQ::rq_iterator *iq_rq_iterator = new BaseIQ::rq_iterator[numIQueues];

#ifdef DEBUG_ISSUE
    std::cerr << "-------------------------------" << std::endl;
    std::cerr << " ISSUE cycle: " << curTick << std::endl;
    std::cerr << "-------------------------------" << std::endl;
#endif

    //  for each queue, the list of RQ iterators to instructions belonging
    //  to the high-priority thread
    list<BaseIQ::rq_iterator> *hp_rq_it_list
	= new list<BaseIQ::rq_iterator>[numIQueues];


    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	issued_by_thread[i] = 0;


    BaseIQ::rq_iterator lsq_rq_iterator  = LSQ->issuable_list();
    StoreBuffer::rq_iterator sb_rq_iterator = storebuffer->issuable_list();


    //  We will start issuing out of this IQ
    unsigned current_iq = issue_starting_iqueue;
    unsigned current_fu_pool = issue_starting_fu_pool;

    //  Increment the last_iq index (RR fashion) for next cycle
    issue_starting_iqueue = (issue_starting_iqueue+1) % numIQueues;
    issue_starting_fu_pool = (issue_starting_fu_pool+1) % numFUPools;


    // visit all ready instructions (i.e., insts whose register input
    // dependencies have been satisfied, stop issue when no more instructions
    // are available or issue bandwidth is exhausted
    n_issued = 0;


    //
    //  Check to see if we need to change HP thread
    //
    if (prioritize_issue && (hp_thread_change <= curTick)) {
	hp_thread = (hp_thread+1) % number_of_threads;

	// schedule the next change
	hp_thread_change = curTick + issue_thread_weights[hp_thread];
    }


#if DUMP_ISSUE
    cerr << "@" << curTick << endl;

    list<issue_info> issued_list;
#endif

    //
    //  If there are no ready instructions, blame the stopage on the oldest
    //  instruction found in the IQ or LSQ.
    //
    //     THERE ARE THREE SUB-CASES:
    //       (a)  No instructions anywhere  ==> ISSUE_NO_INSN
    //       (a)  The oldest instruction has not-ready operands
    //              => ISSUE_DEPS
    //       (b)  The oldest instruction is ops-ready
    //              => ISSUE_IQ
    //
    unsigned n_ready = IQNumReadyInstructions() + LSQ->ready_count();
    if (n_ready == 0) {

	if ((IQNumInstructions() == 0) && (LSQ->count() == 0)) {
	    floss_state.issue_end_cause[0] = ISSUE_NO_INSN;
	} else {
	    //  Find the oldest instruction....
	    evil_inst = IQOldestInstruction();
	    if (evil_inst.notnull()) {
		if (LSQ->oldest().notnull()) {
		    if (LSQ->oldest()->seq < evil_inst->seq) {
			evil_inst = LSQ->oldest();
		    } else {
			// existing IQ inst is older
		    }
		} else {
		    // existing IQ inst older than non-existant LSQ
		}
	    } else {
		//  LSQ oldest since IQ doesn't exist
		evil_inst = LSQ->oldest();

		//  since we've already decided that we MUST have at least
		//  one instruction, and it's not in the IQ...
		assert(evil_inst.notnull());
	    }

	    if (evil_inst->ops_ready()) {
		floss_state.issue_end_cause[0] = ISSUE_IQ;
	    } else {
		//  instruction is waiting on dependencies...
		//  make a note of each fu-type that is producing the data
		//  that this instruction is waiting for
		floss_state.issue_end_cause[0] = ISSUE_DEPS;
		if (!find_idep_to_blame(evil_inst, 0))
		    floss_state.issue_end_cause[0] = ISSUE_IQ;
	    }
	}

        //  If the store-ready-queue is empty, then we're done
        if (sb_rq_iterator.isnull()) {
	    delete[] iq_rq_iterator;
	    delete[] hp_rq_it_list;
            return;
        }
    }


    //
    //  Main issue loop
    //
    //  This loop executes once for each instruction that is considered
    //  for issue. Instructions can issue from the IQ, LSQ, or Storebuffer.
    //  The process is:
    //
    //    (0)  Extract the status of the ready-lists for each IQ
    //    (1)  Extract the oldest instruction from each of the three
    //         ready queues
    //    (2)  Choose the oldest instruction of these three and indicate
    //         the selection with the "source" variable
    //    (3)  Attempt to issue the instruction
    //    (4)  Collect statistical data on what happened in #3
    //    (5)  Check the status of the current queue's ready-list, then
    //         rotate to the next queue
    //    (6)  Check to see if the issue process is complete for this cycle
    //

    //
    //  STEP 0:
    //
    //  This should help our speed through the IQ-selection logic
    //
    IQList done_list(numIQueues);

    //  we don't check BW here, since all should have BW available
    //  at the start of the issue process
    for (int i = 0; i < numIQueues; ++i)
	if (IQ[i]->ready_count() == 0)
	    done_list.markDone(i);

    done_with_iq = done_list.allDone();

    bool *hp_done = new bool[numIQueues];

    //
    //  Point to the issuable lists
    //  Populate the HP thread lists too...
    //
    for (int q = 0; q < numIQueues; ++q) {
	if (! done_list.done(q)) {
	    if (prioritize_issue) {
		BaseIQ::rq_iterator i;

		//  Walk the list for each queue, putting the RQ iterators to
		//  the high-priority thread into the list for this queue

		i = IQ[q]->issuable_list();
		while (i.notnull()) {

		    //  add RQ iterator to list if it's the HP thread
		    if ((*i)->inst->thread_number == hp_thread)
			hp_rq_it_list[q].push_back(i);

		    i = i.next();
		}
	    }

	    //  save this for non-prioritized issue
	    iq_rq_iterator[q] = IQ[q]->issuable_list();
	}

	hp_done[q] = false;
    }

    do {
	InstSeqNum seq_num = 0;
	DynInst *inst = 0;
	unsigned thread = 0;
	Tick pred_issue_cycle = 0;
	Tick ready_ts = 0;
	Tick dispatch_ts = 0;
	OpClass op_class = No_OpClass;
	bool issued = false;

	unsigned issue_events = 0;
	InstSeqNum fetch_seq = 0;

	BaseIQ::iterator iq_it = 0, lsq_it = 0;
	StoreBuffer::iterator sb_it = 0;

	InstSeqNum oldestIQInstSeq = 0;
	int           oldestIQIndex = -1;

	//
	//  STEP #1:  Convert from RQ iterator to Queue iterator
	//
	//  If the RQ is empty, signal that we don't need to continue
	//  looking into that structure...
	//
	//  We also catch the case where we are out of instructions
	//
	if (!done_with_iq) {

	    //
	    //  PRIORITIZED ISSUE:
	    //     set iq_rq_iterator[current_iq] to the next HP instruction,
	    //     if it exists, otherwise,
	    //
	    if (prioritize_issue) {

		//  if there are elements in the high-priority list
		if (hp_rq_it_list[current_iq].size()) {

		    // get the next instruction
		    iq_rq_iterator[current_iq]
			= hp_rq_it_list[current_iq].front();

		    // remove it from this list (still in RQ)
		    hp_rq_it_list[current_iq].pop_front();
		} else {
		    if (!hp_done[current_iq]) {
			// otherwise, look through the remaining RQ entries...
			iq_rq_iterator[current_iq]
			    = IQ[current_iq]->issuable_list();

			hp_done[current_iq] = true;
		    } else {
			// we've already set up the iq_rq_iterator[] entry
			// don't mess with it again
		    }
		}
	    }


	    //
	    //  Try to grab an iterator to the first ready IQ instruction
	    //  we can issue
	    //
	    if (iq_rq_iterator[current_iq].notnull()) {

		if (IQ[current_iq]->issue_bw()) {
		    //  This iterator points to the first instruction in the
		    //  ready-queue
		    iq_it = *iq_rq_iterator[current_iq];
		} else {
		    //  this cluster ran out of bandwidth
		    SET_FIRST_FLOSS_CAUSE(floss_state.issue_end_cause[0],
					  ISSUE_BW);

		    done_with_iq = done_list.markDone(current_iq);

		    if (!done_with_iq) {
			//  get the next index...
			//  false return indicates none available
#ifndef NDEBUG
			bool avail =
#endif
			    done_list.next(current_iq);
			assert(avail);

			//  point to the first instruction in this ready-queue
			iq_it = *iq_rq_iterator[current_iq];
		    }
		}
	    } else {
		//
		//  No ready instructions in this cluster...
		//
		if (IQ[current_iq]->issue_bw() == 0) {

		    //  we have run out of bandwidth, but there were no
		    //  instructions to issue, so...
		    SET_FIRST_FLOSS_CAUSE(floss_state.issue_end_cause[0],
					  ISSUE_NO_INSN);
		} else {
		    //  Ready-List is empty, but BW is available...:
		    //    Either: (1) we have no instructions in the
		    //                issue window,
		    //            (2) or the existing instructions
		    //                have dependants
		    if (IQ[current_iq]->iw_count() == 0) {
			SET_FIRST_FLOSS_CAUSE(floss_state.issue_end_cause[0],
					      ISSUE_NO_INSN);
		    } else {
			// Empty ready-list, insts available ==> DEPS
			if (floss_state.issue_end_cause[0] ==
			    ISSUE_CAUSE_NOT_SET)
			{
			    floss_state.issue_end_cause[0] = ISSUE_DEPS;
			    evil_inst = IQ[current_iq]->oldest();
			    if (!find_idep_to_blame(evil_inst, 0))
				floss_state.issue_end_cause[0] = ISSUE_IQ;
			}
		    }
		}


		done_with_iq = done_list.markDone(current_iq);

		if (!done_with_iq) {
		    //  get the next index...
		    //  false return indicates none available
#ifndef NDEBUG
		    bool avail =
#endif
			done_list.next(current_iq);
		    assert(avail);

		    //  point to the first instruction in this ready-queue
		    iq_it = *iq_rq_iterator[current_iq];
		}
	    }


	    //
	    //  Find the IQ with the oldest instruction that we can issue
	    //
	    if (!done_with_iq) {
		for (int i = 0; i < numIQueues; ++i) {
		    if (!done_list.done(i)) {
			BaseIQ::iterator j = *iq_rq_iterator[i];

			if (j->seq < oldestIQInstSeq || oldestIQIndex == -1) {
			    oldestIQInstSeq = j->seq;
			    oldestIQIndex = i;
			}
		    }
		}
#if ISSUE_OLDEST
		current_iq = oldestIQIndex;
		//  point to the first instruction in this ready-queue
		iq_it = *iq_rq_iterator[current_iq];
#endif
	    }


	    //  Handle the case where we need a 1:1 relationship between
	    //  IQ's and FU pools
	    if (numIQueues == numFUPools)
		current_fu_pool = current_iq;
	}


	if (!done_with_lsq) {
	    if (lsq_rq_iterator.notnull())
		lsq_it = *lsq_rq_iterator;
	    else
		done_with_lsq = true;
	}

	if (!done_with_sb) {
	    if (sb_rq_iterator.notnull())
		sb_it = *sb_rq_iterator;
	    else
		done_with_sb = true;
	}


	//
	//  Early out...
	//
	if (done_with_iq && done_with_lsq && done_with_sb)
	    break;

	//
	//  STEP #2: Select the source for our next issued instruction
	//
	source = none;
	if (!done_with_iq) {
	    seq_num = iq_it->seq;
	    source = iq;

	    if (!done_with_lsq && (seq_num > lsq_it->seq)) {
		seq_num = lsq_it->seq;
		source = lsq;
	    }

	    if (!done_with_sb && (seq_num > sb_it->seq)) {
		seq_num = sb_it->seq;
		source = sb;
	    }
	} else if (!done_with_lsq) {
	    seq_num = lsq_it->seq;
	    source = lsq;

	    if (!done_with_sb && (seq_num > sb_it->seq)) {
		seq_num = sb_it->seq;
		source = sb;
	    }
	} else if (!done_with_sb) {
	    seq_num = sb_it->seq;
	    source = sb;
	}

	if (issue_break && (issue_break == seq_num))
	    issue_breakpoint();

	//
        //  INORDER ISSUE: If seq_num is not the sequence number we expect,
        //                 stall issue.
	//
        if (inorder_issue) {
	    if ((seq_num != expected_inorder_seq_num) && (source != sb)) {
		// [DAG] Do sequence numbers wrap around?  They're long longs,
		// so they shouldn't!
		assert(expected_inorder_seq_num < seq_num);
		break;
	    }
        }


	//
	//  STEP #3:  Attempt to issue the instruction
	//
	switch (source) {
	  case iq:
	    op_class = iq_it->opClass();
	    inst     = iq_it->inst;
	    thread   = iq_it->inst->thread_number;

	    if (issued_by_thread[thread] < issue_bandwidth[thread]) {
		issued = iq_issue(iq_it, current_fu_pool);
#if DUMP_ISSUE
		string s;
		iq_it->inst->dump(s);
		issued_list.push_back(issue_info(iq_it->inst->fetch_seq, current_iq, s));
#endif
	    } else {
		issued = false;
		SET_FIRST_FLOSS_CAUSE(floss_state.issue_end_cause[0],
				      ISSUE_BW);
	    }

	    if (issued) {
		pred_issue_cycle = iq_it->pred_issue_cycle;
		ready_ts         = iq_it->ready_timestamp;
		dispatch_ts      = iq_it->dispatch_timestamp;

		fetch_seq        = inst->fetch_seq;

		if (iq_it->ea_comp)
		    issue_events = PipeTrace::AddressGen;

		//
		//  We have to let everyone else know that an
		//  instruction has issued
		//
		for (unsigned i = 0; i < numIQueues; ++i)
		    if (i != current_iq)
			IQ[i]->inform_issue(iq_it);
#if 0
		if (chainGenerator)
		    chainGenerator->removeInstruction(iq_it->rob_entry);
#endif

		//  Inform the IQ that this instruction has issued
		//  and update the RQ iterator for this queue
		iq_rq_iterator[current_iq]
		    = IQ[current_iq]->issue(iq_rq_iterator[current_iq]);

		//  Clear ROB pointer to this inst
		iq_it->rob_entry->iq_entry = 0;

		//  If this entry has a pointer to an LSQ entry...
		if (iq_it->lsq_entry.notnull()) {
		    // clear the pointer from the LSQ entry to this IQ entry
		    iq_it->lsq_entry->iq_entry = 0;

		    // clear the pointer to the LSQ entry
		    iq_it->lsq_entry = 0;
		}

		++iq_count;

		//  we've removed one ready instrction from the IQ
		--n_ready;
	    } else {
		//  this instruction didn't issue, so point to the next one
		iq_rq_iterator[current_iq] = iq_rq_iterator[current_iq].next();
	    }
	    break;

	  case lsq:
	    op_class = lsq_it->opClass();
	    inst     = lsq_it->inst;
	    thread   = lsq_it->inst->thread_number;

	    if ((lsq_it->seq > oldestIQInstSeq) && (oldestIQInstSeq != 0)) {
		++lsqInversion;
		current_iq = (current_iq + 1) % numIQueues;
		source = none;
		break;  // don't issue it...
		        // we'll get the ordering right next time...
	    }

	    if (issued_by_thread[thread] < issue_bandwidth[thread]) {

		// LSQ portions issue into the same pool as the EA-comp if we
		// have a 1:1 relationship between IQ's and FU Pools
		if (numIQueues == numFUPools)
		    issued = lsq_issue(lsq_it, lsq_it->rob_entry->queue_num);
		else
		    issued = lsq_issue(lsq_it, current_fu_pool);
	    } else {
		issued = false;
		SET_FIRST_FLOSS_CAUSE(floss_state.issue_end_cause[0],
				      ISSUE_BW);
	    }

	    if (issued) {
		pred_issue_cycle = lsq_it->pred_issue_cycle;
		ready_ts         = lsq_it->ready_timestamp;
		dispatch_ts      = lsq_it->dispatch_timestamp;

		fetch_seq        = inst->fetch_seq;
		if (lsq_it->mem_result != MA_HIT)
		    issue_events = PipeTrace::CacheMiss;

		//  Clean up this instruction
		lsq_rq_iterator = LSQ->issue(lsq_rq_iterator);

		//
		//  We have to let the IQ's know that an
		//  instruction has issued
		//
		for (unsigned i = 0; i < numIQueues; ++i)
		    IQ[i]->inform_issue(lsq_it);

		// Clear IQ & ROB pointers to this inst
		lsq_it->rob_entry->lsq_entry = 0;  // ROB

		++lsq_count;

		//  we've removed one ready instrction from the LSQ
		--n_ready;
	    } else {
		// If it was a store (EA comp part) it should have issued
		// ==> doesn't work if we are limiting issue bandwidth...


		if (dcacheInterface->isBlocked())
		    done_with_lsq = true;
		else
		    lsq_rq_iterator = lsq_rq_iterator.next();
	    }
	    break;

	  case sb:
	    //  If we have a 1:1 correspondence between FU's and
	    //  IQ's, then we issue to the FU pool that matches the
	    //  EA-Comp portion of this store
	    if (numIQueues == numFUPools)
		issue_current_fupool_for_sb = sb_it->queue_num;

	    issued = sb_issue(sb_it, issue_current_fupool_for_sb);

	    thread   = sb_it->thread_number();
	    op_class = MemWriteOp;

	    if (issued) {
		fetch_seq = sb_it->fetch_seq;

		//  Remove current element, and return pointer to next
		sb_rq_iterator = storebuffer->issue(sb_rq_iterator);

		// Mark a copy storebuffer as complete, since we don't
		// need a response currently
		if (sb_it->isCopy) {
		    storebuffer->completeStore(sb_it);
		}

		//  each load attempts to issue to the "next" pool
		issue_current_fupool_for_sb
		    = (issue_current_fupool_for_sb+1) % numFUPools;

		++sb_count;
	    } else {
		//  If we can't issue this one... don't worry about the
		//  rest...
		done_with_sb = true;
	    }
	    break;

	  case none:
	    //
	    //  We get here when NO instruction tries to issue on this pass
	    //  through the loop.  This can happen when we hit an empty IQ
	    //  before we empty all the IQ's
	    //
	    break;
	}


	//
	//  STEP #4:
	//
	//  Do book-keeping only if we tried to issue from IQ or LSQ on this
	//  iteration
	//
	if ((source == iq) || (source == lsq)) {
	    if (issued) {

		//
		//  Pipetrace this instruction if it issued...
		//
		//  This doesn't quite work the way we might want it:
		//    - The fourth parameter (latency) can't be filled in,
		//      since we don't actually know how long it will take to
		//      service the cache miss...
		//    - The fifth parameter (longest latency event) is designed
		//      to indicate which of several events took the longest...
		//      since our model doesn't generate multiple events, this
		//      isn't used here either.
		//
		if (ptrace) {
		    unsigned load_latency = 0;
		    unsigned longest_event = 0;
		    ptrace->moveInst(inst, PipeTrace::Issue, issue_events,
				     load_latency, longest_event);
		}

#ifdef DEBUG_ISSUE
		std::cerr << "Issued instruction " << fetch_seq;
		if (inst != 0) {
		    std::cerr << " 0x" << std::hex
			      << inst->PC << std::dec << " (seq "
			      << inst->fetch_seq << " spec mode "
			      << inst->spec_mode << "): ";
		    std::cerr << inst->staticInst->disassemble(inst->xc->PC);
		}
		std::cerr << std::endl;
#endif


		//
		//  RR for FU pool if there isn't a 1:1 match between
		//  Queues and FU pools...
		//
		if (numIQueues != numFUPools)
		    current_fu_pool = (current_fu_pool + 1) % numFUPools;

		++n_issued;
		++issued_by_thread[thread];

		++expected_inorder_seq_num;

		issue_delay_dist[inst->opClass()]
		    .sample(curTick - ready_ts);

		queue_res_dist[inst->opClass()]
		    .sample(curTick - dispatch_ts);

		//
		//  Update INSTRUCTION statistics
		//
		if (source == iq)
		    update_exe_inst_stats(inst);

		//
		//  Update OPERATION statistics
		//
		++issued_ops[thread];
	    }
	}

	//
	//  STEP #5:
	//
	//  Rotate through the IQ's
	//  (first to check to see if this ready-list is empty)
	//
	if (source == iq) {
	    bool no_rdy_insts = iq_rq_iterator[current_iq].isnull();
	    bool no_bw        = (IQ[current_iq]->issue_bw() == 0);

	    if (no_rdy_insts || no_bw) {
		//  mark this queue done, check for all done
		done_with_iq = done_list.markDone(current_iq);

		if (no_rdy_insts) {
		    if (IQ[current_iq]->iw_count()) {
			// iq not empty, but no ready insts...
			if (floss_state.issue_end_cause[0] ==
			    ISSUE_CAUSE_NOT_SET)
			{
			    //
			    //  Either the oldest instruction is waiting for
			    //  it's inputs or some other IQ-related problem
			    //
			    evil_inst = IQ[current_iq]->oldest();
			    if (!evil_inst->ops_ready()) {
				floss_state.issue_end_cause[0] = ISSUE_DEPS;
				if (!find_idep_to_blame(evil_inst, 0))
				    floss_state.issue_end_cause[0] = ISSUE_IQ;
			    } else {
				floss_state.issue_end_cause[0] = ISSUE_IQ;
			    }
			}
		    }
		    else {
			// iq empty
			SET_FIRST_FLOSS_CAUSE(floss_state.issue_end_cause[0],
					      ISSUE_NO_INSN);
		    }
		}
		else {
		    //
		    //  Must have been BW...
		    //
		    SET_FIRST_FLOSS_CAUSE(floss_state.issue_end_cause[0],
					  ISSUE_BW);
		}
	    }

#if ISSUE_OLDEST == 0
	    if (!done_with_iq)
		current_iq = (current_iq + 1) % numIQueues;
#endif
	}


	//
	//  STEP #6:
	//
	//  When we run out of bandwidth, indicate
	//  that we are done issuing from the IQ & LSQ
	//
	if (n_issued >= issue_width) {
	    done_with_iq = true;
	    done_with_lsq = true;
	}


    } while (!done_with_iq || !done_with_sb || !done_with_lsq);


    //
    //  Delete the allocated lists...
    //
    delete[] iq_rq_iterator;
    delete[] hp_rq_it_list;
    delete[] hp_done;


#if DUMP_ISSUE
    if (!issued_list.empty()) {
	issued_list.sort();

	list<issue_info>::iterator i = issued_list.begin();
	list<issue_info>::iterator end = issued_list.end();
	for (; i != end; ++i)
	{
	    cerr << "   #" << i->seq << " (clust " << i->clust << ") "
		 << i->inst << endl;
	}
    }
#endif

    //
    //
    //  We've issued n_issued instructions from the IQ or LSQ
    //
    n_issued_dist[n_issued]++;

    if (n_issued == issue_width) {
	//
	//  Issue BW limit reached... blame loss on BW only if there are
	//  no ready instructions
	//
	if (floss_state.issue_end_cause[0] != ISSUE_IQ) {
	    //
	    //  NOTE:  these causes over-ride any previously-set cause
	    //
	    if (n_ready == 0) {
		if (IQNumInstructions() != 0) {
		    floss_state.issue_end_cause[0] = ISSUE_DEPS;

		    evil_inst = IQOldestInstruction();
		    if (!find_idep_to_blame(evil_inst, 0)) {
			floss_state.issue_end_cause[0] = ISSUE_IQ;
		    }
		} else {
		    floss_state.issue_end_cause[0] = ISSUE_NO_INSN;
		}
	    } else {
		floss_state.issue_end_cause[0] = ISSUE_BW;
	    }
	}
    } else {
	//
	//  We have issue BW left:
	//     (1) We must have tried to issue from the IQ
	//           ==> issue_end_cause is set in Sep #1
	//     (2) OR, we must have tried to issue from the LSQ
	//           ==> issue_end_cause may not be set
	//                 -> No instructions
	//                 -> No instructions ready
	//                 -> issue trouble... cause will be set in lsq_issue
	//     (3) OR, we only issued from the SB
	//           ==> issue_end_cause is set in Sep #1
	//     (4) OR, we were issuing inorder
	//           ==> ISSUE_INORDER
	//

	if (floss_state.issue_end_cause[0] == ISSUE_CAUSE_NOT_SET) {
	    if (LSQ->count() != 0) {
		floss_state.issue_end_cause[0] = ISSUE_DEPS;

		evil_inst = LSQ->oldest();
		if (!find_idep_to_blame(evil_inst, 0))
		    floss_state.issue_end_cause[0] = ISSUE_IQ;
	    } else {
		floss_state.issue_end_cause[0] = ISSUE_NO_INSN;
	    }
	}
    }

#if DEBUG_FLOSS
    if (floss_state.issue_end_cause[0] == ISSUE_CAUSE_NOT_SET)
        panic("cause not set");
#endif
}

//



void
FullCPU::issue_init()
{
    int i;

#if 0
    for (i = 0; i < SMT_MAX_THREADS; i++)
	store_info[i] = new(lsq_store_info_t)[LSQ_size];
#endif

    for (i = 0; i < Num_OpClasses; ++i)
	unissued_names[i] = opClassStrings[i];

    unissued_names[Num_OpClasses]   = "DCache_Blocked";
    unissued_names[Num_OpClasses + 1] = "Too_Young";

#if REMOVE_WP_LOADS
    cerr << "NOT Issuing misspeculated loads..." << endl;
#endif
}

void
FullCPU::issueRegStats()
{
    using namespace Stats;

    com_inst.resize(number_of_threads);
    com_loads.resize(number_of_threads);

    for (int i = 0; i < number_of_threads; ++i) {
	com_inst[i] = 0;
	com_loads[i] = 0;
    }

    stat_com_inst
	.init(number_of_threads)
	.name(name() + ".COM:count")
	.desc("Number of instructions committed")
	.flags(total)
	;

    stat_com_swp
	.init(number_of_threads)
	.name(name() + ".COM:swp_count")
	.desc("Number of s/w prefetches committed")
	.flags(total)
	;

    stat_com_refs
	.init(number_of_threads)
	.name(name() +  ".COM:refs")
	.desc("Number of memory references committed")
	.flags(total)
	;

    stat_com_loads
	.init(number_of_threads)
	.name(name() +  ".COM:loads")
	.desc("Number of loads committed")
	.flags(total)
	;

    stat_com_membars
	.init(number_of_threads)
	.name(name() +  ".COM:membars")
	.desc("Number of memory barriers committed")
	.flags(total)
	;

    stat_com_branches
	.init(number_of_threads)
	.name(name() + ".COM:branches")
	.desc("Number of branches committed")
	.flags(total)
	;

    //
    //  these will count the number of PROGRAM instructions
    //  (NOT "operations") issued... ie. EA-Comp ops are not counted
    //

    exe_inst
	.init(number_of_threads)
	.name(name() + ".ISSUE:count")
	.desc("number of insts issued")
	.flags(total)
	;

    exe_swp
	.init(number_of_threads)
	.name(name() + ".ISSUE:swp")
	.desc("number of swp insts issued")
	.flags(total)
	;

    exe_nop
	.init(number_of_threads)
	.name(name() + ".ISSUE:nop")
	.desc("number of nop insts issued")
	.flags(total)
	;

    exe_refs
	.init(number_of_threads)
	.name(name() + ".ISSUE:refs")
	.desc("number of memory reference insts issued")
	.flags(total)
	;

    exe_loads
	.init(number_of_threads)
	.name(name() + ".ISSUE:loads")
	.desc("number of load insts issued")
	.flags(total)
	;

    exe_branches
	.init(number_of_threads)
	.name(name() + ".ISSUE:branches")
	.desc("Number of branches issued")
	.flags(total)
	;

    issued_ops
	.init(number_of_threads)
	.name(name() + ".ISSUE:op_count")
	.desc("number of insts issued")
	.flags(total)
	;

    n_issued_dist
	.init(issue_width + 1)
	.name(name() + ".ISSUE:issued_per_cycle")
	.desc("Number of insts issued each cycle")
	.flags(total | pdf | dist)
	;

    fu_busy
	.init(number_of_threads)
	.name(name() + ".ISSUE:fu_busy_cnt")
	.desc("FU busy when requested")
	.flags(total)
	;

    stat_fu_busy
	.init(Num_OpClasses)
	.name(name() + ".ISSUE:fu_full")
	.desc("attempts to use FU when none available")
	.flags(pdf | dist)
	;
    for (int i=0; i < Num_OpClasses; ++i) {
	stat_fu_busy.subname(i, opClassStrings[i]);
    }


    stat_fuBusy
	.init(numIQueues, Num_OpClasses)
	.name(name() + ".ISSUE:IQ")
	.flags(pdf | dist)
	;

    for (int q = 0; q < numIQueues; ++q) {
	stringstream label,desc;
	label << q << ":fu_full";
	desc <<  " attempts to issue to FU from pool #" << q <<
	    "when none available";
	stat_fuBusy
	    .subname(q, label.str())
	    .subdesc(q, desc.str())
	    ;
    }
    stat_fuBusy.ysubnames(opClassStrings);

    dist_unissued
	.init(Num_OpClasses+2)
	.name(name() + ".ISSUE:unissued_cause")
	.desc("Reason ready instruction not issued")
	.flags(pdf | dist)
	;
    for (int i=0; i < (Num_OpClasses + 2); ++i) {
	dist_unissued.subname(i, unissued_names[i]);
    }

    stat_issued_inst_type
	.init(number_of_threads,Num_OpClasses)
	.name(name() + ".ISSUE:FU_type")
	.desc("Type of FU issued")
	.flags(total | pdf | dist)
	;
    stat_issued_inst_type.ysubnames(opClassStrings);

    //
    //  How long did instructions for a particular FU type wait prior to issue
    //

    issue_delay_dist
	.init(Num_OpClasses,0,99,2)
	.name(name() + ".ISSUE:")
	.desc("cycles from operands ready to issue")
	.flags(pdf | cdf)
	;

    for (int i=0; i<Num_OpClasses; ++i) {
	stringstream subname;
	subname << opClassStrings[i] << "_delay";
	issue_delay_dist.subname(i, subname.str());
    }

    //
    //  Other stats
    //
    lsq_forw_loads
	.init(number_of_threads)
	.name(name() + ".LSQ:forw_loads")
	.desc("number of loads forwarded via LSQ")
	.flags(total)
	;

    inv_addr_loads
	.init(number_of_threads)
	.name(name() + ".ISSUE:addr_loads")
	.desc("number of invalid-address loads")
	.flags(total)
	;

    inv_addr_swpfs
	.init(number_of_threads)
	.name(name() + ".ISSUE:addr_swpfs")
	.desc("number of invalid-address SW prefetches")
	.flags(total)
	;

    queue_res_dist
	.init(Num_OpClasses, 0, 99, 2)
	.name(name() + ".IQ:residence:")
	.desc("cycles from dispatch to issue")
	.flags(total | pdf | cdf )
	;
    for (int i = 0; i < Num_OpClasses; ++i) {
	queue_res_dist.subname(i, opClassStrings[i]);
    }

    lsq_blocked_loads
	.init(number_of_threads)
	.name(name() + ".LSQ:blocked_loads")
	.desc("number of ready loads not issued due to memory disambiguation")
	.flags(total)
	;

    lsqInversion
	.name(name() + ".ISSUE:lsq_invert")
	.desc("Number of times LSQ instruction issued early")
	;
}

void
FullCPU::issueRegFormulas()
{
    using namespace Stats;

    misspec_cnt
	.name(name() + ".ISSUE:misspec_cnt")
	.desc("Number of misspeculated insts issued")
	.flags(total)
	;
    misspec_cnt = exe_inst - stat_com_inst;

    misspec_ipc
	.name(name() + ".ISSUE:MSIPC")
	.desc("Misspec issue rate")
	.flags(total)
	;
    misspec_ipc = misspec_cnt / numCycles;

    issue_rate
	.name(name() + ".ISSUE:rate")
	.desc("Inst issue rate")
	.flags(total)
	;
    issue_rate = exe_inst / numCycles;

    issue_stores
	.name(name() + ".ISSUE:stores")
	.desc("Number of stores issued")
	.flags(total)
	;
    issue_stores = exe_refs - exe_loads;

    issue_op_rate
	.name(name() + ".ISSUE:op_rate")
	.desc("Operation issue rate")
	.flags(total)
	;
    issue_op_rate = issued_ops / numCycles;

    fu_busy_rate
	.name(name() + ".ISSUE:fu_busy_rate")
	.desc("FU busy rate (busy events/executed inst)")
	.flags(total)
	;
    fu_busy_rate = fu_busy / exe_inst;

    //
    //  Commit-stage statistics
    //  (we include these here because we _count_ them here)
    //

    commit_stores
	.name(name() + ".COM:stores")
	.desc("Number of stores committed")
	.flags(total)
	;
    commit_stores = stat_com_refs - stat_com_loads;

    commit_ipc
	.name(name() + ".COM:IPC")
	.desc("Committed instructions per cycle")
	.flags(total)
	;
    commit_ipc = stat_com_inst / numCycles;

    commit_ipb
	.name(name() + ".COM:IPB")
	.desc("Committed instructions per branch")
	.flags(total)
	;
    commit_ipb = stat_com_inst / stat_com_branches;

    lsq_inv_rate
	.name(name() + ".ISSUE:lsq_inv_rate")
	.desc("Early LSQ issues per cycle")
	;
    lsq_inv_rate = lsqInversion / numCycles;
}

/**************************************************************
 *                                                            *
 *  Collect instruction statistics                            *
 *                                                            *
 **************************************************************/
void
FullCPU::update_exe_inst_stats(DynInst *inst)
{
    int thread_number = inst->thread_number;

    //
    //  Pick off the software prefetches
    //
#ifdef TARGET_ALPHA
    if (inst->isDataPrefetch())
	exe_swp[thread_number]++;
    else
	exe_inst[thread_number]++;
#else
    exe_inst[thread_number]++;
#endif

    //
    //  Control operations
    //
    if (inst->isControl())
	exe_branches[thread_number]++;

    //
    //  Memory operations
    //
    if (inst->isMemRef()) {
	exe_refs[thread_number]++;

	if (inst->isLoad())
	    exe_loads[thread_number]++;
    }
}


