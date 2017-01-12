/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/dep_link.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/rob_station.hh"

class RegInfoElement;

using namespace std;

//==========================================================================
//
//  The "standard" Load/Store queue implementation
//
//
//
//
//==========================================================================

load_store_queue::~load_store_queue()
{
    delete queue;
    delete ready_list;
}


load_store_queue::load_store_queue(FullCPU *_cpu, const string &_name,
				   unsigned _size, bool pri)
    : BaseIQ(_name.c_str())
{
    // initialize internals: no dispatch or issue BW limits, only one
    // queue
    init(_cpu, 0, 0, 0);

    set_size(_size);
    pri_issue = pri;

    queue = new res_list<IQStation>(_size, true, 0);

    for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	insts[i] = 0;
	loads[i] = 0;
	stores[i] = 0;
    }

    total_insts = 0;
    total_stores = 0;
    total_loads = 0;

    string n2(_name);
    n2 += "_ReadyQ";
    ready_list = new ready_queue_t<IQStation,lsq_readyq_policy>
	               (&cpu, n2, _size, pri_issue);
};


void
load_store_queue::dump()
{
    cprintf("======================================================\n"
	    "%s Dump (cycle %d)\n"
	    "------------------------------------------------------\n"
	    "  Total instruction: %u\n", name(), curTick, total_insts);
    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cprintf("  Thread %d instructions: %u\n", i, insts[i]);
    cprintf("------------------------------------------------------\n");
    queue->dump();
    cprintf("======================================================\n\n");
}


void
load_store_queue::raw_dump()
{
    cprintf("======================================================\n"
	    "%s RAW Dump (cycle %n)\n"
	    "------------------------------------------------------\n"
	    "  Total instruction: %u\n", name(), curTick, total_insts);
    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cprintf("  Thread %d instructions: %u\n", i, insts[i]);
    cprintf("------------------------------------------------------\n");
    queue->raw_dump();
    cprintf("======================================================\n\n");
}


void
load_store_queue::rq_dump()
{
    ready_list->dump();
}


void
load_store_queue::rq_raw_dump()
{
    ready_list->raw_dump();
}


BaseIQ::iterator
load_store_queue::add_impl(DynInst *inst, InstSeqNum seq,
		ROBStation *rob, RegInfoElement *ri,
		NewChainInfo *c)
{
	DPRINTF(IQ, "Adding instruction # %d, PC %d\n", seq, inst->PC);

	BaseIQ::iterator p = queue->add_tail();

	++total_insts;
	++insts[inst->thread_number];

	if (inst->isLoad()) {
		++total_loads;
		++loads[inst->thread_number];
	} else if (inst->isStore()) {
		++total_stores;
		++stores[inst->thread_number];
	} else {
		// shouldn't get here otherwise!
		assert(inst->isMemBarrier());
	}

	p->inst		= inst;
	p->in_LSQ		= true;
	p->ea_comp		= false;
	p->seq		= seq;
	p->queued		= false;
	p->squashed		= false;
	//p->blocked	= false;
	p->mem_result	= MA_NOT_ISSUED;
	p->head_of_chain	= false;
	p->rob_entry	= rob;
	p->num_ideps	= 0;

	if (!inst->isMemBarrier()) {
		DPRINTF(IQ, "Instruction # %d (PC %d) is not a memory barrier\n", seq, inst->PC);
		// This dummy idep is for the input dependence on the effective
		// address computation in the IQ.  By putting it first we
		// guarantee that it's at index 0 (MEM_ADDR_INDEX), which is how
		// the writeback of the EA computation finds it (ugliness
		// inherited from SimpleScalar).
		link_idep(p);


		// now link in non-EA input deps (if any: should just be store
		// data on stores)
		StaticInstPtr<TheISA> eff_si = inst->staticInst->memAccInst();

		for (int i = 0; i < eff_si->numSrcRegs(); ++i){
			link_idep(p, eff_si->srcRegIdx(i));
		}
	}

	// This shouldn't be necessary, since we should never look past
	// num_ideps in the array, but there are too many loops that go
	// all the way to TheISA::MaxNumSrcRegs.
	for (int i = p->num_ideps; i < TheISA::MaxInstSrcRegs; ++i) {
		p->idep_ptr[i] = 0;
		p->idep_reg[i] = 0;
		p->idep_ready[i] = true;
	}

	return p;
};


