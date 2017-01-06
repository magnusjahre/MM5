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

/*
 * @file
 * Defines the portions of CPU relating to the fetch stages.
 */

#include <fstream>
#include <iostream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "base/cprintf.hh"
#include "base/loader/symtab.hh"
#include "base/range.hh"
#include "encumbered/cpu/full/bpred.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/dd_queue.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/fetch.hh"
#include "encumbered/cpu/full/floss_reasons.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/issue.hh"
#include "encumbered/cpu/full/spec_state.hh"
#include "encumbered/cpu/full/thread.hh"
#include "mem/functional/memory_control.hh"
#include "mem/mem_interface.hh"
#include "sim/param.hh"
#include "sim/sim_exit.hh"
#include "sim/stats.hh"

#if FULL_SYSTEM
#include "sim/system.hh"
#include "targetarch/vtophys.hh" // can get rid of this when we fix translation
#endif

using namespace std;

/*
 *  Local function prototypes
 */

static int rr_compare(const void *first, const void *second);
static int icount_compare(const void *first, const void *second);

// ==========================================================================
//
//     FetchQueue Implementation
//
//

void
FetchQueue::init(FullCPU *cpu, int _size, int _num_threads)
{
    instrs = new fetch_instr_rec_t[_size];
    head = tail = 0;
    size = _size;
    index_mask = _size - 1;
    // size must be power of two
    if ((_size & index_mask) != 0)
	fatal("fetch_queue: size %d not a power of two!\n",
	      _size);

    num_threads = _num_threads;

    mt_frontend = cpu->mt_frontend;

    num_valid =
	num_reserved =
	num_squashed = 0;

    for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	num_reserved_thread[i] = 0;
	num_valid_thread[i] = 0;
	num_squashed_thread[i] = 0;
    }

    for (int i = 0; i < _size; ++i)
	instrs[i].inst = NULL;
}

void
FetchQueue::reserve(int thread)
{
    assert((mt_frontend && thread == num_threads) ||
	   (!mt_frontend && thread >= 0 && thread <= num_threads));
    assert(num_total() < size);

    num_reserved++;
    num_reserved_thread[thread]++;
}


// append an instruction from an IcacheOutputBuffer to the ifq.
// a slot should already have been reserved by incrementing the
// num_reserved and num_reserved_thread[] counters
void
FetchQueue::append(DynInst *inst)
{
    int thread_number = inst->thread_number;

    fetch_instr_rec_t *frec = &instrs[tail];
    tail = incr(tail);

    frec->inst = inst;
    frec->thread_number = thread_number;
    frec->squashed = false;

    frec->contents_valid = true;

    --num_reserved;
    --num_reserved_thread[thread_number];

    ++num_valid;
    ++num_valid_thread[thread_number];

    assert(num_reserved >= 0);

    // don't need to increment num_valid or num_valid_thread[]
    // since these counts already include reserved slots
}


DynInst *
FetchQueue::pull()
{
    DynInst *rv = instrs[head].inst;

    if (!instrs[head].squashed) {
	//
	//  instruction was not squashed...
	//
	--num_valid;
	--num_valid_thread[rv->thread_number];

	instrs[head].inst = 0;
	head = incr(head);
    } else {
	//
	//  instruction WAS squashed...
	//
	--num_squashed;
	--num_squashed_thread[instrs[head].thread_number];

	head = incr(head);
    }

    return rv;
}


//
//   Overloaded Function ! ! !
//

//    This version squashes everything in this queue
void
FetchQueue::squash()
{
    assert(mt_frontend);

    if (num_valid) {
	int idx = head;
	do {
	    if (!instrs[idx].squashed) {
		unsigned t = instrs[idx].thread_number;
		assert(mt_frontend && t == num_threads);

		--num_valid;
		--num_valid_thread[t];

		++num_squashed;
		++num_squashed_thread[t];

		instrs[idx].squash();
	    }

	    idx = incr(idx);

	} while (idx != tail);

	assert(num_valid == 0);
    }

    num_reserved = 0;

    assert(num_valid_thread[num_threads] == 0);
    num_reserved_thread[num_threads] = 0;
}


//
// This version squashes instructions from the specified thread in this queue
//
//  should only be used in the non-MT frontend case
//
void
FetchQueue::squash(int t)
{
    assert(!mt_frontend && t >= 0 && t <= num_threads);

    if (num_valid) {
	int idx = head;
	do {
	    if (!instrs[idx].squashed && (instrs[idx].thread_number == t)) {

		instrs[idx].squash();

		--num_valid;
		--num_valid_thread[t];

		++num_squashed;
		++num_squashed_thread[t];
	    }

	    idx = incr(idx);
	} while (idx != tail);

	assert(num_valid_thread[t] == 0);
    }

    num_reserved -= num_reserved_thread[t];
    num_reserved_thread[t] = 0;
}


void
FetchQueue::dump(const string &str)
{
    cprintf("=======================================================\n");
    cprintf("Contents Of Fetch Queue%s:\n", str);
    cprintf("fetch_num: %d (%d valid)(%d reserved)(%d squashed)\n",
	    num_total(), num_valid, num_reserved, num_squashed);

    cprintf("-------------------------------------------------------\n");
    cprintf("fetch_head: %d, fetch_tail: %d\n", head, tail);
    cprintf("-------------------------------------------------------\n");

    for (int i = 0, idx = head; i < num_total(); i++, idx = incr(idx)) {
        fetch_instr_rec_t *frec = &(instrs[idx]);
        DynInst *inst = frec->inst;

	if (frec->squashed) {
	    cprintf("%2d:             <squashed>\n", idx);
	} else {
	    cprintf("%2d: (Thread %d) ", idx, frec->thread_number);
	    if (inst)
		inst->dump();
	    else
		cprintf("RESERVED\n");
	}
    }

    cprintf("=======================================================\n\n");
}

// ==========================================================================

/*
 * The icache_output_buffer structures are per-thread structures that
 * serve as the destination for outstanding icache accesses.  We read
 * the actual instructions from memory when we initiate the fetch (in
 * order to have perfect prediction information); these buffers hold
 * those instructions while we wait for the actual icache access
 * delay.  We can't have the ifetch queue serve this purpose, since
 * icache accesses from different threads may complete out of order,
 * and we want to put instructions in the ifq in the order they come
 * back from the icache.  The icache fetch completion event
 * (FetchCompleteEvent) copies the instructions from the
 * icache_output_buffer to the ifq when it is processed.
 */
struct IcacheOutputBufferEntry
{
    DynInst *inst;
    bool ready;

    // constructor
    IcacheOutputBufferEntry() {
	inst = NULL;
	ready = false;
    }
};


struct IcacheOutputBuffer
{
    IcacheOutputBufferEntry *insts;
    short head;
    short tail;
    short index_mask;
    short num_insts;
    short size;
    int   thread;

    // initialization
    void init(int _size, int _thread) {
	head = tail = 0;
	num_insts = 0;
	insts = new IcacheOutputBufferEntry[_size];
	index_mask = _size - 1;
	// size must be power of two
	if ((_size & index_mask) != 0)
	    fatal("icache_output_buffer: size %d not a power of two!\n",
		  _size);
	size = _size;
        thread = _thread;
    }

