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

#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

#include "base/cprintf.hh"
#include "base/statistics.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/create_vector.hh"
#include "encumbered/cpu/full/dep_link.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/iq/segmented/iq_seg.hh"
#include "encumbered/cpu/full/iq/segmented/iq_segmented.hh"
#include "encumbered/cpu/full/iq/segmented/seg_chain.hh"
#include "encumbered/cpu/full/issue.hh"
#include "encumbered/cpu/full/ls_queue.hh"
#include "encumbered/cpu/full/reg_info.hh"
#include "sim/eventq.hh"
#include "sim/stats.hh"

using namespace std;

#define use_dest_sort   1
#define use_pushdown    1

//==========================================================================
//
//  The segment implementation
//
//==========================================================================

//
//  Constructor
//
segment_t::segment_t(SegmentedIQ *iq, string n,
		     unsigned seg_num, unsigned num_segs,
		     unsigned sz, unsigned n_chains, unsigned thresh,
		     bool pipelined, bool en_pri)
{
    //  Allows us to reference our own queue data
    seg_queue = iq;

    name_string = n;
    segment_number = seg_num;
    num_segments = num_segs;
    size = sz;
    num_chains = n_chains;
    threshold = thresh;

    // we need to set this after the chain info table gets built
    chain_info = 0;

    //
    //  The list of iterators to the instructions we're tracking
    //
    //  ["size" elements, allocated, doesn't grow]
    //
    queue = new iq_iterator_list(size, true, 0);

    chains = new iq_iterator_list * [num_chains];
    for (int i = 0; i < num_chains; ++i)
	chains[i] = new iq_iterator_list(20, true, 2);

    self_timed_list = new iq_iterator_list(20, true, 2);


    //  Keep track of which instructions are "ready" to promote (or issue)
    //
    //  NOTE: Segment zero uses a different ready-list policy!
    string seg_name = name() + ":RQ";
    if (seg_num != 0) {
	ready_list =
	    new ready_queue_t<IQStation, RQ_Seg_Policy>
	    (&(seg_queue->cpu), seg_name, size, en_pri);
    } else {
	ready_list =
	    new ready_queue_t<IQStation, RQ_Issue_Policy>
	    (&(seg_queue->cpu), seg_name, size, en_pri);
    }


    //
    //  Initilize statistics, etc
    //
    total_insts = 0;

    segment_full = 0;
    segment_empty = 0;

    cycles_low_ready = 0;
    insts_fanout_loads = 0;
    insts_waiting_for_loads = 0;
    insts_waiting_for_unissued = 0;

    for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	insts[i] = 0;
    }
}



//
//  Copy an instruction into this segment. Add this instruction to
//  the correct specified chain.
//
//  The instruction is checked to see if it is in the "right" segment
//  and if not, is automatically marked as ready to promote. This assumes
//  that the queue _forward_of_this_segment_ is in a consistent state.
//
//  This allows instructions that have not reached their correct segment
//  to move forward, even though their chain-head isn't moving.
//
segment_t::iq_iterator
segment_t::add(segment_t::iq_iterator &p)
{
    unsigned n_chains = 0;

    //  Add this iterator to this segment's queue
    iq_it_list_iterator q = queue->add_tail(p);
    if (q.isnull())
	return 0;

    //  So we can remove it from the segment queue
    p->queue_entry = q;

    p->queued = false;
    p->rq_entry = 0;
    p->segment_number = segment_number;

    //
    //  Add this instruction to it's chain(s)
    //
    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	if (p->idep_info[i].chained) {
	    unsigned c_num = p->idep_info[i].follows_chain;

	    bool duplicate = false;
	    for (int j = 0; j < i; ++j) {
		if (p->idep_info[j].chained &&
		    (p->idep_info[j].follows_chain == c_num)) {

		    duplicate = true;
		    break;
		}
	    }

	    if (! duplicate) {
		p->idep_info[i].chain_entry = chains[c_num]->add_tail(p);

		++n_chains;
	    } else {
		p->idep_info[i].chain_entry = 0;
	    }

	}
    }

    //
    //  Only if this instruction is completely unchained, do we add it
    //  to the self-timed list
    //
    if (n_chains == 0)
	p->idep_info[0].chain_entry = self_timed_list->add_tail(p);

    //  Has this instruction reached the "right" segment yet?
    if (segment_number != 0 && promotable(p))
	enqueue(p);

    ++insts[p->thread_number()];
    ++total_insts;

    //
    //  SEGMENT 0:  Enqueue incoming instructions if their input
    //              dependencies have been met.
    //
    if ((segment_number == 0) && p->ops_ready()) {
	// we shouldn't become ready too early
	//	assert(p->delay == 0);

	enqueue(p);
    }

    return p;
}


