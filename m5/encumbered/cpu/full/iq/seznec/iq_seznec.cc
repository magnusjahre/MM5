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
#include "encumbered/cpu/full/iq/seznec/iq_seznec.hh"
#include "encumbered/cpu/full/issue.hh"
#include "sim/builder.hh"

class RegInfoElement;

using namespace std;

#define MISS_ADDITIONAL_LATENCY  10

SeznecIQ::~SeznecIQ()
{
    delete inst_store;
    delete issue_buffer;
    delete ready_list;
    delete hm_predictor;

    for (int i = 0; i < num_lines; ++i)
	delete presched_buffer[i];

    delete[] presched_buffer;
}


//
//  Constructor
//
SeznecIQ::SeznecIQ(string _name, unsigned _num_lines, unsigned _line_size,
		   unsigned _issue_buf_size, bool _use_hm_pred)
    : BaseIQ(_name)
{
    num_lines        = _num_lines;
    line_size        = _line_size;
    issue_buf_size   = _issue_buf_size;
    use_hm_predictor = _use_hm_pred;

    //
    //  Create storage for the actual instructions
    //
    //  the modeled structures will hold iterators to this store
    //
    inst_store = new res_list<IQStation>(MAX_INSNS, true, 0);

    //
    //  The pre-schedule buffer
    //
    presched_buffer = new iq_it_list_ptr[num_lines];
    for (int i = 0; i < num_lines; ++i)
	presched_buffer[i] = new iq_iterator_list(line_size, true, 0);
    active_line_index = 0;

    presched_insts = 0;
    total_insts = 0;
    for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	presched_thread_insts[i] = 0;
	thread_insts[i] = 0;
    }

    //
    //  The issue buffer
    //
    issue_buffer = new iq_iterator_list(issue_buf_size, true, 0);

    //
    //  The ready queue
    //
    ready_list = new ready_queue_t<IQStation, RQSeznecPolicy>
	  (&cpu, "IQ_RQ", issue_buf_size, false);

    hm_predictor = new SaturatingCounterPred("IQ:HMP", "miss", "hit", 14);
}



SeznecIQ::iq_iterator
SeznecIQ::add_impl(DynInst *inst, InstSeqNum seq, ROBStation *rob,
		   RegInfoElement *ri, NewChainInfo * new_chain)
{
    iterator p = inst_store->add_tail();

    if (p.isnull())
	return 0;

    p->inst		= inst;
    p->in_LSQ		= false;
    p->ea_comp		= inst->isMemRef();
    p->seq		= seq;
    p->queued		= false;
    p->squashed		= false;
    //p->blocked	= false;
    p->rob_entry	= rob;
    p->segment_number	= 0;    // need this for dump_dep_tree()
    p->in_issue_buffer	= false;
    p->lsq_entry	= 0; // may be set later

    //=====================================================
    //
    //  Now, determine the schedule for this instruction
    //
    new_slot_t slot = schedule_inst(p);

    //
    //  Finish adding the instruction if we were able to
    //  schedule it
    //
    if (slot.valid && !presched_buffer[slot.schedule_line]->full()) {
	p->queue_entry = presched_buffer[slot.schedule_line]->add_tail(p);
	p->presched_line = slot.schedule_line;

	StaticInstPtr<TheISA> si = inst->staticInst;

	for (int i = 0; i < si->numDestRegs(); ++i)
	    reg_info[si->destRegIdx(i)].use_line = slot.result_line;

	StaticInstPtr<TheISA> eff_si = p->ea_comp ? si->eaCompInst() : si;

	p->num_ideps = 0;
	for (int i = 0; i < eff_si->numSrcRegs(); ++i)
	    link_idep(p, eff_si->srcRegIdx(i));

	// This shouldn't be necessary, since we should never look past
	// num_ideps in the array, but there are too many loops that go
	// all the way to TheISA::MaxNumSrcRegs.
	for (int i = p->num_ideps; i < TheISA::MaxInstSrcRegs; ++i) {
	    p->idep_ptr[i] = 0;
	    p->idep_reg[i] = 0;
	    p->idep_ready[i] = true;
	}

	++total_insts;
	++thread_insts[inst->thread_number];

	++presched_insts;
	++presched_thread_insts[inst->thread_number];
    } else {
	inst_store->remove(p);
	p = 0;
    }

    return p;
}