    // number of available slots
    int free_slots() {
	return size - num_insts;
    }

    // increment tail pointer & return tail entry
    IcacheOutputBufferEntry *new_tail() {
	IcacheOutputBufferEntry *entryp = &insts[tail];
	tail = (tail + 1) & index_mask;
	num_insts++;
	return entryp;
    }

    // increment a queue index (with wrap)
    int incr(int index) {
	return ((index + 1) & index_mask);
    }

    // squash all instructions
    void squash(int thread_number);

    void dump();
};


//void fetch_squash_inst(DynInst *inst);

void
IcacheOutputBuffer::dump()
{
    ccprintf(cerr,
	     "=========================================================\n"
	     "I-Cache Output Buffer (Thread %d)\n"
	     "---------------------------------------------------------\n"
	     "Head=%d, Tail=%d\n"
	     "---------------------------------------------------------\n",
	     thread, head, tail);
    for (int i = 0, idx = head; i < num_insts; ++i, idx = incr(idx)) {
	DynInst *inst = insts[idx].inst;

        ccprintf(cerr, "%2d: PC %#08x, Pred_PC %#08x: ",
		 idx, inst->PC, inst->Pred_PC);
        cerr << inst->staticInst->disassemble(inst->PC);
        cerr << "\n";
    }

    cerr << "=========================================================\n";
}

void
IcacheOutputBuffer::squash(int thread_number)
{
    for (int i = 0, idx = head; i < num_insts; ++i, idx = incr(idx)) {
	IcacheOutputBufferEntry *entryp = &insts[idx];

	entryp->inst->squash();
	delete entryp->inst;
	entryp->inst = NULL;

	entryp->ready = false;
    }

    // buffer is now empty
    head = tail;
    num_insts = 0;

}



// ==========================================================================

//
//  This routine does double-duty...
//    (1)  Complete initialization of the fetch-list and thread_info structs
//    (2)  Re-initilizes the fetch-list on a state change
//
void
FullCPU::initialize_fetch_list(int initial)
{
    int active_list[SMT_MAX_THREADS];
    int inactive_list[SMT_MAX_THREADS];

    ThreadListElement temp_list[SMT_MAX_THREADS];

    //
    //  Initialize the state of the fetch-list & thread_info struct
    //
    for (int i = 0; i < SMT_MAX_THREADS; i++) {
	if (initial) {
	    fetch_list[i].thread_number = i;
	    fetch_list[i].sort_key = 0;
	    fetch_list[i].blocked = 0;
	    fetch_list[i].priority = 0;
	    fetch_list[i].last_fetch = 0;

	    thread_info[i].last_fetch = 0;
	    thread_info[i].blocked = false;
	    thread_info[i].active = false;
	    thread_info[i].fetch_counter = 0;
	    thread_info[i].commit_counter = 0;
	    thread_info[i].base_commit_count = 0;
	    thread_info[i].fetch_average = 0;
	    thread_info[i].current_icount = 0;
	    thread_info[i].cum_icount = 0;

            thread_info[i].recovery_event_pending = false;
            thread_info[i].recovery_spec_level = 0;
	}
	else {
	    // only need to initialize these for the non-initial case
	    active_list[i] = -1;
	    inactive_list[i] = -1;
	}
    }

    //
    //  This section of code manipulates the fetch-list such that active
    //  threads are at the front of the list, inactive threads are at the
    //  bottom of the list.  If the fetch-policy is RR, then the active
    //  threads are also sorted by thread priority
    //
    if (!initial) {

	int active_index = 0;
	int inactive_index = 0;
	int templist_index = 0;

	//
	//  Place each thread into either the active or inactive list
	//
	for (int thread = 0; thread < SMT_MAX_THREADS; thread++) {
	    if (thread_info[thread].active)
		active_list[active_index++] = thread;
	    else
		inactive_list[inactive_index++] = thread;
	}

	/*
	 *  We make a copy of the current fetch_list in temp_list
	 *  => The orderering from the fetch_list MUST be maintianed!!!
	 */

	/*  itterate through the fetch list  */
	for (int j = 0; j < SMT_MAX_THREADS; j++) {
	    int thread = fetch_list[j].thread_number;

	    /*  look for this thread number in the active list  */
	    for (int i = 0; i < active_index; i++) {

		/*  If it's in the active list,
		 *  Put it into the temporary list so we can sort it
		 */
		if (active_list[i] == thread) {
		    temp_list[templist_index] = fetch_list[j];

		    /*  Must make sure that we have the latest
			priority information  */
		    temp_list[templist_index].priority =
			thread_info[thread].priority;

		    templist_index++;
		}
	    }
	}

	/*  RR policy requires these threads to be sorted into
	 * descending priority order */
	if (fetch_policy == RR)
	    qsort(temp_list,templist_index,
		  sizeof(ThreadListElement),rr_compare);

	/* put the inactive entries into the temp list */
	for (int i = 0; i < inactive_index; i++)
	    for (int j = 0; j < SMT_MAX_THREADS; j++)
		if (fetch_list[j].thread_number == inactive_list[i])
		    temp_list[templist_index++] = fetch_list[j];

	/* copy back to the real list */
	for (int i = 0; i < SMT_MAX_THREADS; i++)
	    fetch_list[i] = temp_list[i];
    }
}


void
FullCPU::change_thread_state(int thread_number, int activate, int priority)
{
    // override specified priority if prioritization disabled via option
    if (!fetch_priority_enable)
	priority = 100;

    thread_info[thread_number].priority = priority;

    /*  look for thread in the fetch list  */
    int ptr = -1;
    for (int i = 0; i < number_of_threads; i++)
	if (fetch_list[i].thread_number == thread_number)
	    ptr = i;

    /*  ptr >= 0 if we found it  */
    if (ptr >= 0) {
	thread_info[thread_number].active = activate;
	thread_info[thread_number].priority = priority;
	fetch_list[ptr].priority = priority;

	if (fetch_policy == RR)
	    fetch_list[ptr].sort_key = priority;
    } else
	panic("fetch list is screwed up!");

    /*  make sure that the fetch list is kosher  */
    initialize_fetch_list(false);
}

/*  Remove all instructions from a specific thread from the fetch queue  */
void
FullCPU::fetch_squash(int thread_number)
{
    icache_output_buffer[thread_number]->squash(thread_number);

    // free ifq slots reserved for icache_output_buffer insts
    if (mt_frontend)
        ifq[thread_number].squash();
    else
        ifq[0].squash(thread_number);

    icacheInterface->squash(thread_number);
}


/* initialize the instruction fetch pipeline stage */
void
FullCPU::fetch_init()
{
    icache_block_size = icacheInterface->getBlockSize();
    insts_per_block = icache_block_size / TheISA::fetchInstSize();

    for (int i = 0; i < number_of_threads; i++) {

        /* allocate the IFETCH -> DISPATCH instruction queue */
        if (!mt_frontend) {
            if (i == 0)
                ifq[0].init(this, ifq_size, number_of_threads);
        }
        else
            ifq[i].init(this, ifq_size, i);

	icache_output_buffer[i] = new IcacheOutputBuffer;
        icache_output_buffer[i]->init(fetch_width, i);

	fetch_stall[i] = 0;
	fetch_fault_count[i] = 0;
    }
}