//
//  Removing an instruction (actually an iterator to an instruction) from
//  this queue segment
//
//    (1) Remove the inst from the chain it's following (or the unchained list)
//    (2) Remove the inst from the chain it's the head of
//    (3) Remove the inst from the ready-queue (if queued)
//    (4) Remove the iterator from the queue
//
segment_t::iq_iterator
segment_t::remove(segment_t::iq_iterator &e)
{
    iq_iterator next = e.next();
    unsigned n_chains = 0;

    if (e.notnull()) {
	--insts[e->thread_number()];
	--total_insts;

	for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	    //
	    //  Remove it from the correct chain...
	    //
	    if (e->idep_info[i].chained) {
		unsigned chain = e->idep_info[i].follows_chain;
		chains[chain]->remove(e->idep_info[i].chain_entry);
		++n_chains;
	    }
	}

	if (n_chains == 0)
	    self_timed_list->remove(e->idep_info[0].chain_entry);

	//
	//  If this inst was queued
	//
	if (e->queued)
	    ready_list->remove(e->rq_entry);

	//
	//  Finally, remove it from the segment queue list
	//
	queue->remove(e->queue_entry);
    }

    return next;
}


//
//  When the head instruction issues, we leave the instructions on
//  the chain, but tell it to self-time.
//
//  We may want to stop the self-time operation later...
//
void
segment_t::self_time(ROBStation *rob)
{
    if (rob->seq == (*chain_info)[rob->head_chain].creator)
	(*chain_info)[rob->head_chain].self_timed = true;
}


//
//  Stop the chain from self-timing
//
void
segment_t::stop_self_time(ROBStation *rob)
{
    if (rob->seq == (*chain_info)[rob->head_chain].creator)
	(*chain_info)[rob->head_chain].self_timed = false;
}


//
//  When the head instruction writes-back, we'll free the chain,
//  Allowing the individual instructions to sit on the self_timed_list
//
void
segment_t::release_chain(ROBStation *rob_entry)
{
    iq_it_list_iterator n;
    unsigned t_count = 0;

    unsigned chain_num = rob_entry->head_chain;


#if 0
    // We don't want to free a chain that really belongs to someone else!
    if (rob_entry->seq != seg_queue->chain_info[chain_num]->creator)
 	return;
#endif

    //
    //  Walk the list of instructions on this chain
    //
    iq_it_list_iterator i = chains[chain_num]->head();
    for (; i.notnull(); i = n) {

	unsigned n_chains = 0;

	n = i.next();

	for (int j = 0; j < TheISA::MaxInstSrcRegs; ++j) {
	    if ((*i)->idep_info[j].chained) {
		if ((*i)->idep_info[j].follows_chain == chain_num) {
		    (*i)->idep_info[j].chained = false;
		    //chains[chain_num]->remove(i);
		    ++t_count;
		} else {
		    ++n_chains;
		}
	    }
	}

	//  If this inst is no longer on _any_ chain... put it on the
	//  self-timed list
	if (n_chains == 0) {
	    (*i)->idep_info[0].chain_entry = self_timed_list->add_tail(*i);

	    //  We need to set the self-timed flag in any register produced
	    //  by this instruction
	    ROBStation *rob = (*i)->rob_entry;
	    for (int j = 0; j < rob->num_outputs; ++j) {

		RegInfoElement &r =
		    (*reg_info_table)[rob->thread_number][rob->onames[j]];
		//
		//  If this ROB entry is responsible for producing this
		//  arch register result...
		//
		if (r.producer() && (r.producer()->seq == rob->seq)) {

		    //  Clear the chained flag so that future consumers
		    //  will know not to wait on the chain head
		    r.unChain();
		}
	    }
	}
    }

    //  instead of the individual removes above...
    assert(chains[chain_num]->count() == t_count);
    chains[chain_num]->clear();
}