//
//  Walk the output-dependence list for this instruction...
//
//  Only remove those entries which belong to instructions in this
//  queue.
//
//  Note that the only instructions that actually get a value here will
//  be stores... Effective addresses get updated in rob_station::writeback()
//  so this must be the DATA part of an LSQ op...
//
//  We remove those entries from the chain that we handle here so
//  that the chain can be applied to the IQ also
//
//  NOTE: IF YOU ARE CHANGING *THIS* ROUTINE, YOU PROBABLY WANT TO
//        CHANGE iq_standard::writeback() ALSO!
//
unsigned load_store_queue::writeback(ROBStation *rob, unsigned ignored)
{
	unsigned consumers = 0;
	unsigned const queue_num = 0;

	DPRINTF(IQ, "Writing back instruction # %d (PC %d)\n", rob->seq, rob->inst->PC);

	for (int index = 0; index < TheISA::MaxInstDestRegs; ++index) {
		DepLink *olink = rob->odep_list[index][queue_num];
		DepLink *olink_next;
		for (; olink; olink = olink_next) {
			//  grab the next link... we may delete this one
			olink_next = olink->next();

			if (olink->valid()) {
				res_list<IQStation>::iterator q_entry = olink->consumer();

				//  This instruction is doing writeback into the LSQ...
				//  Ignore an IQ entries in the consumer list
				if (!q_entry->in_LSQ) {
					continue;
				}

				++consumers;

				if (q_entry->idep_ready[olink->idep_num])
					panic("output dependence already satisfied");

				assert(q_entry->inst->isStore());

				// input is now ready
				q_entry->idep_ready[olink->idep_num] = true;
				q_entry->idep_ptr[olink->idep_num] = 0;

				// are all the input operands ready?
				if (q_entry->ops_ready()) {
					ready_list_enqueue(q_entry);
					q_entry->ready_timestamp = curTick;
				}
			}

			//  Remove this link from the chain...
			if (rob->odep_list[index][queue_num] != olink) {
				if (olink->prev_dep)
					olink->prev_dep->next_dep = olink->next_dep;
				if (olink->next_dep)
					olink->next_dep->prev_dep = olink->prev_dep;
			} else {
				// Special handling for first element
				rob->odep_list[index][queue_num] = olink->next_dep;
				if (olink->next_dep)
					olink->next_dep->prev_dep = 0;
			}

			//  Free link elements that belong to this queue
			delete olink;
		}
	}

	return consumers;
}


BaseIQ::iterator load_store_queue::squash(BaseIQ::iterator &e)
{
    iterator next;

    if (e.notnull()) {
	next = e.next();

	for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	    if (e->idep_ptr[i]) {
		delete e->idep_ptr[i];
		e->idep_ptr[i] = 0;
	    }
	}

	--total_insts;
	--insts[e->thread_number()];
	if (e->inst->isLoad()) {
	    --total_loads;
	    --loads[e->thread_number()];
	} else {
	    --total_stores;
	    --stores[e->thread_number()];
	}

	queue->remove(e);

	if (e->queued) {
	    ready_list->remove(e->rq_entry);
	}
    }

    return next;
}

void
load_store_queue::regModelStats(unsigned threads)
{
   ready_list->regStats(SimObject::name() + ":RQ", threads);
}

void
load_store_queue::regModelFormulas(unsigned threads)
{
    ready_list->regFormulas(SimObject::name() + ":RQ", threads);
}
