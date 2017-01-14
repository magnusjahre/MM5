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
#include "encumbered/cpu/full/create_vector.hh"
#include "encumbered/cpu/full/dep_link.hh"
#include "encumbered/cpu/full/iq/standard/iq_standard.hh"
#include "sim/builder.hh"

class RegInfoElement;

using namespace std;

//==========================================================================
//
//  The "standard" instruction queue implementation
//
//
//
//
//==========================================================================

StandardIQ::StandardIQ(string _name, unsigned _size, int pri,
		       bool _caps_valid, vector<unsigned> _caps)
    : BaseIQ(_name, _caps_valid, _caps)
{
    string n2(_name);

    name = _name;

    set_size(_size);
    pri_issue = pri;

    queue = new res_list<IQStation>(_size, true, 0);

    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	insts[i] = 0;

    total_insts = 0;

    n2 = n2 + ":RQ";
    ready_list = new ready_queue_t<IQStation, iq_standard_readyq_policy>
	(&cpu, n2.c_str(), _size, pri_issue);
}

StandardIQ::~StandardIQ()
{
    delete queue;
    delete ready_list;
}

BaseIQ::iterator
StandardIQ::add_impl(DynInst *inst, InstSeqNum seq, ROBStation *rob,
		     RegInfoElement *ri, NewChainInfo * new_chain)
{
    iterator p = queue->add_tail();

    DPRINTFR(IQ, "%d: %s: Adding instruction # %d, PC %d\n", curTick, name, seq, inst->PC);

    ++total_insts;
    ++insts[inst->thread_number];

    p->inst				= inst;
    p->in_LSQ			= false;
    p->ea_comp			= inst->isMemRef();
    p->seq				= seq;
    p->queued			= false;
    p->squashed			= false;
    p->rob_entry		= rob;
    p->segment_number	= 0;   // need this for dump_dep_tree();
    p->lsq_entry		= 0;   // may be modified later

    StaticInstPtr<TheISA> si = inst->staticInst;
    StaticInstPtr<TheISA> eff_si = p->ea_comp ? si->eaCompInst() : si;

    p->num_ideps = 0;
    for (int i = 0; i < eff_si->numSrcRegs(); ++i){
    	link_idep(p, eff_si->srcRegIdx(i));
    }

    // This shouldn't be necessary, since we should never look past
    // num_ideps in the array, but there are too many loops that go
    // all the way to TheISA::MaxNumSrcRegs.
    for (int i = p->num_ideps; i < TheISA::MaxInstSrcRegs; ++i) {
    	p->idep_ptr[i] = 0;
    	p->idep_reg[i] = 0;
    	p->idep_ready[i] = true;
    }

    if (p->ops_ready()) {
    	DPRINTFR(IQ, "%d: %s: Instruction # %d, PC %d ops are ready, adding to ready queue\n", curTick, name, seq, inst->PC);
    	ready_list_enqueue(p);
    	p->ready_timestamp = curTick;
    }

    return p;
}