void
segment_t::check_promotable()
{
    //
    //  All segments except segment 0 need to determine which instructions are
    //  promotable.
    //
    //  Segment zero instructions are enqueued either on entry
    //  (if no outstanding ideps) or when an instruction writes-back.
    //



    //
    //  Walk the entire list of instructions...
    //    (1)  Decrement the delay values as necessary
    //    (2)  Enqueue instructions that are promotable
    //
    iq_it_list_iterator i = queue->head();
    for (; i.notnull(); i = i.next()) {
	bool recalc_position = false;

	//
	//  Do the easy case first
	//
	//  (this works because any instruction that is ops-ready MUST be
	//   self-timed)
	//
	if ((*i)->ops_ready() && !(*i)->queued)
	    enqueue(*i);

	//
	// Decrement the delay counters depending on the state of the chain
	//
	for (int j = 0; j < TheISA::MaxInstSrcRegs; ++j) {
	    if (!(*i)->idep_ready[j]) {
		if ((*i)->idep_info[j].chained) {
		    unsigned c_num = (*i)->idep_info[j].follows_chain;

		    //  If the head of this chain was promoted,
		    //  or is self-timed, then we need to see if we are
		    //  promotable
		    if (chain_info->head_was_promoted(c_num, segment_number) ||
			(*chain_info)[c_num].self_timed)
		    {
			if ((*chain_info)[c_num].self_timed) {
			    decrement_delay(*i, j);
			    recalc_position = true;
			}

			//  Don't enqueue for segment zero
			if (segment_number > 0)
			    if (!(*i)->queued && promotable(*i))
				enqueue(*i);
		    }
		} else {
		    //  Self-timed...
		    decrement_delay(*i, j);
		    recalc_position = true;

		    //  Don't enqueue for segment zero
		    if (segment_number > 0 && !(*i)->queued && promotable(*i))
			enqueue(*i);
		}
	    } else {
		decrement_delay(*i, j);
		recalc_position = true;
	    }
	}

	if (recalc_position)
	    (*i)->dest_seg = proper_segment(*i);
    }
}


void
segment_t::decrement_delay(iq_iterator i, int op_num)
{
    bool all_zero = true;

    if (i->idep_info[op_num].delay != 0) {
	--(i->idep_info[op_num].delay);

	// if this delay counter just went to zero...
	if (i->idep_info[op_num].delay == 0) {

	    //  check all ideps for zero delay
	    for (int j = 0; j < TheISA::MaxInstSrcRegs; ++j) {
		if (i->idep_info[j].delay != 0) {
		    all_zero = false;
		    break;
		}
	    }

	    if (all_zero)
		i->st_zero_time = curTick;
	}
    }
}