SeznecIQ::new_slot_t
SeznecIQ::schedule_inst(SeznecIQ::iterator &p)
{
    DynInst *inst = p->inst;

    //  Default for where this instruction should go
    unsigned max_line = active_line_index + 1;
    if (max_line >= num_lines)
	max_line = 0;

    //
    //  Loop through all the possible input dependencies
    //
    for (int i = 0; i < inst->numSrcRegs(); ++i) {
	int reg = inst->srcRegIdx(i);

	//
	//  Find the use_line farthest in the future
	//
	reg_info_t *info = &reg_info[reg];

	if (info->scheduled && later_than(info->use_line, max_line))
	    max_line = info->use_line;
    }


    // determine when this instruction's result should be
    // available
    unsigned use_line = max_line + cpu->FUPools[0]->getLatency(p->opClass());

    //  must take cache access time into account for LOAD instructions
    if (inst->isLoad())
	use_line += cache_hit_latency;

    //  If we think this will be a load miss... adjust the "use_line"
    if (use_hm_predictor && inst->isLoad()) {
	if (hm_predictor->predict(inst->PC) == 0) {
	    use_line += MISS_ADDITIONAL_LATENCY;
	    p->rob_entry->hm_prediction = MA_CACHE_MISS;
	} else {
	    p->rob_entry->hm_prediction = MA_HIT;
	}
    }


    //  Wrap this line number at NUM_LINES
    if (use_line >= num_lines)
	use_line -= num_lines;

    //  Assume we're ok for now
    new_slot_t rv;
    rv.valid = true;
    rv.schedule_line = max_line;
    rv.result_line = use_line;

    //
    //  Check to see if we've passed the active_line
    //
    if (use_line == active_line_index ||
	(use_line > active_line_index && max_line < active_line_index))
	rv.valid = false;

    return rv;
}


//
//  This function determines whether the first line number
//  is farther into the future than the second.
//
//  It handles wrapping at NUM_LINES & active_line_index
//
bool
SeznecIQ::later_than(unsigned _first, unsigned _second)
{
    unsigned first = _first;
    unsigned second = _second;

    //
    //  Compensate for the position of the index
    //
    if (first < active_line_index)
	first += num_lines;
    if (second < active_line_index)
	second += num_lines;

    return (first > second);
}