void
FullCPU::clear_fetch_stall(Tick when, int thread_number, int stall_type)
{
    if (fetch_stall[thread_number] == stall_type) {
	fetch_stall[thread_number] = 0;
    }
}



// ===========================================================================

//
//  Events used here
//

void
ClearFetchStallEvent::process()
{
    cpu->clear_fetch_stall(curTick, thread_number, stall_type);
    delete this;
}


const char *
ClearFetchStallEvent::description()
{
    return "clear fetch stall";
}


void
FetchCompleteEvent::process()
{
	IcacheOutputBuffer *bufferp;
	IcacheOutputBufferEntry *entryp;
	int index = first_index;

	bufferp = cpu->icache_output_buffer[thread_number];

	DPRINTF(Fetch, "Processing FetchCompleteEvent for CPU %d, thread %d\n", cpu->CPUParamsCpuID, thread_number);

	// checkmshrp->targets[target_idx]. to see if first instruction is
	// still valid
	entryp = &(bufferp->insts[first_index]);
	if (!entryp->inst || entryp->inst->fetch_seq != first_seq_num) {
		// squashed! no point in continuing, as any squash will
		// eliminate the whole group of instrs
		DPRINTF(Fetch, "Eliminating fetch group due to squash for CPU %d, thread %d (seq num %d, first seq num %d)\n",
				cpu->CPUParamsCpuID, thread_number, entryp->inst->fetch_seq, first_seq_num);
		goto done;
	}

	// are the instrs we fetched at the head of the buffer?  if so,
	// we'll want to copy them to the ifq as we go.  if we allow
	// multiple fetches on the same thread and they complete out of
	// order, this may not be the case.
	if (first_index == bufferp->head) {
		for (int i = 0; i < num_insts; ++i) {
			entryp = &bufferp->insts[index];
			if (cpu->mt_frontend) {
				cpu->ifq[entryp->inst->thread_number].append(entryp->inst);
			} else {
				cpu->ifq[0].append(entryp->inst);
			}
			entryp->inst = NULL;
			index = bufferp->incr(index);
		}

		// now copy any succeeding ready instrs (that were the result
		// of an out-of-order fetch completion) to the ifq
		while (entryp = &bufferp->insts[index], entryp->ready) {
			if (cpu->mt_frontend) {
				cpu->ifq[entryp->inst->thread_number].append(entryp->inst);
			} else {
				cpu->ifq[0].append(entryp->inst);
			}
			entryp->ready = false;
			entryp->inst = NULL;
			++num_insts;
			index = bufferp->incr(index);
		}

		// update head & num_insts to reflect instrs copied to ifq
		bufferp->head = index;
		bufferp->num_insts -= num_insts;
		assert(bufferp->num_insts >= 0);
	} else {
		// just mak instrs as ready until preceding instrs complete fetch
		for (int i = 0; i < num_insts; ++i) {
			entryp = &(bufferp->insts[index]);
			entryp->ready = true;
			index = bufferp->incr(index);
		}
	}

	done:
	delete this;
}


const char *
FetchCompleteEvent::description()
{
    return "fetch complete";
}


// ===========================================================================


/**
 * Fetch one instruction from functional memory and execute it
 * functionally.
 *
 * @param thread_number Thread ID to fetch from.
 * @return A pair consisting of a pointer to a newly
 * allocated DynInst record and a fault code.  The DynInst pointer may
 * be NULL if the fetch was unsuccessful.  The fault code may reflect a
 * fault caused by the fetch itself (e.g., ITLB miss trap), or on the
 * functional execution.  The fault code will be No_Fault if no
 * fault was encountered.
 */