//
//  Determine if this instruction should be promoted
//
//  Assumes that the queue _forward_of_this_instruction_
//  is in a consistent state.
//
bool
segment_t::promotable(iq_iterator &inst)
{
    bool rv = false;
    unsigned pred_wait;
    unsigned max_wait=0;
    int max_wait_index=-1;

    //  We NEVER 'promote' from segment zero
    if (segment_number == 0)
	return false;

    //  If this instruction is ready, then hurry it on down...
    //  This shouldn't make any performance difference, but keeps us
    //  from messing around with the code below...
    if (inst->ops_ready())
	return true;

    //
    //  Figure out which operand we should be waiting for...
    //
    //  This loop calculates the predicted wait time before each head
    //  instruction should be ready to isssue. We compare these values
    //  in order to determine which chain we should be following.
    //
    //
    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	if (!inst->idep_ready[i]) {
	    if (inst->idep_info[i].chained) {
		unsigned c_num = inst->idep_info[i].follows_chain;
		unsigned head_level = (*chain_info)[c_num].head_level;

		if (head_level == 0)
		    pred_wait = 0 + inst->idep_info[i].delay;
		else
		    pred_wait = seg_queue->seg_thresholds[head_level - 1]
			+ inst->idep_info[i].delay;
	    } else {
		pred_wait = inst->idep_info[i].delay;
	    }

	    if (max_wait <= pred_wait) {
		max_wait = pred_wait;
		max_wait_index = i;
	    }
	}
    }
    assert(max_wait_index >= 0);

    IQStation::IDEP_info *idep_info = &inst->idep_info[max_wait_index];

    if (idep_info->chained) {
	unsigned head_level =
	    (*chain_info)[idep_info->follows_chain].head_level;

	//  Be sure not to promote ahead of the chain-head
	if (head_level < segment_number) {
	    if ((*chain_info)[idep_info->follows_chain].self_timed) {

		if (idep_info->delay <
		    seg_queue->seg_thresholds[segment_number - 1])
		{
		    rv = true;
		}
	    } else {
		unsigned head_thresh;
		unsigned inst_thresh
		    = seg_queue->seg_thresholds[segment_number - 1];

		if (head_level == 0)
		    head_thresh = 0;
		else
		    head_thresh = seg_queue->seg_thresholds[head_level - 1];

		// calculate this instructions distance from the head
		// (the head_level remains at 0 after the head has issued)
		unsigned dist = inst_thresh - head_thresh;

		//  If this instruction is too far behind the head...
		if (idep_info->delay < dist)
		    rv = true;
	    }
	}
    } else {
	//  Instructions that are not chained don't need to worry about
	//  a chain-head... they just use their built-in delay counter
	if (idep_info->delay < seg_queue->seg_thresholds[segment_number - 1]) {
	    rv = true;
	}
    }

    return rv;
}

void
segment_t::regStats(FullCPU *cpu, std::string &n)
{
    using namespace Stats;

    number_of_threads = cpu->number_of_threads;

    stringstream label, desc, formula;
    string c = cpu->name() + '.';

    //  Occupancy stats

    label << n << setw(2) << setfill('0') << segment_number
	  << ":cum_num_insts";
    cum_insts
	.init(cpu->number_of_threads)
	.name(label.str())
	.desc("number of insts in this segment")
	.flags(total)
	;
    label.str("");

    label << n << setw(2) << setfill('0') << segment_number
	  << ":full_cycles";
    segment_full
	.name(label.str())
	.desc("number of cycles segment was full")
	;
    label.str("");

    label << n << setw(2) << setfill('0') << segment_number
	  << ":empty_cycles";
    segment_empty
	.name(label.str())
	.desc("number of cycles segment was empty")
	;
    label.str("");

    //  Segment promotion stats
    cum_promotions.init(number_of_threads);
    if (segment_number != 0) {
	label << n << setw(2) << setfill('0') << segment_number
	      << ":cum_num_prom";
	cum_promotions
	    .name(label.str())
	    .desc("number of promotions out of this segment")
	    .flags(total)
	    ;
	label.str("");
    }

    //
    //  Use issue rate to determine residency for segment zero... rest use
    //  promotion rate
    //
    if (segment_number == 0) {
	//
	//  load-counting stats, etc.
	//
	cycles_low_ready
	    .name(n+"00:L:cycle_low_ready")
	    .desc("number of cycles segment had < 2 ready insts")
	    ;

	insts_waiting_for_loads
	    .name(n+"00:L:inst_wait_loads")
	    .desc("number of insts waiting for issued loads")
	    ;

	insts_waiting_for_unissued
	    .name(n+"00:L:inst_wait_uniss")
	    .desc("number of insts waiting for unissued insts")
	    ;

	insts_fanout_loads
	    .name(n+"00:L:inst_loads")
	    .desc("number of issued load insts waited on")
	    ;
    }

    label << n << setw(2) << setfill('0') << segment_number
	  << ":chained_cum";
    cum_chained
	.init(cpu->number_of_threads)
	.name(label.str())
	.desc("number of chained insts in this segment")
	.flags(total)
	;
    label.str("");

    cum_ops_ready.init(cpu->number_of_threads);
    if (segment_number != 0) {
	label << n << setw(2) << setfill('0') << segment_number
	      << ":cum_ops_ready";
	cum_ops_ready
	    .name(label.str())
	    .desc("number of ops-ready insts")
	    ;
	label.str("");
    }

    if (use_pushdown && segment_number < num_segments - 1 &&
	segment_number > 0)
    {
	label << n << setw(2) << setfill('0') << segment_number
	      << ":pd_inst";
	seg_queue->pushdown_count[segment_number]
	    .name(label.str())
	    .desc("number of instructions into this segment")
	    ;
	label.str("");

	label << n << setw(2) << setfill('0') << segment_number
	      << ":pd_events";
	seg_queue->pushdown_events[segment_number]
	    .name(label.str())
	    .desc("number of pushed-down events")
	    ;
	label.str("");
    }
    //  Stats for the ready-queue
    label.str("");
    label << n << setw(2) << setfill('0') << segment_number << ":RQ";
    ready_list->regStats(label.str(), cpu->number_of_threads);
}

