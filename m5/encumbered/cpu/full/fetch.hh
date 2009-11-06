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

#ifndef __ENCUMBERED_CPU_FULL_FETCH_HH__
#define __ENCUMBERED_CPU_FULL_FETCH_HH__

#include "base/statistics.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/bpred.hh"
#include "encumbered/cpu/full/floss_reasons.hh"
#include "sim/eventq.hh"

/* Define flags that connote the different types of memory/fetch stalls */
#define BRANCH_STALL 	0x01

class FullCPU;

class fetch_instr_rec_t
{
  public:
    DynInst *inst;
    short  thread_number;
    bool squashed;

    bool contents_valid;

    fetch_instr_rec_t() {
	contents_valid = false;
	squashed = true;
	inst = 0;
    }

    void squash() {
	assert(contents_valid);
	assert(!squashed);

	//  reserved entries don't have inst set yet...
	if (inst) {
	    inst->squash();
	    delete inst;
	    inst = 0;
	}

	squashed = true;
    }
};


struct FetchQueue
{
    struct fetch_instr_rec_t *instrs;
    short head;
    short tail;
    short size;
    short index_mask;
    bool mt_frontend;

    // num_threads refers to the number of threads in the non-mt queue,
    // but actually referrs to the thred number in the mt case.
    short num_threads;

    // there are three types of instructions that get counted:
    // present-valid: actually stored in the queue, not squashed
    // present-squashed: actually stored in the queue, but have been squashed
    //     (instrs[i].squashed is set)
    // reserved: icache fetch has been initiated, but is not yet complete
    //     (not stored in ifq, DynInst structs are in icache_output_buffer)
    //
    //  Each instruction is counted as either valid, reserved, or squashed
    //
    short num_valid;
    short num_reserved;
    short num_squashed;
    short num_valid_thread[SMT_MAX_THREADS];
    short num_reserved_thread[SMT_MAX_THREADS];
    short num_squashed_thread[SMT_MAX_THREADS];

    // initialization
    void init(FullCPU *cpu, int size, int number_of_threads);

    // increment a queue index (with wrap)
    int incr(int index) {
	return ((index + 1) & index_mask);
    }

    short num_total();
    short num_total(unsigned t)
    {
	assert((mt_frontend && t == num_threads) ||
	       (!mt_frontend && t >= 0 && t <= num_threads));
	return num_valid_thread[t] + num_reserved_thread[t]
	    + num_squashed_thread[t];
    }

    short num_available() { return num_valid + num_squashed; }
    short num_available(unsigned t)
    {
	assert((mt_frontend && t == num_threads) ||
	       (!mt_frontend && t >= 0 && t <= num_threads));
	return num_valid_thread[t] + num_squashed_thread[t];
    }

    void reserve(int thread);

    // append a newly fetched instruction
    void append(DynInst *);

    //  grab the next instruction from the queue
    DynInst *pull();

    // squash all instructions from the specified thread
    void squash();          // Used for MT front-end
    void squash(int thread);    // Used for non-MT front-end

    void dump() { dump(" "); }
    void dump(const std::string &str);
};

struct ThreadListElement
{
    int thread_number;
    int blocked;
    int priority;
    int sort_key;
    Tick last_fetch;
};

enum FetchPolicyEnum {
    RR,			// round-robin
    IC,			// instruction-count
    ICRC,
    Ideal_NP,		// Ideal non-prioritized
    Conf,		// Branch-confidence
    Redundant,		// Redundant
    RedIC,		// Hybrid Redundant/IC)
    RedSlack,		// Redundant: target slack
    Rand,
    NUM_FETCH_POLICIES
};


void fetch_uninit();

/*
 * Stored in PC[] if we have no valid PC to fetch from.  Must not match
 * any potentially valid PC.
 */
#define PC_PRED_TAKEN_BTB_MISS		1
#define PC_NO_LEADING_THREAD_PRED	2

/* Schedulable event to invoke clear_fetch_stall at a particular cycle */
class ClearFetchStallEvent : public Event
{
  private:
    // event data fields
    FullCPU *cpu;
    int thread_number;
    int stall_type;

  public:
    // constructor
    ClearFetchStallEvent(FullCPU *_cpu, int thread, int type)
	: Event(&mainEventQueue)
    {
	cpu = _cpu;
	thread_number = thread;
	stall_type = type;
    }

    // event execution function
    void process();

    const char* description();
};


/* Schedulable event to invoke clear_fetch_stall at a particular cycle */
class FetchCompleteEvent : public Event
{
  private:
    // event data fields
    FullCPU *cpu;
    short thread_number;
    short first_index; // starting index in icache_output_buffer[thread_number]
    short num_insts;
    bool ready;
    InstSeqNum first_seq_num;
    MemReqPtr req;

  public:
    // constructor
    FetchCompleteEvent(FullCPU *_cpu, int thread,
		       int _first_index, int _num_insts,
		       InstSeqNum _seq,
		       MemReqPtr _req = NULL)
	: Event(&mainEventQueue)
    {
	cpu = _cpu;
	thread_number = thread;
	first_index = _first_index;
	num_insts = _num_insts;
	first_seq_num = _seq;
	ready = false;
	req = _req;
    }

    // event execution function
    void process();

    const char* description();

    // squash this fetch before it returns... currently the cache
    // operation is *not* squashed, but the instructions are discarded
    // when they arrive
    void squash();
};

#endif // __ENCUMBERED_CPU_FULL_FETCH_HH__