pair<DynInst *, Fault>
FullCPU::fetchOneInst(int thread_number)
{
    SpecExecContext *xc = thread[thread_number];
    Addr pc = xc->regs.pc;

    // fetch instruction from *functional* memory into IR so we can
    // look at it
    MachInst IR;	// instruction register

#if FULL_SYSTEM
#define IFETCH_FLAGS(pc)	((pc) & 1) ? PHYSICAL : 0
#else
#define IFETCH_FLAGS(pc)	0
#endif

    MemReqPtr req = new MemReq(pc & ~(Addr)3, xc, sizeof(MachInst),
			       IFETCH_FLAGS(pc));
    req->flags |= INST_READ;
    req->pc = pc;

    /**
     * @todo
     * Need to set asid correctly once we put in a unified memory system.
     */
    // Need to make sure asid is 0 so functional memory works correctly
    req->asid = 0;

    Fault fetch_fault = xc->translateInstReq(req);

#if FULL_SYSTEM
    if (xc->spec_mode &&
	(fetch_fault != No_Fault || (req->flags & UNCACHEABLE) ||
	 xc->memctrl->badaddr(req->paddr))) {
	// Bad things can happen on misspeculated accesses to bad
	// or uncached addresses...  stop now before it's too late
	return make_pair((DynInst *)NULL, No_Fault);
    }
#endif

    if (fetch_fault == No_Fault) {
	// translation succeeded... attempt access
	fetch_fault = xc->mem->read(req, IR);
    }

    if (fetch_fault != No_Fault) {
#if FULL_SYSTEM
	assert(!xc->spec_mode);
	xc->ev5_trap(fetch_fault);
	// couldn't fetch real instruction, so replace it with a noop
	IR = TheISA::NoopMachInst;
#else
	fatal("Bad translation on instruction fetch, vaddr = 0x%x",
	      req->vaddr);
#endif
    }

    StaticInstPtr<TheISA> si(IR);

    // If SWP_SQUASH is set, filter out SW prefetch instructions right
    // away; we don't want them to even count against our fetch
    // bandwidth limit.
    if (softwarePrefetchPolicy == SWP_SQUASH && si->isDataPrefetch()) {

	xc->regs.pc += sizeof(MachInst);

	// need this for EIO syscall checking
	if (!xc->spec_mode)
	    xc->func_exe_inst++;

	exe_swp[thread_number]++;

	// next instruction instead by calling fetchOneInst()
	// again recursively
	return fetchOneInst(thread_number);
    }

    //
    // Allocate a DynInst object and populate it
    //
    DynInst *inst = new DynInst(si);

    inst->fetch_seq = next_fetch_seq++;

    inst->PC = pc;

    inst->thread_number = thread_number;
    inst->asid = thread[thread_number]->getDataAsid();
    inst->cpu = this;

    inst->xc = xc;
    inst->fault = fetch_fault;
    inst->spec_mode = xc->spec_mode;
    inst->trace_data = Trace::getInstRecord(curTick, xc, this,
					    inst->staticInst,
					    xc->regs.pc, thread_number);

    inst->correctPathSeq = (xc->spec_mode == 0) ? correctPathSeq[thread_number]++ : 0;

    // Predict next PC to fetch.
    // default guess: sequential
    inst->Pred_PC = pc + sizeof(MachInst);

    if (inst->isControl()) {
	// it's a control-flow instruction: check with predictor

	// FIXME: This should be a compressed fetch_address
	BranchPred::LookupResult result;
	if (branch_pred) {
	  result =
	    branch_pred->lookup(thread_number, pc, inst->staticInst,
				&inst->Pred_PC, &inst->dir_update);

	  // The old branch predictor interface used a predicted PC of 0
	  // to indicate Predict_Not_Taken.  In some rare cases (e.g.,
	  // underflow of an uninitialized return address stack), the
	  // predictor will predict taken but provided a predicted
	  // target of 0.  With the old interface, this was
	  // misinterpreted as a not-taken prediction.  Since the new
	  // predictor interface actually distinguishes these cases, it
	  // gets slightly different misspeculation behavior, leading to
	  // slightly different results.  For exact emulation of the old
	  // behavior, uncomment the code below.
	  //
	  // if (inst->Pred_PC == 0) {
	  //     inst->Pred_PC = pc + sizeof(MachInst);
	  //     result = BranchPred::Predict_Not_Taken;
	  // }

	  inst->btb_missed = (result == BranchPred::Predict_Taken_No_Target);
	} else {
	  // Do perfect branch prediction
	  // We can't set this yet.
	  //inst->Pred_PC = inst->Next_PC;
	  inst->btb_missed = false;
	}
    }

    /********************************************************/
    /*                                                      */
    /*   Fill in the fetch queue entry                      */
    /*                                                      */
    /********************************************************/

    IcacheOutputBufferEntry *buf_entry =
	icache_output_buffer[thread_number]->new_tail();
    buf_entry->inst = inst;
    buf_entry->ready = false;

    if (mt_frontend) {
	ifq[thread_number].reserve(thread_number);
	assert(ifq[thread_number].num_total() <= ifq_size);
    } else {
	ifq[0].reserve(thread_number);
	assert(ifq[0].num_total() <= ifq_size);
    }

    /**********************************************************/
    /*                                                        */
    /*   Execute this instruction.			      */
    /*                                                        */
    /**********************************************************/
    if (inst->fault == No_Fault)
	inst->fault = execute_instruction(inst, thread_number);

    if (inst->fault != No_Fault)
	return make_pair(inst, inst->fault);

    if (!branch_pred) {
      // Do perfect branch prediction
      inst->Pred_PC = inst->Next_PC;
    }

    //
    // Update execution context's PC to point to next instruction to
    // fetch (based on predicted next PC).
    //

    //
    //  Pipe-tracing...
    //
    //  This doesn't quite work the way we might want it:
    //    - The fourth parameter (latency) is supposed to indicate the
    //      cache miss delay...
    //    - The fifth parameter (longest latency event) is designed
    //      to indicate which of several events took the longest...
    //      since our model doesn't generate events, this
    //      isn't used here either.
    //
    if (ptrace) {
	ptrace->newInst(inst);
	ptrace->moveInst(inst, PipeTrace::Fetch, 0, 0, 0);
    }

    /*
     *   Handle the transition "into" mis-speculative mode
     *   (note that we may _already_ be misspeculating)
     *
     *   There are two "forms" of misspeculation that we have to
     *   deal with:
     *     (1) Mispredicting the direction of a branch
     *     (2) Mispredicting the
     */


    if (inst->Pred_PC != inst->Next_PC) {

	// Conflicting maps are also recover insts, but why?  Perhaps
	// because all of the aliased registers are updated?  But
	// that shouldn't matter for a renaming machine with the LRT.
	// Ah, except the LRT won't be updated until the map executes,
	// which is too late.
	inst->recover_inst = true;
	xc->spec_mode++;
    }

    return make_pair(inst, No_Fault);
}


/**
 * Fetch several instructions (up to a full cache line) from
 * functional memory and execute them functionally (by calling
 * fetchOneInst()).  Fetching will cease if the end of the current
 * cache line is reached, a predicted-taken branch is encountered, a
 * fault occurs, the fetch queue fills up, or the CPU parameter limits
 * on instructions or branches per cycle are reached.  In the first
 * two cases, fetching can continue for this thread in this cycle in
 * another block; in the remaining cases, fetching for this thread
 * must be terminated for this cycle.
 *
 * @param thread_number Thread ID to fetch from.
 * @param max_to_fetch Maximum number of instructions to fetch.
 * @param branch_cnt Reference to number of branches fetched this cycle
 * for the current thread.  This value will be updated if any branches
 * are fetched.
 * @return A pair combining an integer indicating
 * the number of instructions fetched and a boolean indicating whether
 * fetching on the current thread should continue.
 */
pair<int, bool>
FullCPU::fetchOneLine(int thread_number, int max_to_fetch, int &branch_cnt,
		      bool entering_interrupt)
{
    SpecExecContext *xc = thread[thread_number];
    int num_fetched = 0;

    // remember start address of current line, so we can tell if we
    // cross to the next one
    Addr blockBaseAddr = icacheBlockAlignPC(xc->regs.pc);

    do {
#if FULL_SYSTEM
	// do PC-based annotations for the *next* PC here, now that
	// we've update the PC.  This lets us magically transition to
	// a totally different instruction with zero overhead (e.g.,
	// if the annotation modifies pc).

	if (!xc->spec_mode) {
	    Addr oldpc;
	    do {
		oldpc = xc->regs.pc;
		system->pcEventQueue.service(xc);
	    } while (oldpc != xc->regs.pc);
	}
#endif

	pair<DynInst *, Fault> r = fetchOneInst(thread_number);
	DynInst *inst = r.first;
	Fault fault = r.second;

	if (inst != NULL)
	    num_fetched++;

	// inst == NULL signals failure to fetch for some reason (like
	// refusal to fetch a speculative uncached instruction)
	if (fault != No_Fault || inst == NULL) {
	    if (fault != No_Fault) {
		fetch_fault_count[thread_number]++;
	    }
	    return make_pair(num_fetched, false);
	}

	xc->regs.pc = inst->Pred_PC;

	// if we're entering the asynchronous interrupt handler, mark
	// the first instruction as "serializing" to flush the ROB
	// before dispatching it.  Otherwise we're likely to
	// underestimate the overhead of entering the handler.
	if (entering_interrupt) {
	    inst->serializing_inst = true;
	    entering_interrupt = false;	// just flag first one
	}

	/*
	 *  Now, figure out if we need to stop fetching...
	 */

	// did we exceed the per-cycle instruction limit?
	if (num_fetched >= max_to_fetch)
	    return make_pair(num_fetched, false);

	// is the fetch queue full?
	if ((mt_frontend && ifq[thread_number].num_total() == ifq_size) ||
	    (!mt_frontend && ifq[0].num_total() == ifq_size)) {
	    floss_state.fetch_end_cause[thread_number] = FLOSS_FETCH_QFULL;
	    return make_pair(num_fetched, false);
	}

	if (inst->isControl()) {
	    branch_cnt++;
	    fetched_branch[thread_number]++;

	    /*  if we've exceeded our branch count, then we're  */
	    /*  done...                                         */
	    if (branch_cnt >= fetch_branches) {
		floss_state.fetch_end_cause[thread_number] = FLOSS_FETCH_BRANCH_LIMIT;
		return make_pair(num_fetched, false);
	    } else if (inst->Pred_PC != inst->PC + sizeof(MachInst)) {
		/*  otherwise...                                      */
		/*  if this is a predicted-taken branch, discontinue  */
		/*  getting instructions from this block, move on to  */
		/*  the next one.                                     */
		return make_pair(num_fetched, true);
	    }
	}
	// did we fall through to the next cache line?
    } while (icacheBlockAlignPC(xc->regs.pc) == blockBaseAddr);

    return make_pair(num_fetched, true);
}