void
segment_t::regFormulas(FullCPU *cpu, std::string &n)
{
    using namespace Stats;

    stringstream label, desc, formula;
    string c = cpu->name() + '.';

    //  Occupancy stats

    label << n << setw(2) << setfill('0') << segment_number
	  << ":occ_rate";
    occ_rate
	.name(label.str())
	.desc("Average segment occupancy")
	;
    occ_rate = cum_insts / cpu->numCycles;
    label.str("");

    label << n << setw(2) << setfill('0') << segment_number
	  << ":full_frac";
    full_frac
	.name(label.str())
	.desc("fraction of cycles where segment was full")
	;
    full_frac = 100 * segment_full / cpu->numCycles;
    label.str("");


    label << n << setw(2) << setfill('0') << segment_number
	  << ":empty_frac";
    empty_frac
	.name(label.str())
	.desc("fraction of cycles where segment was empty")
	;
    empty_frac = 100 * segment_empty / cpu->numCycles;
    label.str("");

    //  Segment promotion stats
    if (segment_number != 0) {
	label << n << setw(2) << setfill('0') << segment_number
	      << ":prom_rate";
	prom_rate
	    .name(label.str())
	    .desc("promotions per cycle from this segment")
	    ;
	prom_rate = cum_promotions / cpu->numCycles;
	label.str("");
    }

    //
    //  Use issue rate to determine residency for segment zero... rest use
    //  promotion rate
    //
    if (segment_number != 0) {
	label << n << setw(2) << setfill('0') << segment_number
	      << ":residency";
	seg_residency
	    .name(label.str())
	    .desc("segment residency (cycles)")
	    ;
	seg_residency = occ_rate / prom_rate;
	label.str("");
    } else {
	seg_residency
	    .name(n+"00:residency")
	    .desc("segment residency (cycles)")
	    ;
	seg_residency = occ_rate / cpu->issue_rate;

	rate_loads
	    .name(n+"00:L:rate_loads")
	    .desc("number of issued loads waited on per cycle")
	    ;
	rate_loads = insts_fanout_loads / cycles_low_ready;

	rate_wait_loads
	    .name(n+"00:L:rate_wait_loads")
	    .desc("number of insts waiting for issued loads per cycle")
	    ;
	rate_wait_loads = insts_waiting_for_loads / cycles_low_ready;

	rate_wait_uniss
	    .name( n+"00:L:rate_wait_uniss")
	    .desc("number of insts waiting for unissued insts per cycle")
	    ;
	rate_wait_uniss = insts_waiting_for_unissued / cycles_low_ready;

	avg_wait_load
	    .name(n+"00:L:avg_wait_load")
	    .desc("avg number of insts waiting for each load")
	    ;
	avg_wait_load = insts_waiting_for_loads / insts_fanout_loads;

	frac_low_ready
	    .name(n+"00:L:frac_low_ready")
	    .desc("fraction of all cycles with low ready rates")
	    ;
	frac_low_ready = cycles_low_ready / cpu->numCycles;
    }

    label << n << setw(2) << setfill('0') << segment_number
	  << ":chained_frac";
    chained_frac
	.name(label.str())
	.desc("fraction of insts chained")
	;
    chained_frac = 100 * cum_chained / cum_insts;
    label.str("");

    if (segment_number != 0) {
	label << n << setw(2) << setfill('0') << segment_number
	      << ":ops_ready_rate";
	ops_ready_rate
	    .name(label.str())
	    .desc("average number of ops-ready insts")
	    ;
	ops_ready_rate = cum_ops_ready / cpu->numCycles;
	label.str("");
    }

    if (use_pushdown && segment_number < num_segments - 1 &&
	segment_number > 0)
    {
	label << n << setw(2) << setfill('0') << segment_number
	      << ":pd_rate";
	pd_rate
	    .name(label.str())
	    .desc("average push-down events per cycle")
	    ;
	pd_rate = seg_queue->pushdown_events[segment_number] / cpu->numCycles;
	label.str("");

	label << n << setw(2) << setfill('0') << segment_number
	      << ":pd_inst_rate";
	pd_inst_rate
	    .name(label.str())
	    .desc("average push-down insts per event")
	    ;
	pd_inst_rate = seg_queue->pushdown_count[segment_number] / cpu->numCycles;
	label.str("");
    }
    //  Stats for the ready-queue
    label.str("");
    label << n << setw(2) << setfill('0') << segment_number << ":RQ";
    ready_list->regFormulas(label.str(), cpu->number_of_threads);
}