//
//  Walk the output-dependence list for this instruction...
//
//  Only remove those entries which belong to instructions in this
//  queue.
//
//  We remove those entries from the chain that we handle here so
//  that the chain can be applied to the LSQ also
//
//  NOTE: IF YOU ARE CHANGING *THIS* ROUTINE, YOU PROBABLY WANT TO
//        CHANGE ls_queue::writeback() ALSO!
//
unsigned
StandardIQ::writeback(ROBStation *rob, unsigned queue_num)
{
    DepLink *olink, *olink_next;
    unsigned consumers = 0;

    for (int i=0; i<TheISA::MaxInstDestRegs; ++i) {
	for (olink = rob->odep_list[i][queue_num]; olink; olink = olink_next) {
	    //  grab the next link... we may delete this one
	    olink_next = olink->next();

	    if (olink->valid()) {
		res_list<IQStation>::iterator q_entry = olink->consumer();

		//  This instruction is writting back into an IQ...
		//  Ignore an LSQ entries in the consumer list
		if (q_entry->in_LSQ)
		    continue;

		++consumers;

		if (q_entry->idep_ready[olink->idep_num])
		    panic("output dependence already satisfied");

		// Mark the output as ready

		q_entry->idep_ready[olink->idep_num] = true;
		q_entry->idep_ptr[olink->idep_num] = 0;

		// are all the input operands ready?
		if (q_entry->ops_ready()) {

		    ready_list_enqueue(q_entry);
		    q_entry->ready_timestamp = curTick;

#if 0
		    if (cpu->chainGenerator)
			q_entry->rob_entry->last_op = olink->idep_num;
#endif
		}
	    }

	    //  Remove this link from the chain...
	    if (rob->odep_list[i][queue_num] != olink) {
		if (olink->prev_dep)
		    olink->prev_dep->next_dep = olink->next_dep;
		if (olink->next_dep)
		    olink->next_dep->prev_dep = olink->prev_dep;
	    } else {
		// Special handling for first element
		rob->odep_list[i][queue_num] = olink->next_dep;
		if (olink->next_dep)
		    olink->next_dep->prev_dep = 0;
	    }

	    //  Free link elements that belong to this queue
	    delete olink;
	}
    }

    return consumers;
}


//
//  Remove an IQ element, and its associated RQ entry
//
BaseIQ::iterator
StandardIQ::squash(BaseIQ::iterator &e)
{
    iterator next = 0;

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
	queue->remove(e);

	if (e->queued)
	    ready_list->remove(e->rq_entry);
    }

    return next;
};


//
//  Every-cycle IQ stats
//
void
StandardIQ::tick_model_stats()
{
}

void
StandardIQ::regModelStats(unsigned num_threads)
{
   ready_list->regStats(SimObject::name() + ":RQ", num_threads);
}

void
StandardIQ::regModelFormulas(unsigned num_threads)
{
    ready_list->regFormulas(SimObject::name() + ":RQ", num_threads);
}

void
StandardIQ::dump()
{
    cprintf("======================================================\n"
	    "%s Dump (cycle %d)\n"
	    "------------------------------------------------------\n"
	    "  Total instruction: %u\n", name, curTick, total_insts);
    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cprintf("  Thread %d instructions: %u\n", i, insts[i]);
    cout << "------------------------------------------------------\n";
    queue->dump();
    cout << "======================================================\n\n";
}

void StandardIQ::raw_dump()
{
    cprintf("======================================================\n"
	    "%s RAW Dump (cycle %d)\n"
	    "------------------------------------------------------\n"
	    "  Total instruction: %u\n", name, curTick, total_insts);
    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cprintf("  Thread %d instructions: %u\n", i, insts[i]);
    cout << "------------------------------------------------------\n";
    queue->raw_dump();
    cout << "======================================================\n\n";
}

void
StandardIQ::rq_dump()
{
    ready_list->dump();
}

void
StandardIQ::rq_raw_dump()
{
    ready_list->raw_dump();
}

//////////////////////////////////////////////////////////////////////////////
//   Interface to INI file mechanism
//////////////////////////////////////////////////////////////////////////////


BEGIN_DECLARE_SIM_OBJECT_PARAMS(StandardIQ)

    Param<int> size;
    Param<bool> prioritized_issue;
    VectorParam<unsigned> caps;

END_DECLARE_SIM_OBJECT_PARAMS(StandardIQ)


BEGIN_INIT_SIM_OBJECT_PARAMS(StandardIQ)

    INIT_PARAM(size, "number of entries"),
    INIT_PARAM_DFLT(prioritized_issue, "use thread priorities in issue",
		    false),
    INIT_PARAM(caps, "IQ caps")

END_INIT_SIM_OBJECT_PARAMS(StandardIQ)


CREATE_SIM_OBJECT(StandardIQ)
{
    return new StandardIQ(getInstanceName(),
			  size, prioritized_issue,
			  caps.isValid(), caps);
}

REGISTER_SIM_OBJECT("StandardIQ", StandardIQ)