// For debugging purposes
static Addr uncompressedBlockAddress = 0;

/**
 * Do fetch for one thread.
 *
 * @param thread_number Thread ID to fetch from.
 * @param max_to_fetch Maximum number of instructions to fetch.
 * @return Number of instructions fetched.
 */
int
FullCPU::fetchOneThread(int thread_number, int max_to_fetch)
{
    SpecExecContext *xc = thread[thread_number];
    int fetched_this_thread = 0;
    int branch_cnt = 0;

    // Track fetched blocks so we don't fetch the same one twice in
    // the same cycle.
    // (This is relatively expensive... we should find a way to do
    // without it -- Steve)
    std::set<Addr> fetchedAddresses;

#if FULL_SYSTEM
    bool entering_interrupt = false;

    // Check for interrupts here.  We may want to do this sooner in
    // SMT full system (up in fetch(), before we do the thread
    // selection), but for a single-threaded processor it should be OK
    // here.
    if (!xc->spec_mode && checkInterrupts && check_interrupts() &&
	!xc->inPalMode()) {
	int ipl = 0;
	int summary = 0;
	checkInterrupts = false;
	IntReg *ipr = xc->regs.ipr;

	if (xc->regs.ipr[AlphaISA::IPR_SIRR]) {
	    for (int i = AlphaISA::INTLEVEL_SOFTWARE_MIN;
		 i < AlphaISA::INTLEVEL_SOFTWARE_MAX; i++) {
		if (ipr[AlphaISA::IPR_SIRR] & (ULL(1) << i)) {
		    // See table 4-19 of 21164 hardware reference
		    ipl = (i - AlphaISA::INTLEVEL_SOFTWARE_MIN) + 1;
		    summary |= (ULL(1) << i);
		}
	    }
	}

	uint64_t interrupts = xc->cpu->intr_status();
	for (int i = AlphaISA::INTLEVEL_EXTERNAL_MIN;
	    i < AlphaISA::INTLEVEL_EXTERNAL_MAX; i++) {
	    if (interrupts & (ULL(1) << i)) {
		// See table 4-19 of 21164 hardware reference
		ipl = i;
		summary |= (ULL(1) << i);
	    }
	}

	if (ipr[AlphaISA::IPR_ASTRR])
	    panic("asynchronous traps not implemented\n");

	if (ipl && ipl > xc->regs.ipr[AlphaISA::IPR_IPLR]) {
	    ipr[AlphaISA::IPR_ISR] = summary;
	    ipr[AlphaISA::IPR_INTID] = ipl;
	    xc->ev5_trap(Interrupt_Fault);
	    entering_interrupt = true;

	    DPRINTF(Flow, "Interrupt! IPLR=%d ipl=%d summary=%x\n",
		    ipr[AlphaISA::IPR_IPLR], ipl, summary);
	}
    }
#else
    const bool entering_interrupt = false;
#endif

    // Fetch up to the maximum number of lines per cycle allowed
    for (int fetchedLines = 0; fetchedLines < lines_to_fetch; ++fetchedLines) {

	/* is this a bogus text address? (can happen on mis-spec path) */
	if (!xc->validInstAddr(xc->regs.pc)) {
	    floss_state.fetch_end_cause[thread_number] = FLOSS_FETCH_INVALID_PC;
	    break;
	}

	// remember index & seq. number of first inst in this line for
	// cache fetch later
	int first_inst_index = icache_output_buffer[thread_number]->tail;
	InstSeqNum first_inst_seq_num = next_fetch_seq;

	uncompressedBlockAddress = xc->regs.pc;

	/*  Mask lower bits to get block starting address         */
	Addr blockAddress = icacheBlockAlignPC(xc->regs.pc);

#if FULL_SYSTEM
	bool pal_pc = xc->inPalMode();
#endif
	pair<int, bool> r = fetchOneLine(thread_number,
					 max_to_fetch - fetched_this_thread,
					 branch_cnt,
					 entering_interrupt);
	int fetched_this_line = r.first;
	bool keep_fetching = r.second;

	fetched_this_thread += fetched_this_line;

	/*
	 *  Fetch the entire cache block containing the instruction
	 *  at "start_address"
	 */
	if (fetched_this_line > 0
	    && (fetchedAddresses.find(blockAddress) ==
		fetchedAddresses.end())) {
	    MemAccessResult mem_access_result;

	    assert(!icacheInterface->isBlocked());
	    MemReqPtr req = new MemReq(blockAddress, xc,
				     icache_block_size);
	    req->flags |= INST_READ;
	    req->cmd = Read;
	    req->asid = thread[thread_number]->getInstAsid();
	    req->thread_num = thread_number;
	    req->time = curTick;
	    req->data = new uint8_t[req->size];
	    req->xc = xc;
	    req->pc = xc->regs.pc;

	    Event *ev = new FetchCompleteEvent(this,
					       thread_number,
					       first_inst_index,
					       fetched_this_line,
					       first_inst_seq_num,
					       req);

	    req->completionEvent = ev;
            req->expectCompletionEvent = true;

#if FULL_SYSTEM
	    // ugly hack!
	    if (pal_pc)
		req->paddr = req->vaddr;
	    else
		req->paddr = vtophys(xc, blockAddress);

	    req->paddr &= EV5::PAddrImplMask;
#else
	    Fault fetch_fault = xc->translateInstReq(req);

	    if (fetch_fault != No_Fault)
		fatal("Bad translation on instruction fetch, vaddr = 0x%x",
		      req->vaddr);
#endif

	    mem_access_result = icacheInterface->access(req);

	    if (mem_access_result != MA_HIT) {

		/* if we missed in the I-cache, stop fetching after this
		   block.   */
		floss_state.fetch_end_cause[thread_number] = FLOSS_FETCH_IMISS;
		floss_state.fetch_mem_result[thread_number] =
		    mem_access_result;
		break;
	    }
	}

	if (!keep_fetching)
	    break;

	/*
	 * fetch_branches == 0, fetch one cache line per thread
	 */
	if (fetch_branches == 0) {
	    floss_state.fetch_end_cause[thread_number] = FLOSS_FETCH_BRANCH_LIMIT;
	    break;
	}
    }


    if (fetched_this_thread) {
	thread_info[thread_number].last_fetch = curTick;
    }

    /*
     *  STATISTICS  (per-thread)
     */
    fetch_nisn_dist_[thread_number].sample(fetched_this_thread);
    fetched_inst[thread_number] += fetched_this_thread;

    thread_info[thread_number].fetch_counter += fetched_this_thread;

    return fetched_this_thread;
}