void
segment_t::collect_inst_stats()
{
    typedef map<ROBStation *, unsigned> LoadMap;
    LoadMap *loads = 0;
    bool collect_rob_stats = false;

    //  Only allocate this if we're going to use it
    if (segment_number == 0)
	loads = new LoadMap;

    for (iq_it_list_iterator i = head(); i.notnull(); i = i.next()) {
	unsigned thread = (*i)->thread_number();

	//
	//  Is this inst on a chain?
	//
	for (int j = 0; j < TheISA::MaxInstSrcRegs; ++j) {
	    if ((*i)->idep_info[j].chained) {
		++cum_chained[thread];
		break;
	    }
	}

	//
	//  Only segment zero does the rest of this stuff...
	//
	if (segment_number == 0) {
	    //
	    //  We only collect the following data if the ready-queue holds
	    //  less than two instructions, and the segment has more than 24
	    //  instructions:
	    //
	    //    (1)  How many instructions are waiting for an issued load?
	    //    (2)  How many instructions are waiting for each individual
	    //         load (fanout)?
	    //    (3)  How many instructions are waiting for a non-issued inst
	    //         (we hope this number is REALLY small)
	    //
	    if ((total_insts > 23) && (ready_list->count() < 3)) {
		LoadMap::iterator pos;

		collect_rob_stats = true;

		//
		//  What kind of inst is _this_ inst waiting for?
		//
		bool load_flag = false;
		bool unissued_flag = false;
		for (int j = 0; j < TheISA::MaxInstSrcRegs; ++j) {
		    if (!(*i)->idep_ready[j]) {
			ROBStation *r = (*i)->idep_ptr[j]->rob_producer;
			DynInst *p = r->inst;

			//  place issued loads into the list
			if (p->isLoad() && r->issued) {
			    load_flag = true;

			    //  Increment the counter for this ROB entry
			    pos = loads->find(r);
			    if (pos == loads->end()) {
				// entry not in map...
				// insert it with counter value=1
				loads->insert(make_pair(r,1));
			    } else {
				// increment the existing counter
				++(pos->second);
			    }
			} else if (!r->issued) {
			    unissued_flag = true;
			}
		    }
		}

		if (load_flag)
		    ++insts_waiting_for_loads;
		else if (unissued_flag)
		    ++insts_waiting_for_unissued;

	    }  //  if number of instructions...

	}  // if segment_number is 0

    }  // for each instruction

    //
    //  We do this once, and only if the criteria above have been met
    //
    if (collect_rob_stats) {
	LoadMap::iterator pos = loads->begin();
	for (; pos != loads->end(); ++pos)
	    ++insts_fanout_loads;

	++cycles_low_ready;
    }

    if (segment_number == 0)
	delete loads;
}