//  Broadcast result of executed instruction back to IQ
unsigned
SeznecIQ::writeback(ROBStation *rob, unsigned queue_num)
{
    DepLink *olink, *olink_next;
    unsigned consumers = 0;

    for (int i = 0; i < TheISA::MaxInstDestRegs; ++i) {
	for (olink = rob->odep_list[i][queue_num]; olink; olink = olink_next) {

	    //  grab the next link... we may delete this one
	    olink_next = olink->next();

	    if (olink->valid()) {
		res_list<IQStation>::iterator q_entry = olink->consumer();

		//  This is the IQ... ignore LSQ entries!
		if (q_entry->in_LSQ)
		    continue;

		if (q_entry->idep_ready[olink->idep_num])
		    panic("output dependence already satisfied");

		++consumers;

		// Mark the output as ready

		q_entry->idep_ready[olink->idep_num] = true;

		// are all the input operands ready?
		// We have to be in the issue queue to do this
		if (q_entry->ops_ready() && q_entry->in_issue_buffer) {
		    ready_list_enqueue(q_entry);
		    q_entry->ready_timestamp = curTick;
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



//  For things which must be done every cycle
void
SeznecIQ::tick()
{
    //
    //  Move instructions from the active-line into the
    //  issue buffer
    //

    if (presched_buffer[active_line_index]->empty()) {
	if (++active_line_index >= num_lines)
	    active_line_index = 0;

	return;
    }

    if (!issue_buffer->full()) {
	unsigned num_to_promote = issue_buffer->num_free();

	//
	//  Move instructions from the active line into
	//  the issue buffer
	//
	for (int i = 0; i < num_to_promote; ++i) {
	    iq_it_list_iterator p =
		presched_buffer[active_line_index]->head();

	    presched_buffer[active_line_index]->remove(p);

	    (*p)->queue_entry = issue_buffer->add_tail(*p);
	    (*p)->in_issue_buffer = true;

	    --presched_insts;
	    --presched_thread_insts[(*p)->thread_number()];

	    //
	    //  Enqueue the instruction if its ops are ready
	    //
	    if ((*p)->ops_ready()) {
		ready_list_enqueue(*p);
		(*p)->ready_timestamp = curTick;
	    }

	    //  we may leave early if the line is empty
	    if (presched_buffer[active_line_index]->empty())
		break;
	}

	//
	//  If we promoted instructions, and emptied the active
	//  line, then advance the active line for next cycle.
	//
	if (presched_buffer[active_line_index]->empty() &&
	    (active_line_index + 1) >= num_lines)
	    active_line_index = 0;
    }
}


SeznecIQ::iq_iterator
SeznecIQ::oldest()
{
    return head().notnull() ? head() : 0;
}

SeznecIQ::iq_iterator
SeznecIQ::oldest(unsigned thread)
{
    iq_iterator i;

    for (i = head();
	 i.notnull() && (i->thread_number() != thread);
	 i = i.next());

    return i.notnull() ? i : 0;
}


void SeznecIQ::dump() {
    cout << "======================================================\n";
    cout << name() << " Dump (cycle " << curTick << ")\n";
    cout << "------------------------------------------------------\n";
    cout << "  Total instruction: " << total_insts << endl;
    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cout << "  Thread " << i << " instructions: " << thread_insts[i]
	     << endl;
    cout << "------------------------------------------------------\n";
    cout << "Preschedule Buffer: (active_line_index=" << active_line_index
	 << ")\n";

    unsigned i=active_line_index;
    do {
	if (!presched_buffer[i]->empty()) {
	    cout << "Line " << i << ":\n";
	    presched_buffer[i]->dump();
	    cout << "......................................\n";
	}

	if (++i >= num_lines)
	    i = 0;
    } while (i != active_line_index);

    cout << "------------------------------------------------------\n";
    cout << "Issue Buffer:\n";
    issue_buffer->dump();

    cout << "======================================================\n\n";
}


///////////////////////////////////////////////////////////////////////////////
//
//  This ready-list policy
//
class RQSeznecPolicy
{
  public:

    static bool goes_before(FullCPU *cpu,
			    res_list<IQStation>::iterator &first,
			    res_list<IQStation>::iterator &second,
			    bool use_thread_priorities) {
	bool rv = false;

	unsigned first_p = cpu->thread_info[first->thread_number()].priority;
	unsigned second_p = cpu->thread_info[second->thread_number()].priority;

	if (use_thread_priorities) {

	    // if first has higher priority...
	    if (first_p > second_p) {
		rv = true;
	    } else if (second_p > first_p) {
		// second higher priority
		rv = false;
	    } else if (first->seq < second->seq) {
		//  same priority
		rv = true;
	    }
	} else if (first->seq < second->seq)
	    rv = true;

	return rv;
    }
};


//////////////////////////////////////////////////////////////////////////////
//   Interface to INI file mechanism
//////////////////////////////////////////////////////////////////////////////


BEGIN_DECLARE_SIM_OBJECT_PARAMS(SeznecIQ)

    Param<unsigned> num_lines;
    Param<unsigned> line_size;
    Param<unsigned> issue_buf_size;
    Param<bool>     use_hm_predictor;

END_DECLARE_SIM_OBJECT_PARAMS(SeznecIQ)


BEGIN_INIT_SIM_OBJECT_PARAMS(SeznecIQ)

      INIT_PARAM(num_lines,        "number of prescheduling lines"),
      INIT_PARAM(line_size,        "number of insts per prescheduling line"),
      INIT_PARAM(issue_buf_size,   "number of issue buffer entries"),
      INIT_PARAM(use_hm_predictor, "use hit/miss predictor")

END_INIT_SIM_OBJECT_PARAMS(SeznecIQ)


CREATE_SIM_OBJECT(SeznecIQ)
{
    return new SeznecIQ(getInstanceName(),
			num_lines,
			line_size,
			issue_buf_size,
			use_hm_predictor);
}

REGISTER_SIM_OBJECT("SeznecIQ", SeznecIQ)