/*****************************************************************************/
/* fetch up as many instruction as one branch prediction and one cache line  */
/* acess will support without overflowing the IFETCH -> DISPATCH QUEUE       */
/*                                                                           */
/*  This function calls choose_next_thread() to determine which thread will  */
/*  fetch next.                                                              */
/*       => choose_next_thread() calls the individual policy routines        */
/*          based on the setting of "fetch_policy"                           */
/*                                                                           */
/*****************************************************************************/

void
FullCPU::fetch()
{
	int fetched_this_cycle = 0;
	int fetched_this_thread;
	int ports_used = 0;

	int thread_fetched[number_of_threads];

	/*
	 *  Reset the number of instrs fetched for each thread
	 */
	icache_ports_used_last_fetch = 0;
	for (int i = 0; i < number_of_threads; i++) {
		thread_fetched[i] = 0;

#if 0
		if (curTick > 10000 && thread_info[i].last_fetch < curTick - 2000) {
			stringstream s;
			s << "Thread " << i << " hasn't fetched since cycle " <<
					thread_info[i].last_fetch << ends;
			exitNow(s.str(), 1);
		}
#endif
	}


	/* always update icounts... we use them for bias adjustment even
	 * if we don't need them for scheduling this cycle */

	update_icounts();

	bool blockedDueToCache = false;

	/*
	 * For each thread, set/clear the thread_info[].blocked flag.
	 * If set, also set floss_state.fetch_end_cause[] to indicate why.
	 */
	for (int thread_number = 0; thread_number < number_of_threads;
			thread_number++) {

		ExecContext *xc = thread[thread_number];

		/* assume the worst until proven otherwise */
		thread_info[thread_number].blocked = true;

		/* Unless we fetch a full fetch_width of instructions, this
		 * should get set to indicate why we didn't */
		floss_state.fetch_end_cause[thread_number] = FLOSS_FETCH_NONE;

		//
		//  Now: check all the reasons we could be blocked... if none of
		//       them are true, then mark as not blocked
		//
		//
		if (!thread_info[thread_number].active)
			continue;

		if (xc->status() != ExecContext::Active) {
#if FULL_SYSTEM
			if (xc->status() == ExecContext::Suspended && check_interrupts()) {
				xc->activate();
			} else
#endif // FULL_SYSTEM
			{
				continue;
				DPRINTF(Fetch, "Skipping thread %d since it is not active\n", thread_number);
			}
		}

		//
		//  The case where the IFQ is full, but all slots are reserved
		//  (ie. no real instructions present) indicates a cache miss.
		//  This will be detected and handled later.
		//
		int flag = 0;
		if (mt_frontend) {
			FetchQueue &q = ifq[thread_number];
			if (q.num_total() == q.size && q.num_reserved < q.num_total()) {
				floss_state.fetch_end_cause[thread_number] = FLOSS_FETCH_QFULL;
				flag = 1;
			}
		} else {
			//
			//  For the non-MT case...
			//
			FetchQueue &q = ifq[0];
			if (q.num_total() == ifq_size && q.num_reserved < q.num_total()) {

				floss_state.fetch_end_cause[thread_number] = FLOSS_FETCH_QFULL;

				if (thread_number == 0)
					flag = 1;   // First time through, we collect stats...
				else
					continue;   // After that, we just keep going...
			}
		}

		if (flag) {
			//
			//  We can't fetch for this thread...
			//
			for (int i = 0; i < number_of_threads; ++i) {
				unsigned c = IQNumInstructions(i);
				qfull_iq_occupancy[i] += c;
				qfull_rob_occupancy[i] += ROB.num_thread(i);

				qfull_iq_occ_dist_[i].sample(c);
				qfull_rob_occ_dist_[i].sample(ROB.num_thread(i));
			}
			DPRINTF(Fetch, "Skipping thread %d because the fetch queue is full\n", thread_number);
			continue;
		}

		if (fetch_stall[thread_number] != 0) {
			/* fetch loss cause for this thread is fid_cause value */
			floss_state.fetch_end_cause[thread_number] =
					fid_cause[thread_number];
			DPRINTF(Fetch, "Skipping thread %d because of a FID cause\n", thread_number);
			continue;
		}

		if (fetch_fault_count[thread_number] != 0) {
			// pending faults...
			floss_state.fetch_end_cause[thread_number] =
					FLOSS_FETCH_FAULT_FLUSH;
			DPRINTF(Fetch, "Skipping thread %d because of pending faults\n", thread_number);
			continue;
		}

		/* if icache_output_buffer is still full (due to icache miss,
           or multi-cycle hit) then stall */
		if (icache_output_buffer[thread_number]->free_slots() < fetch_width) {
			floss_state.fetch_end_cause[thread_number] = FLOSS_FETCH_IMISS;
			floss_state.fetch_mem_result[thread_number] = MA_CACHE_MISS;
			blockedDueToCache = true;
			DPRINTF(Fetch, "Skipping thread %d because of instruction cache being blocked\n", thread_number);
			continue;
		}

		thread_info[thread_number].blocked = false;
		DPRINTF(Fetch, "Thread %d is not blocked\n", thread_number);
	}


	/*
	 *  We need to block threads that have been assigned zero priority
	 *  Check for all blocked while we're at it...
	 */
	bool all_threads_blocked = true;
	for (int i = 0; i < number_of_threads; i++) {
		if (thread_info[i].priority == 0)
			thread_info[i].blocked = true;

		if (!thread_info[i].blocked)
			all_threads_blocked = false;
	}

	if (all_threads_blocked) {
		flossRecord(&floss_state, thread_fetched);
		fetch_idle_cycles++;
		if(blockedDueToCache) fetch_idle_cycles_cache_miss++;
		//	check_counters();
		return;
	}

	/*  Add our static biases into the current icounts                     */
	/*  ==> these will be removed after the choose_next_thread() function  */
	for (int i = 0; i < number_of_threads; i++)
		thread_info[i].current_icount += static_icount_bias[i];

	/*
	 *  This function takes the contents of thread_info[] into account
	 *  and may change fetch_list[].blocked
	 */
	choose_next_thread(fetch_list);

	/*  Remove our static biases from the current icounts  */
	for (int i = 0; i < number_of_threads; i++)
		thread_info[i].current_icount -= static_icount_bias[i];

	//
	//  Assert blocked flag for threads with active ROB or IQ caps
	//
	for (int i = 0; i < number_of_threads; i++) {
		int thread_number = fetch_list[i].thread_number;

		/*  Handle IQ and ROB caps  */
		if (iq_cap_active[thread_number] || rob_cap_active[thread_number])
			fetch_list[i].blocked = true;
	}

	/*
	 *  Are all threads blocked?
	 *  => Need to check again, because the fetch policy may block a thread
	 *
	 *  scan by fetch_list[] index to find threads not blocked by cache miss
	 *  or by fetch policy
	 */
	all_threads_blocked = true;

	for (int i = 0; i < number_of_threads; i++) {
		int thread_number = fetch_list[i].thread_number;

		if (fetch_list[i].blocked)
			floss_state.fetch_end_cause[thread_number] = FLOSS_FETCH_POLICY;
		else if (!thread_info[thread_number].blocked)
			all_threads_blocked = false;
	}


	/* if all threads are in icache misses, we're done */
	if (all_threads_blocked) {
		flossRecord(&floss_state, thread_fetched);
		fetch_idle_cycles++;
		//	check_counters();
		DPRINTF(Fetch, "Returning, all threads are blocked on a cache miss\n");
		return;
	}

	/*
	 * If the icache is totally blocked, we won't be able to fetch for
	 * any thread, even if some are ready.  This should be very
	 * unlikely now that we squash outstanding I-cache requests on a
	 * misspeculation, as long as there are as many MSHRs as threads
	 * (and we're not doing i-cache prefetching).
	 *
	 * If this does occur, we'd like to spread the blame among all
	 * threads that currently have outstanding icache misses.  Howver,
	 * this doesn't fit into our floss_reasons interface, so for now
	 * we'll just consider it an idel fetch cycle.
	 */
	if (icacheInterface->isBlocked()) {
		flossRecord(&floss_state, thread_fetched);
		fetch_idle_cycles++;
		fetch_idle_icache_blocked_cycles++;
		//	check_counters();
		DPRINTF(Fetch, "Returning, all threads are blocked because the instruction cache is blocked\n");
		return;
	}


	/*====================================================================*/

	/*  Fetch from _some_ thread as long as:
	 * (1) We have fetch bandwidth left
	 * (2) We haven't used all the icache ports
	 * (3) We haven't tried all the threads
	 */

	for (int list_index = 0; list_index < number_of_threads; ++list_index) {

		if (fetched_this_cycle >= fetch_width
				|| ports_used >= num_icache_ports
				|| icacheInterface->isBlocked()) {

			//  We didn't look at all the threads...
			for (int j = 0; j < number_of_threads; ++j) {
				if (floss_state.fetch_end_cause[j] == FLOSS_FETCH_NONE) {
					// Call the cause BANDWIDTH
					// (actual cause is either _real_ BW or cache ports)
					floss_state.fetch_end_cause[j] = FLOSS_FETCH_BANDWIDTH;
				}
			}

			break;
		}

		int thread_number = fetch_list[list_index].thread_number;

		if (thread_info[thread_number].blocked
				|| fetch_list[list_index].blocked) {
			continue;
		}

		fetch_decisions++;

		fetch_chances[thread_number]++;

		ports_used++;

		/*  Indicate that we've tried to fetch from a thread  */
		fetch_choice_dist[list_index]++;

		fetched_this_thread =
				fetchOneThread(thread_number,
						fetch_width - fetched_this_cycle);

		if (fetched_this_thread > 0)
			++icache_ports_used_last_fetch;

		fetched_this_cycle += fetched_this_thread;
		DPRINTF(Fetch, "Fetched %d instructions for thread %d\n", fetched_this_thread, list_index);

		//	thread_fetched[thread_number] = fetched_this_thread;
		thread_fetched[0] += fetched_this_thread;
	}				/*  fetch for 'n' threads...  */

	fetch_nisn_dist.sample(fetched_this_cycle);

	flossRecord(&floss_state, thread_fetched);

	/*  RR Policy:
	 *     cycle around the list by the number of "extra" threads
	 *     checked this cycle (we cycle for one thread automagically)
	 */
	if (fetch_policy == RR)
		for (int i = 1; i < ports_used; i++)
			choose_next_thread(fetch_list);
}