void
segment_t::tick_stats()
{
    //
    //  Do every-cycle stuff
    //

    //  Statistics
    for (int i = 0; i < number_of_threads; ++i)
	cum_insts[i] += insts[i];

    if (free_slots() == 0)
	++segment_full;
    else if (count() == 0)
	++segment_empty;

    collect_inst_stats();
}


void
segment_t::tick_ready_stats()
{
    ready_list->tick_stats();

    if (segment_number != 0) {
	current_ops_ready = 0;
	for (iq_it_list_iterator i=head(); i.notnull(); i=i.next()) {
	    if ((*i)->ops_ready()) {
		++current_ops_ready;

		++cum_ops_ready[(*i)->thread_number()];
	    }
	}
    } else {
	current_ops_ready = ready_list->count();
	for (int t = 0; t < number_of_threads; ++t)
	    cum_ops_ready[t] += ready_list->count(t);
    }
}

bool
RQ_Issue_Policy::goes_before(FullCPU *cpu,
			     res_list<IQStation>::iterator &first,
			     res_list<IQStation>::iterator &second,
			     bool use_thread_priorities)
{

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
    } else {
	if (first->seq < second->seq)
	    rv = true;
    }

    return rv;
}

bool
RQ_Seg_Policy::goes_before(FullCPU *cpu,
			   res_list<IQStation>::iterator &first,
			   res_list<IQStation>::iterator &second,
			   bool use_thread_priorities)
{

    if (use_dest_sort) {
	unsigned dest_seg[] = {0, 0};

	dest_seg[0] = first->dest_seg;
	dest_seg[1] = second->dest_seg;


	//  if the first's distance is larger than the second's distance,
	//  then the first should go before the second
	if (dest_seg[0] < dest_seg[1])
	    return true;

	if (dest_seg[0] > dest_seg[1])
	    return false;
    }

    //  If the destinations are the same, use age...
    if (first->seq < second->seq)
	return true;

    return false;
}

void
segment_t::enqueue(iq_iterator &p)
{
    p->rq_entry = ready_list->enqueue(p);
    p->queued = true;
}

void
segment_t::tick()
{
    //  We have to do this if we have a dynamic ordering...
    ready_list->resort();
}

//
//  Determine the "correct" segment for this instruction
//
unsigned
segment_t::proper_segment(iq_iterator &inst)
{
    unsigned my_seg = 0;

    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	unsigned new_seg = 0;

	if (!inst->idep_ready[i]) {
	    if (inst->idep_info[i].chained) {
		unsigned c = inst->idep_info[i].follows_chain;
		unsigned head_level = (*chain_info)[c].head_level;

		if (head_level == num_segments - 1) {
		    new_seg = head_level;
		} else {
		    //
		    //  Use the "cumulative" thresholds here
		    //
		    new_seg = num_segments - 1;  // default
		    unsigned inst_offset =
			seg_queue->get_thresh(head_level) +
			                            inst->idep_info[i].delay;

		    for (unsigned s = head_level; s < num_segments - 1; ++s) {
			if (inst_offset < seg_queue->get_thresh(s + 1)) {
			    new_seg = s;
			    break;
			}
		    }
		}
	    } else {
		new_seg = num_segments - 1;  // default

		for (int s = 0; s < num_segments; ++s) {
		    if (inst->idep_info[i].delay < seg_queue->get_thresh(s)) {
			new_seg = s;
			break;
		    }
		}
	    }
	}

	//  make sure we don't blow our top...
	if (new_seg >= num_segments)
	    new_seg = num_segments - 1;

	//  we're looking for the "highest" segment
	if (new_seg > my_seg)
	    my_seg = new_seg;
    }

    return my_seg;
}