/*=====================================================================*/


void
FullCPU::choose_next_thread(ThreadListElement *thread_list)
{
    switch (fetch_policy) {
    case RR:
	round_robin_policy(thread_list);
	break;
    case IC:
	icount_policy(thread_list);
	break;
    default:
	panic("Illegal fetch policy requested");
	break;
    }
}


/*=====================================================================*/

void
FullCPU::round_robin_policy(ThreadListElement *thread_list)
{
    int first, last;
    ThreadListElement temp;


    /*
     *  Check for valid thread_list[]
     *  (can become invalid when a thread finishes before the end of run)
     */
    for (int i = 0; i < number_of_threads; i++) {
	/*
	 * look for inactive threads at the top of the list
	 */
	if (!thread_info[thread_list[i].thread_number].active) {
	    /*
	     *  We have to re-build the list
	     */
	    initialize_fetch_list(false);
	    break;
	}

	/*  Make sure that we don't block any threads in here...  */
	/*  if we need to initialize the fetch list, this will be */
	/*  done there...                                         */
	thread_list[i].blocked = false;
    }

    /*  At this point, the threads in the list are grouped by priority    */
    /*  from highest to lowest. Now we rotate within each priority group  */
    first = 0;
    last = 0;
    while (first < number_of_threads - 1) {

	/*  look for the last member of this priority group  */
	while ((thread_list[first].priority == thread_list[last].priority)
	       && (last < number_of_threads)) {
	    last++;
	}
	last--;

	if (first != last) {
	    /*  rotate this group  */
	    temp = thread_list[first];
	    for (int i = first; i < last; i++)
		thread_list[i] = thread_list[i + 1];

	    thread_list[last] = temp;
	}
	/*  point to the next group  */
	first = last + 1;
	last = first;
    }
}


void
FullCPU::update_icounts()
{
    /*  initialize instruction counters  */
    if (mt_frontend)
        for (int i = 0; i < number_of_threads; i++)
            thread_info[i].current_icount = ifq[i].num_valid +
		decodeQueue->count(i) + IQNumInstructions(i);
    else
        for (int i = 0; i < number_of_threads; i++)
            thread_info[i].current_icount = ifq[0].num_valid_thread[i] +
                decodeQueue->count(i) + IQNumInstructions(i);

    /*  update cumulative instruction counters  */
    for (int i = 0; i < number_of_threads; i++)
	thread_info[i].cum_icount += thread_info[i].current_icount;
}


void
FullCPU::fetch_dump()
{
    for (int i = 0; i < number_of_threads; i++)
        icache_output_buffer[i]->dump();

    cout << "=======================================================\n";
    cout << "Fetch Stage State:\n";
    for (int i = 0; i < number_of_threads; i++) {
	cprintf(" (Thread %d)  PC: %#08x\n", i, thread[i]->regs.pc);
	if (thread[i]->spec_mode)
	    cout << "   {Mis-speculating}\n";
    }

    if (mt_frontend) {
        for (int i = 0; i < number_of_threads; i++) {
	    stringstream stream;
            ccprintf(stream, " (thread %d)", i);
            ifq[i].dump(stream.str());
        }
    } else
        ifq[0].dump("");
}


void
FullCPU::icount_policy(ThreadListElement *thread_list)
{
    /*  put instruction counts into thread list  */
    for (int i = 0; i < number_of_threads; i++) {
	thread_list[i].thread_number = i;
	thread_list[i].blocked = false;
	thread_list[i].priority = thread_info[i].priority;
	thread_list[i].sort_key = thread_info[i].current_icount +
	    10000 * !thread_info[i].active + 10000 * thread_info[i].blocked;
    }

#ifdef DEBUG_ICOUNT
    cerr << "-----------------------------------------------\n";
    for (int i = 0; i < number_of_threads; i++)
	ccprintf(cerr, "%d ", thread_list[i].sort_key);
    cerr << "\n";
#endif

    /*  sort the list of threads in descending order... */
    /*  only the first 'n' threads are actually sorted  */
    qsort(thread_list, number_of_threads,
	  sizeof(ThreadListElement), icount_compare);

#ifdef DEBUG_ICOUNT
    for (int i = 0; i < number_of_threads; i++)
	ccprintf(cerr, "%d ", thread_list[i].sort_key);
    cerr << "\n";
#endif

}