void
segment_t::dump_chain(unsigned c)
{
    iq_it_list_iterator i = chains[c]->head();
    for (; i.notnull(); i = i.next())
	cout << (*i)->seq << " ";
}


void
segment_t::dump_chains()
{
    for (int i = 0; i < num_chains; ++i) {
	if (chains[i]->count()) {
	    cout << "Chain " << i << ": ";
	    dump_chain(i);
	    cout << endl;
	}
    }
}

//
//  Normal segment dump
//
void
segment_t::dump(int length)
{
    cprintf("=========================================================\n"
	    "Segment %u  (thresh=%u)\n"
	    "  Total Instructions: %u\n"
	    "  By thread: [", segment_number, threshold, queue->count());

    for (unsigned i = 0; i < seg_queue->cpu->number_of_threads; ++i) {
	cprintf("%u", insts[i]);
	if (i == seg_queue->cpu->number_of_threads - 1)
	    cout << "]\n";
	else
	    cout <<", ";
    }
    cout << "---------------------------------------------------------\n";

    iq_iterator_list *sorted_queue = new iq_iterator_list(size, true, 0);

    //  Place each instruction into the sorted_queue
    for (iq_it_list_iterator i = queue->head(); i.notnull(); i = i.next()) {

	//  find the right spot for this inst
	if (sorted_queue->empty()
	    || (*i)->seq < (*(sorted_queue->head()))->seq)
	{
	    //  Special case... place this inst up front...
	    sorted_queue->insert_after(0, *i);
	} else {
	    bool done = false;
	    iq_it_list_iterator prev_spot = 0;
	    iq_it_list_iterator j = sorted_queue->head();
	    for (; j.notnull(); j = j.next()) {
		if ((*i)->seq < (*j)->seq) {
		    sorted_queue->insert_after(prev_spot, *i);
		    done = true;
		    break;
		}
		prev_spot = j;
	    }

	    if (!done)
		sorted_queue->add_tail(*i);
	}
    }

    iq_it_list_iterator i = sorted_queue->head();
    for (; i.notnull(); i = i.next()) {
	cout << "   ";  // Cheating a little here
	(*i)->iq_segmented_dump(length);
    }

    delete sorted_queue;
}



//
//  Special one-line dump for looking at massive amounts of data
//
void
segment_t::short_dump()
{
    cout << "=========================================================\n";
    cprintf("Segment %u (%u insts) (thresh=%u)\n",
	    segment_number, queue->count(), threshold);

    iq_iterator_list *sorted_queue = new iq_iterator_list(size, true, 0);

    //  Place each instruction into the sorted_queue
    for (iq_it_list_iterator i = queue->head(); i.notnull(); i = i.next()) {
	//  find the right spot for this inst
	if (sorted_queue->empty() || (*i)->seq <
	    (*(sorted_queue->head()))->seq)
	{
	    //  Special case... place this inst up front...
	    sorted_queue->insert_after(0, *i);
	} else {
	    bool done = false;
	    iq_it_list_iterator prev_spot = 0;
	    iq_it_list_iterator j = sorted_queue->head();
	    for (; j.notnull(); j = j.next()) {
		if ((*i)->seq < (*j)->seq) {
		    sorted_queue->insert_after(prev_spot, *i);
		    done = true;
		    break;
		}
		prev_spot = j;
	    }
	    if (!done)
		sorted_queue->add_tail(*i);
	}
    }

    iq_it_list_iterator i = sorted_queue->head();
    for (; i.notnull(); i = i.next()) {
	cout << "   ";  // Cheating a little here
	(*i)->iq_segmented_short_dump();
    }

    delete sorted_queue;
}