/*======================================================================*/


/*  sort by descending priority field  */
static int
rr_compare(const void *first, const void *second)
{

    /*   FIRST > SECOND  -->  (-1)  */
    if (((ThreadListElement *) first)->priority >
	((ThreadListElement *) second)->priority) {
	return (-1);
    }
    /*   FIRST < SECOND  -->  (1)  */
    if (((ThreadListElement *) first)->priority <
	((ThreadListElement *) second)->priority) {
	return (1);
    }
    /*   FIRST == SECOND  -->  (0)  */
    return (0);
}



/*  sort based on (in order):
 *      1) decreasing priority
 *      2) increasing sort_key
 *      3) last fetch time
 */
static int
icount_compare(const void *first, const void *second)
{

    /*  Priority:  FIRST > SECOND  -->  (-1)  */
    if (((ThreadListElement *) first)->priority >
	((ThreadListElement *) second)->priority) {
	return (-1);
    }
    /*  Priority:  FIRST < SECOND  -->  (1)  */
    if (((ThreadListElement *) first)->priority <
	((ThreadListElement *) second)->priority) {
	return (1);
    }
    /*  otherwise, must be equal priority  */

    /*   FIRST > SECOND  -->  (1)  */
    if (((ThreadListElement *) first)->sort_key >
	((ThreadListElement *) second)->sort_key) {
	return (1);
    }
    /*   FIRST < SECOND  -->  (-1)  */
    if (((ThreadListElement *) first)->sort_key <
	((ThreadListElement *) second)->sort_key) {
	return (-1);
    }
    /*  Same priority, Same sort-key... Use last fetch time  */
    /*  Earlier last-fetch times go first  */
    /*   FIRST > SECOND  -->  (1)  */
    if (((ThreadListElement *) first)->last_fetch >
	((ThreadListElement *) second)->last_fetch) {
	return (1);
    }
    /*   FIRST < SECOND  -->  (-1)  */
    if (((ThreadListElement *) first)->last_fetch <
	((ThreadListElement *) second)->last_fetch) {
	return (-1);
    }
    /*   FIRST == SECOND  -->  (0)  */
    return (0);
}

void
FullCPU::fetchRegStats()
{
    using namespace Stats;

    fetch_decisions
	.name(name() + ".FETCH:decisions")
	.desc("number of times the fetch stage chose between threads")
	;
    fetch_idle_cycles
            .name(name() + ".FETCH:idle_cycles")
            .desc("number of cycles where fetch stage was idle")
            ;

    fetch_idle_cycles_cache_miss
            .name(name() + ".FETCH:idle_cycles_cache_miss")
            .desc("number of cycles where fetch stage was idle due to cache miss")
            ;

    fetch_idle_icache_blocked_cycles
	.name(name() + ".FETCH:idle_icache_blocked_cycles")
	.desc("number of cycles where fetch was idle due to icache blocked")
	;

    qfull_iq_occupancy
	.init(number_of_threads)
	.name(name() + ".IFQ:qfull_iq_occ")
	.desc("Number of insts in IQ when fetch-queue full")
	.flags(total)
	;

    qfull_iq_occ_dist_
	.init(/* size */ number_of_threads,
	      /* base value */ 0,
	      /* last value */(int) (IQSize()),
	      /* bucket size */ 10)
	.name(name() + ".IFQ:qfull_iq_occ_dist")
	.desc("Number of insts in IQ when fetch-queue full")
	.flags(pdf)
	;
    qfull_rob_occupancy
	.init(number_of_threads)
	.name(name() + ".IFQ:qfull_rob_occ")
	.desc("Number of insts in ROB when fetch-queue full")
	.flags(total)
	;
    qfull_rob_occ_dist_
	.init(/* size */ number_of_threads,
	      /* base value */ 0,
	      /* last value */ (int)(ROB_size),
	      /* bucket size */ 10)
	.name(name() + ".IFQ:qfull_rob_occ_dist")
	.desc("Number of insts in ROB when fetch-queue full")
	.flags(pdf)
	;

    priority_changes
	.init(number_of_threads)
	.name(name() + ".FETCH:prio_changes")
	.desc("Number of times priorities were changed")
	.flags(total)
	;
    fetch_chances
	.init(number_of_threads)
	.name(name() + ".FETCH:chances")
	.desc("Number of fetch opportunities")
	.flags(total)
	;
    fetched_inst
	.init(number_of_threads)
	.name(name() + ".FETCH:count")
	.desc("Number of instructions fetched")
	.flags(total)
	;
    fetched_branch
	.init(number_of_threads)
	.name(name() + ".FETCH:branch_count")
	.desc("Number of branches fetched")
	.flags(total)
	;
    fetch_choice_dist
	.init(number_of_threads)
	.name(name() + ".FETCH:choice")
	.desc("Number of times we fetched from our first choice")
	.flags(total | pdf | dist)
	;
    fetch_nisn_dist
	.init(/* base value */ 0,
	      /* last value */ fetch_width,
	      /* bucket size */ 1)
	.name(name() + ".FETCH:rate_dist")
	.desc("Number of instructions fetched each cycle (Total)")
	.flags(pdf)
	;
    fetch_nisn_dist_ = new Distribution<>[number_of_threads];

    for (int i = 0; i < number_of_threads; i++) {
	stringstream lblStream;
	lblStream << name() << ".FETCH:rate_dist_" << i;
	stringstream descStream;
	descStream << "Number of instructions fetched each cycle (Thread "
		   << i << ")";

	fetch_nisn_dist_[i]
	    .init(/* base value */ 0,
		  /* last value */fetch_width,
		  /* bucket size */ 1)
	    .name(lblStream.str())
	    .desc(descStream.str())
	    .flags(pdf)
	    ;
    }
}

void
FullCPU::fetchRegFormulas()
{
    using namespace Stats;

    idle_rate
	.name(name() + ".FETCH:idle_rate")
	.desc("percent of cycles fetch stage was idle")
	.precision(2)
	;
    idle_rate = fetch_idle_cycles * 100 / numCycles;

    branch_rate
	.name(name() + ".FETCH:branch_rate")
	.desc("Number of branch fetches per cycle")
	.flags(total)
	;
    branch_rate = fetched_branch / numCycles;

    fetch_rate
	.name(name() + ".FETCH:rate")
	.desc("Number of inst fetches per cycle")
	.flags(total)
	;
    fetch_rate = fetched_inst / numCycles;

    fetch_chance_pct
	.name(name() + ".FETCH:chance_pct")
	.desc("Percentage of all fetch chances")
	;
    fetch_chance_pct = fetch_chances / sum(fetch_chances);

}

short
FetchQueue::num_total(){
	short total = num_valid + num_reserved + num_squashed;
	assert(total >= 0);
	return total;
}
