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
#include <sstream>

#include "base/cprintf.hh"
#include "base/statistics.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/create_vector.hh"
#include "encumbered/cpu/full/dep_link.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/iq/segmented/iq_segmented.hh"
#include "encumbered/cpu/full/iq/segmented/seg_chain.hh"
#include "encumbered/cpu/full/ls_queue.hh"
#include "encumbered/cpu/full/reg_info.hh"
#include "mem/mem_interface.hh"
#include "sim/builder.hh"
#include "sim/eventq.hh"
#include "sim/stats.hh"

using namespace std;

#define DEBUG_PROMOTION 0

#define USE_NEW_SELF_TIME_CODE 1

#define SANITY_CHECKING 1


#define use_mod_pushdown      1


#define use_mod_bypassing     1
#define bypass_slot_checking  0

//  Always begin self-timing when the chain head issues
#define load_chain_st         0
//  Begin self-timing only if Segment 0 is less than half-full
//  of not-ready instructions
#define s0_st_limit           0

#define DUMP_CHINFO 0



//==========================================================================
//
//  The "segmented" instruction queue implementation
//
//==========================================================================

//
//  The constructor
//
SegmentedIQ::SegmentedIQ(string n,
			 unsigned _num_segments,
			 unsigned _max_chain_depth,
			 unsigned _segment_size,
			 unsigned _segment_thresh,
			 bool en_pri,
			 bool _use_bypassing,
			 bool _use_pushdown,
			 bool _use_pipelined_prom) : BaseIQ(n)
{
    string s;

    //
    //  Save params
    //
    num_segments    = _num_segments;

    max_chain_depth = _max_chain_depth;
    segment_size    = _segment_size;
    segment_thresh  = _segment_thresh;

    enable_priority = en_pri;

    use_bypassing        = _use_bypassing;
    use_pushdown         = _use_pushdown;
    use_pipelined_prom   = _use_pipelined_prom;

    if (s0_st_limit && load_chain_st)
	fatal("SegmentedIQ: You really don't want to use both ld_chain_st "
	      "AND s0_st_limit");

    last_new_chain = 0;

    total_size = num_segments * segment_size;
    set_size(total_size);

    last_segment = num_segments - 1;
    deadlock_seg_flag = num_segments + 1;

    //
    //  Allocate the IQStations
    //
    //  [total_size elements, allocated, doesn't grow]
    //
    active_instructions = new iq_list(total_size, true, 0);



    //
    //  Initialize now that we have the parameters
    //
    seg_thresholds = new unsigned[num_segments];

    free_slot_info = new unsigned[num_segments];

    //  An array of pointers to segment_t
    queue_segment = new segment_ptr_t[num_segments];

    //  Initialize Deadlock
    deadlock_recovery_mode = false;
    deadlock_slot = 0;

    dedlk_promotion_count = 0;
    dedlk_issue_count = 0;

    iq_heads = 0;
    //    head_count = 0;

    //
    //  Statistics
    //
    pushdown_events = new Stats::Scalar<>[num_segments - 1];
    pushdown_count = new Stats::Scalar<>[num_segments - 1];

    total_pd_events = 0;
    total_pd_count = 0;

    deadlock_events = 0;
    deadlock_cycles = 0;
    last_deadlock = 0;

    total_ready_count = 0;
    cum_delay = 0;
    st_limit_events = 0;

    rob_chain_heads = 0;

    seg0_prom_early_count = 0;
}

void
SegmentedIQ::init(FullCPU *_cpu, unsigned dw, unsigned iw, unsigned qn)
{
    unsigned cum_thresh = 0;

    // Call the base-class init
    BaseIQ::init(_cpu, dw, iw, qn);

    num_chains = cpu->max_chains;

    total_insts = 0;
    for (int i = 0; i < cpu->number_of_threads; ++i)
	insts[i] = 0;

    for (unsigned i = 0; i < num_segments; ++i) {
	stringstream s;

	cum_thresh += segment_thresh;

	seg_thresholds[i] = cum_thresh;

	s << name() << ":" << setw(2) << setfill('0') << i << ends;
	queue_segment[i] =
	    new segment_t(this, s.str(), i, num_segments, segment_size,
			  num_chains, cum_thresh, use_pipelined_prom,
			  enable_priority);
    }
    total_thresh = cum_thresh;

    ccprintf(cerr,
	     "******************************************\n"
	     "  IQ Model : Segmented\n"
	     "  %6u Segments\n"
	     "     (size=%u, delta-thresh=%u)\n"
	     "     (%u Total slots)\n"
	     "  %6u Chains (maximum)\n"
	     "  %6u Maximum chain length\n"
	     "*****************************************\n"
	     "\n",
	     num_segments, segment_size, segment_thresh, total_size,
	     num_chains, max_chain_depth);
}


SegmentedIQ::~SegmentedIQ()
{
    delete[] pushdown_events;
    delete[] pushdown_count;

    for (int i = 0; i < num_segments; ++i)
	delete queue_segment[i];
    delete[] queue_segment;

    delete[] free_slot_info;
    delete[] seg_thresholds;

    delete active_instructions;

    delete hm_predictor;
}


//============================================================================
//
//  Shared information structure...
//
//     This structure holds information which must be shared between all
//     clusters, or between the clusters and the dispatch stage.
//
//     The buildSharedInfo() method is called for IQ[0], then the structure
//     address is passed to any remaining clusters via the setSharedInfo()
//     method
//
ClusterSharedInfo *
SegmentedIQ::buildSharedInfo()
{
    ClusterSharedInfo *rv = new ClusterSharedInfo;

    //
    //  Build the Chain Info Table
    //
    rv->ci_table = new SegChainInfoTable(num_chains, cpu->numIQueues,
					 num_segments, use_pipelined_prom);

    rv->hm_predictor = new SaturatingCounterPred(name()+":HMP", "miss", "hit",
						 12, 4, 0, 1, 13);

    rv->lr_predictor = new SaturatingCounterPred(name()+":LRP", "0", "1", 10);

    rv->total_chains = num_chains;

    //  make a copy of the info locally
    setSharedInfo(rv);

    return rv;
}

void
SegmentedIQ::setSharedInfo(ClusterSharedInfo *p)
{
    shared_info = p;

    //  for local use...
    reg_info_table = p->ri_table;
    chain_info     = static_cast<SegChainInfoTable *>(p->ci_table);

    hm_predictor = static_cast<SaturatingCounterPred *>(p->hm_predictor);
    lr_predictor = static_cast<SaturatingCounterPred *>(p->lr_predictor);

    //  tell all the segments where this info is...
    for (int i = 0; i < num_segments; ++i) {
	queue_segment[i]->reg_info_table = reg_info_table;
	queue_segment[i]->chain_info = chain_info;
    }
}


//============================================================================


//
//  Add an instruction to the queue:
//
//  All instructions "live" in the "active_instructions" list. We pass
//  iterators to these entries around instead of moving the data. This
//  also makes it possible to walk dependence chains at writeback time
//
SegmentedIQ::iq_iterator
SegmentedIQ::add_impl(DynInst *inst, InstSeqNum seq, ROBStation *rob,
		      RegInfoElement *ri, NewChainInfo *new_chain)
{
    IQStation rs;  // We'll fill in these fields then copy the object

    unsigned follows_chain = 1000;
    unsigned n_chains = 0;

    //
    //  If we're in the process of recovering from a deadlock condition,
    //  then disable instruction dispatch
    //
    if (deadlock_recovery_mode)
	return 0;

    rs.inst		= inst;
    rs.in_LSQ		= false;
    rs.ea_comp		= inst->isMemRef();
    rs.seq		= seq;   // The dispatch sequence, not fetch sequence
    rs.queued		= false;
    rs.squashed		= false;
    //rs.blocked	= false;
    rs.dispatch_timestamp = curTick;
    rs.lsq_entry	= 0;  // may be changed by dispatch()
    rs.tag		= inst->fetch_seq;
    rs.rob_entry	= rob;

    rob->head_of_chain = false;


    assert(!new_chain->out_of_chains);


    //  Now that we're sure we'll add this instruction...
    ++total_insts;
    ++insts[inst->thread_number];

    //  Insert this info into instruction record
    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i)
	rs.idep_info[i] = new_chain->idep_info[i];

    rs.head_of_chain      = new_chain->head_of_chain;
    rs.head_chain         = new_chain->head_chain;
    rs.pred_last_op_index = new_chain->pred_last_op_index;
    rs.lr_prediction      = new_chain->lr_prediction;

    //
    //  Add this instruction to the active list so we have someplace to link
    //  ideps, etc
    //
    iq_iterator p = active_instructions->add_tail(rs);

    p->hm_prediction    = new_chain->hm_prediction;
    rob->hm_prediction  = p->hm_prediction;

    //
    //  Determine which segment this instruction will be placed in
    //
    //  Result is determined by bypassing mode, etc.
    //
    unsigned destination = choose_dest_segment(p);

    //
    //  Now, link into producing instructions and predict issue/writeback
    //
    unsigned op_lat = cpu->FUPools[0]->getLatency(rs.opClass());
    Tick   max_wb = 0;
    Tick   max_chained_op_rdy_time = 0;
    unsigned max_depth = 0,
	     max_delay = 0;
    Tick   max_op_ready_time = 0;


    //  Adjust the instruction latency if this is the EA-Comp portion of
    //  a LOAD instruciton
    if (rs.inst->isLoad()) {
	op_lat += cache_hit_latency;
    }

    //
    //  This hunk of code not only links the input deps, but also returns
    //  the expected cycle that that dep will write-back. We use these
    //  values to determine the latest WB time. The latest WB time will
    //  be used to determine the earliest Issue cycle, and all other time
    //  calculations follow from there.
    //
    StaticInstPtr<TheISA> si = rs.inst->staticInst;
    StaticInstPtr<TheISA> eff_si = rs.ea_comp ? si->eaCompInst() : si;

    p->num_ideps = 0;

    for (int i = 0; i < eff_si->numSrcRegs(); ++i) {

	Tick this_wb = link_idep(p, eff_si->srcRegIdx(i));

	//  Find the latest predicted ops-ready time from all inputs
	//  (we'll over-write this later with the chained value, if
	//   we decide that we're chained)
	if (this_wb > max_op_ready_time)
	    max_op_ready_time = this_wb;

	//  If this op is chained & not-ready, we have to look at it
	if (!p->idep_ready[i] && new_chain->idep_info[i].chained) {
	    ++n_chains;
	    if (this_wb > max_chained_op_rdy_time) {
		follows_chain = new_chain->idep_info[i].follows_chain;

		max_depth = new_chain->idep_info[i].chain_depth;
		max_delay = new_chain->idep_info[i].delay;

		max_chained_op_rdy_time = this_wb;
	    }
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

    //  If we have ANY chains, we want to use the chained time
    if (n_chains)
	max_op_ready_time = max_chained_op_rdy_time;

    //  Now, calculate the predicted issue cycle...
    //  Note that we can't issue until the cycle AFTER the instruction arrives
    //  in segment zero.
    if (max_wb < curTick + destination + 1)
	p->pred_issue_cycle = curTick + destination + 1;
    else
	p->pred_issue_cycle = max_wb;

    //  ... and store it
    rob->pred_issue_cycle = p->pred_issue_cycle;

    //  Use that to calculate the predicted WB cycle
    rob->pred_wb_cycle = p->pred_issue_cycle + op_lat;

    p->pred_ready_time = max_op_ready_time;


    //
    //  Decide whether we want the output of this inst to be chained from
    //  an incoming chain... Wierdness due to the possiblity of following
    //  multiple chains (this forces an instruction following multiple
    //  chains to be a "head")...
    //
    bool chained = false;
    if (n_chains > 1) {
	//  We'd better be the head of a new chain!
    } else if (n_chains == 1) {
	chained = true;
    } else {
	// Operands are self-timed (or ready)
	// ==> The result register should be marked as self-timed
	//     (ie. not chained)
    }



    inst_depth_dist.sample(max_depth);
    inst_depth_lat_dist.sample(max_delay);

    //
    //  "pred_wb_time" is an prediction of when this instructions
    //  RESULT VALUES will be ready... We put this value into the
    //  register-info table.
    //
    Tick pred_wb_time;
    if (p->ops_ready()) {
	p->ready_timestamp = curTick;

	delay_at_ops_rdy_dist.sample(0);
    }

    pred_wb_time = rob->pred_wb_cycle;

    //
    //  If this instruction is the head of a chain
    //
    if (p->head_of_chain) {

	++iq_heads;    // decremented when head leaves the IQ/LSQ

	//  Make sure the creator seq number matches correctly...
	if (!rs.ea_comp) {
	    (*chain_info)[p->head_chain].creator = seq;
	} else {
	    // the LSQ portion of this (must be a store) will generate value
	    (*chain_info)[p->head_chain].creator = seq + 1;
	}
	(*chain_info)[p->head_chain].created_ts = curTick;
	(*chain_info)[p->head_chain].head_level = destination;

	//
	//  The ROB element is what actually does the writeback, since the
	//  instruction will have been removed from the queue at issue...
	//
	rob->head_of_chain = true;
	rob->head_chain = p->head_chain;


	//
	//  Set the chain depth to one
	//
	(*chain_info)[p->head_chain].chain_depth = 0;
	max_depth = 0;  // Put this plus one into the reg_info struct
    }



    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	if (new_chain->idep_info[i].chained) {
	    //  Let's see just how long this chain is...
	    //  if we're using a register created near the head of the chain,
	    //  our depth may not be the deepest of all chained instructions
	    SegChainInfoEntry &info =
		(*chain_info)[new_chain->idep_info[i].follows_chain];

	    if (max_depth >= info.chain_depth)
		info.chain_depth = max_depth + 1;
	}
    }

#if 0   // we don't actually set hmp_func anywhere...
    //
    //  HMP:  Add latency to the result of this load if we predict a load miss
    //
    if (use_hm_predictor && hmp_func == HMP_LATENCY && inst->isLoad()) {
	//  If we predict this to be a miss
	if (p->hm_prediction == MA_CACHE_MISS)
	    pred_wb_time += MISS_ADDITIONAL_LATENCY;
    }

    //
    //  Special handling if we're doing BOTH...
    //  (prediction has already been made and stored in ROB... don't
    //   want to make _another_ one -- first in choose_chain() )
    //
    if (use_hm_predictor && hmp_func == HMP_BOTH && inst->isLoad()) {
	//  If we predict this to be a miss
	if (p->hm_prediction == MA_CACHE_MISS)
	    pred_wb_time += MISS_ADDITIONAL_LATENCY;
    }
#endif

#if DUMP_CHINFO
    cout << "@ " << curTick << ": #" << seq << " head of C#";
    if (rs.head_of_chain)
	cout << rs.head_chain;
    else
	cout << "--";

    if (chained)
	cout << " follows C#" << follows_chain << " w/ latency "
	     << max_delay + op_lat;
    else
	cout << " self-timed";
    cout << endl;
#endif


    //
    //  Add the instruciton to the queue segment AFTER all of it's fields
    //  have been filled in.
    //
    //  The following IQStation entries are initialized during
    //  the call to segment_t::add() :
    //    (1) segment_number
    //    (2) chain_entry
    //    (3) head_entry
    //    (4) rq_entry
    //
    assert( queue_segment[destination]->add(p).notnull() );
    --free_slot_info[destination];

    //
    //  Update the register-info structure so that subsequent insts
    //  get the right chain info for regs produced by this inst.
    //
    for (unsigned i = 0; i < inst->numDestRegs(); ++i) {
	//  The ready time is based on a timestamp...
	ri[i].setPredReady(pred_wb_time);

	if (p->head_of_chain) {
	    ri[i].setChain(p->head_chain, max_depth + 1);
	    ri[i].setLatency(0 + op_lat);
	}  else if (chained) {
	    //  this instruction is following one (or more) chains
	    ri[i].setChain(follows_chain, max_depth + 1);
	    ri[i].setLatency(max_delay + op_lat);
	} else {
	    // this instruction is self-timed... NO CHAIN
	    ri[i].setLatency(max_delay + op_lat);
	}
    }

    return p;
}


//
//  This function gets called when an instruction is dispatched to a
//  different cluster
//
void
SegmentedIQ::inform_dispatch(iq_iterator i) {

}


void
SegmentedIQ::registerLSQ(iq_iterator &p, BaseIQ::iterator &lsq)
{
    p->lsq_entry = lsq;

    lsq->hm_prediction = p->hm_prediction;

    lsq->pred_issue_cycle = p->pred_issue_cycle;

    //
    //  We need this to clean up some squashed instructions, and
    //  for stores...
    //
    if (p->head_of_chain) {
	lsq->head_of_chain = true;
	lsq->head_chain = p->head_chain;
    }
}


unsigned SegmentedIQ::choose_dest_segment(iq_iterator &p)
{
    unsigned destination;

    unsigned desired_pos = queue_segment[0]->proper_segment(p);
    p->dest_seg = desired_pos;


    //
    //  This is easy if we're not doing bypassing
    //
    if (!use_bypassing)
	return num_segments - 1;

    //
    //  Search for the lowest segment...
    //
    int d;
    for (d = num_segments - 1; d >= 0; --d) {
	//
	//  We break out early if we are looking at the "correct" segment
	//  for this instruction
	//
	if (!use_mod_bypassing && d == desired_pos) {
	    unsigned slots;

	    //  Dispatch stage is close enough to get the real number
	    //  for the top segment
	    if (d == last_segment)
		slots = queue_segment[d]->free_slots();
	    else
		slots = free_slot_info[d];

	    //  We can do this w/out checking which segment we're looking at
	    //  because we're guaranteed that the TOP segment has a free
	    //  slot in it...
	    if (slots == 0)
		++d;

	    break;
	}

	//
	//  keep going down until we find a non-empty segment
	//
	if (!queue_segment[d]->empty()) {
	    unsigned slots;

	    //  Dispatch stage is close enough to get the real number
	    //  for the top segment
	    if (d == last_segment)
		slots = queue_segment[d]->free_slots();
	    else
		slots = free_slot_info[d];

	    //
	    //  If we're checking for a minimun number of free slots...
	    //      OR
	    //  there are no open slots...
	    //
	    if (bypass_slot_checking && slots < cpu->issue_width ||
		slots == 0)
	    {
		//  We need to try to go back one segment...
		if (d < num_segments - 1)
		    ++d;
	    }

	    break;
	}
    }

    //  Just in case...
    if (d < 0)
	d = 0;

    if (d > last_segment)
	d = last_segment;

    destination = d;

    //
    //  Statistics...
    //
    if (destination != num_segments - 1) {
	++bypassed_insts[p->thread_number()];

	bypassed_segments[p->thread_number()] +=
	    num_segments - destination - 1;
    }

    return destination;
}




//
//  remove an instruction from the queue...
//  Cleans up everything except for the output registers
//
SegmentedIQ::iq_iterator
SegmentedIQ::internal_remove(SegmentedIQ::iq_iterator &e)
{
    iq_iterator n = e.next();

    if (e.notnull()) {
	unsigned seg = e->segment_number;

	--total_insts;
	--insts[e->thread_number()];

	if (e->segment_number != deadlock_seg_flag)
	    queue_segment[seg]->remove(e);
	else
	    deadlock_slot = 0;

	//  Make sure that the tag is invalidated!
	e->tag++;

	for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	    if (e->idep_ptr[i]) {
		delete e->idep_ptr[i];   // delete the DepLink
		e->idep_ptr[i] = 0;
	    }
	}

	//  Release the actual IQStation
	active_instructions->remove(e);
    }

    return n;
}


//
//  Every cycle:
//     (1) Promote instuctions that need it
//     (2) Check to see which instructions will follow those just promoted
//
void
SegmentedIQ::tick_model_stats()
{
    //
    //  Statistics
    //
    for (int i = 0; i < cpu->number_of_threads; ++i) {
	unsigned c = chain_info->chainsInUseThread(i);
	chains_cum[i] += c;
	if (c > chains_peak[i].value())
	    chains_peak[i] = c;
    }

    for (int i = 0; i < num_segments; ++i) {
	//  do segments stats
	queue_segment[i]->tick_stats();
    }

    //  The number of chain heads in the ROB ONLY is the difference
    //  between number of chain heads & the number of chain heads still
    //  in the IQ/LSQ
    rob_chain_heads += (cpu->chain_heads_in_rob - iq_heads);
}


void
SegmentedIQ::tick_ready_stats()
{
    for (int i = 0; i < num_segments; ++i) {
	//  do segments stats
	queue_segment[i]->tick_ready_stats();
	total_ready_count += queue_segment[i]->ops_ready_count();
    }
}


unsigned
SegmentedIQ::sanity_check()
{
    unsigned rv = 0;

    if (!chain_info->sanityCheckOK())
	rv |= 0x01;

    for (iterator i = active_instructions->head(); i != 0; i = i.next()) {
	unsigned seq = i->seq;

	for (int j = 0; j < TheISA::MaxInstSrcRegs; ++j) {
	    if (i->idep_info[j].chained) {
		unsigned c = i->idep_info[j].follows_chain;
		if ((*chain_info)[c].free) {
		    rv |= 0x02;
		    return rv;
		    rv = seq;
		    seq = c;
		}
	    }
	}
    }

    return rv;
}


//============================================================================
//
//
//
void
SegmentedIQ::tick()
{
#if SANITY_CHECKING
    assert(!sanity_check());
#endif


    //----------------------------------------------------------

    //
    //  Ready Queue re-sorting, and debugging stuff
    //
    for (int i = 0; i < num_segments; ++i)
	queue_segment[i]->tick();

    //----------------------------------------------------------

    //
    //  Deadlock Recovery...
    //
    if (deadlock_slot.notnull()) {
	// Put the instruction from the deadlock_slot into
	// the last segment (if there's room)
	if (!queue_segment[last_segment]->full()) {
	    assert(queue_segment[last_segment]->add(deadlock_slot).notnull());
	    deadlock_slot = 0;
	}
    }


    //----------------------------------------------------------


    for (int i = 0; i < num_segments; ++i)
	free_slot_info[i] = queue_segment[i]->free_slots();

    //
    //  Promote those instructions marked ready-to-promote during
    //  the previous cycle.
    //
    //  The "head_promoted" signal is asserted as appropriate
    //
    for (unsigned i = 0; i < num_segments; ++i) {
	//  Promote instructions from this segment
	if (i != 0)
	    promote_insts(i);
    }

    //
    //  Shift the bits for pipelined promotion
    //
    chain_info->tick();

    //
    //  Let each segment decide which instructions should be promoted
    //
    for (unsigned i = 0; i < num_segments; ++i)
	queue_segment[i]->check_promotable();

    //
    //  Register timers may decremnt also
    //
    for (unsigned t = 0; t < cpu->number_of_threads; ++t) {
	for (unsigned r = 0; r < TotalNumRegs; ++r) {
	    RegInfoElement &ri = (*reg_info_table)[t][r];

	    if (ri.isChained() == false ||
		(*chain_info)[ri.chainNum()].self_timed)
		ri.tickLatency();
	}
    }


    //----------------------------------------------------------

    // ONLY CALL THIS ONCE!!!! (It resets counters)
    deadlock_recovery_mode = check_deadlock();

    if (deadlock_recovery_mode) {
	++deadlock_cycles;

	//  Count a new event only if last cycle wasn't a deadlock cycle
	if (last_deadlock != curTick - 1)
	    ++deadlock_events;

	last_deadlock = curTick;


	// Pull the youngest instruction from Segment 0 if the
	// deadlock_slot is open
	if (deadlock_slot.isnull()) {
	    deadlock_slot = queue_segment[0]->youngest();
	    if (deadlock_slot.notnull()) {
		queue_segment[0]->remove(deadlock_slot);
		deadlock_slot->segment_number = deadlock_seg_flag;
	    }
	}

	//
	//  Force the oldest instruction in the segment onto the
	//  ready list
	//
	for (int seg = 1; seg < num_segments; ++seg) {
	    iq_iterator i = queue_segment[seg]->oldest();

	    if (i.notnull() && !i->queued)
		queue_segment[seg]->enqueue(i);
	}
    }
}


//
//  Prompote instructions from the specified queue segment to the next
//
void
SegmentedIQ::promote_insts(unsigned src_seg)
{
    unsigned num_to_promote;

    if (src_seg == 0)
	panic("IQ-Segmented: Don't promote from seg 0");

    //  Figure out how many to promote
    num_to_promote = queue_segment[src_seg]->ready_count();

    if (num_to_promote > free_slot_info[src_seg - 1])
	num_to_promote = free_slot_info[src_seg - 1];

    if (num_to_promote > cpu->issue_width)
	num_to_promote = cpu->issue_width;

    segment_t::rq_iterator p = queue_segment[src_seg]->issuable_list();
    for (int i = 0; i < num_to_promote; ++i) {
	segment_t::rq_iterator n = p.next();

	++dedlk_promotion_count;

	// We must remove the entry FIRST since add() overwrites some
	// of the tracking information
	queue_segment[src_seg]->remove(*p);
	assert(queue_segment[src_seg - 1]->add(*p).notnull());
	--free_slot_info[src_seg - 1];


	//  If we are promoting into Segment 0
	if (src_seg == 1) {
	    unsigned max_delay = 0;

	    (*p)->seg0_entry_time = curTick;

	    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i)
		if (!(*p)->idep_ready[i] && (*p)->idep_info[i].chained)
		    if (max_delay < (*p)->idep_info[i].delay)
			max_delay = (*p)->idep_info[i].delay;

	    if (max_delay >= segment_thresh)
		seg0_prom_early_count++;
	}

	//
	//  If we are promoting the head of a chain, notify the chain
	//
	if ((*p)->head_of_chain) {

	    //
	    //  Actually, if this instruction is going through the queue again
	    //  as a result of the deadlock-recovery mechanism, we don't want
	    //  to signal the promotion.
	    //
	    SegChainInfoEntry &info = (*chain_info)[(*p)->head_chain];
	    if (info.head_level == src_seg) {
		info.head_promoted = true;
		info.head_level = src_seg - 1;
	    }

#if DEBUG_PROMOTION
	    cprintf("Promoted seq %d (head of chain %d) to seg %d\n",
		    (*p)->seq, (*p)->head_chain, src_seg - 1);
#endif
	}
#if DEBUG_PROMOTION
	else {
	    cprintf("Promoted seq %d (follows chain %d) to seg %d\n",
		    (*p)->seq, (*p)->follows_chain, src_seg - 1);
	}
#endif

	// we're counting promotions OUT of segments
	++queue_segment[src_seg]->cum_promotions[(*p)->thread_number()];

	//  Point to next
	p = n;
    }

    if (use_pushdown) {
	//
	//  Push-Down to Next Segment
	//

	//  not if no b/w or this is segment 1
	if (num_to_promote == cpu->issue_width || src_seg < 2)
	    return;

	//
	// Pushdown criteria:
	//    - This segment has less than a full width of free slots
	//      (make enough room for a full width)
	//    - The next segment must have a half-width of free slots
	//      after we finish pushing down
	//
	unsigned f_slots = free_slot_info[src_seg];

	if (f_slots  < cpu->issue_width) {

	    unsigned f_slots_1 = free_slot_info[src_seg - 1];

	    if (f_slots_1 > cpu->issue_width / 2) {

		//  Calculate our paramaters
		unsigned want_to_push = cpu->issue_width - f_slots;
		unsigned room_avail   = f_slots_1 - cpu->issue_width / 2;
		unsigned bw_avail = cpu->issue_width - num_to_promote;

		//  Don't over-fill the next segment
		if (room_avail < want_to_push)
		    want_to_push = room_avail;

		//  We have a fixed amount of bandwidth
		if (bw_avail < want_to_push)
		    want_to_push = bw_avail;

		++pushdown_events[src_seg - 1];
                ++total_pd_events;
		pushdown_count[src_seg - 1] += want_to_push;
                total_pd_count += want_to_push + 1;

		for (int i = 0; i < want_to_push; ++i) {
		    iq_iterator p = queue_segment[src_seg]->oldest();

		    if (use_mod_pushdown) {
			iq_iterator q = queue_segment[src_seg]->lowest_dest();
			if (q->dest_seg < p->dest_seg)
			    p = q;
		    }

		    ++dedlk_promotion_count;

		    queue_segment[src_seg]->remove(p);

		    //  zero return value indicates add error
		    assert(queue_segment[src_seg - 1]->add(p).notnull());
		    --free_slot_info[src_seg - 1];

		    //
		    //  If we are promoting the head of a chain,
		    //  notify the chain
		    //
		    if (p->head_of_chain) {
			//
			//  Actually, if this instruction is going
			//  through the queue again as a result of the
			//  deadlock-recovery mechanism, we don't want
			//  to signal the promotion.
			//
			SegChainInfoEntry &info = (*chain_info)[p->head_chain];
			if (info.head_level == src_seg) {
			    info.head_promoted = true;
			    info.head_level = src_seg - 1;
			}

#if DEBUG_PROMOTION
			cprintf("Promoted seq %d (head of chain %d) "
				"to seg %d\n",
				p->seq, p->head_chain, src_seg - 1);
		    }
		    else {
			cprintf("Promoted seq %d (follows chain %d) "
				"to seg %d\n",
				p->seq, p->follows_chain, src_seg - 1);
#endif
		    }

		    // we're counting promotions OUT of segments
		    ++queue_segment[src_seg]->
			cum_promotions[p->thread_number()];
		}
	    }
	}
    }
}


bool
SegmentedIQ::release_register(unsigned t, unsigned r, InstSeqNum rob_seq)
{

    //  If this instruction is the last writer or this register,
    //  Mark the register so that future consumers don't chain
    //  off of it.
    if ((*reg_info_table)[t][r].producer() &&
	(*reg_info_table)[t][r].producer()->seq == rob_seq)
    {
	(*reg_info_table)[t][r].clear();
	return true;
    }

    return false;
}



//
//  Handle the removal of the specified instruction from both
//  the instruction queue and the ready queue
//
SegmentedIQ::rq_iterator
SegmentedIQ::issue_impl(rq_iterator &p)
{
    rq_iterator next = p.next();

    ++dedlk_issue_count;

    if (p.notnull()) {
	pred_issue_error_dist.sample(curTick-(*p)->pred_issue_cycle);

	ready_error_dist.sample((*p)->pred_ready_time - (*p)->ready_timestamp);

	//  If we haven't actually counted down to zero yet...
	if ((*p)->st_zero_time == 0)
	    (*p)->st_zero_time = curTick + (*p)->max_delay() - 1;

	int error = (*p)->seg0_entry_time -
	    max((*p)->dispatch_timestamp + num_segments - 1,
		(*p)->st_zero_time);
	seg0_entry_error_dist.sample(error);

	//
	//  If head of chain, self-time chain
	//
	if ((*p)->head_of_chain) {

	    // chain head has left IQ
	    --iq_heads;

	    //  if this is an EA-comp instruction, then we really
	    //  don't want to start self-timing yet... wait until
	    //  the memory op issues from the LSQ
	    if (!(*p)->ea_comp) {
#if USE_NEW_SELF_TIME_CODE
		ROBStation *rob = (*p)->rob_entry;
		if (rob->seq == (*chain_info)[rob->head_chain].creator)
		    (*chain_info)[rob->head_chain].self_timed = true;
#else
		for (int seg = 0; seg < num_segments; ++seg)
		    queue_segment[seg]->self_time((*p)->rob_entry);
#endif
	    }
	}


	//  Remove IQ element
	internal_remove(*p);
    }

    return next;
}


//
//  This function gets called when an instruction issues form a different IQ
//  or the LSQ
//
void
SegmentedIQ::inform_issue(iq_iterator i)
{
    ++dedlk_promotion_count;

    pred_issue_error_dist.sample(curTick - i->pred_issue_cycle);

    if (i->head_of_chain) {

	// head has left the IQ
	//	--iq_heads;

	bool do_st = false;

	if (load_chain_st) {
	    //  always self-time when the head issues
	    do_st = true;
	} else if (s0_st_limit) {
	    //  start self-timing if there aren't too many not-ready
	    //  instructions in segment zero
	    if ((queue_segment[0]->count() - queue_segment[0]->ready_count())
		< (segment_size/2))
	    {
		do_st = true;
	    } else {
		++st_limit_events;
	    }
	}
	//    else if (i->rob_entry->hm_prediction == MA_CACHE_MISS) {
	//          ???????????
       	else if (i->rob_entry->hm_prediction == MA_HIT) {
	    do_st = true;
	}

	if (do_st) {
#if USE_NEW_SELF_TIME_CODE
	    ROBStation *rob = i->rob_entry;
	    if (rob->seq == (*chain_info)[rob->head_chain].creator)
		(*chain_info)[rob->head_chain].self_timed = true;
#else
	    //  Start self-timing the chained instructions
	    for (int seg = 0; seg < num_segments; ++seg)
		queue_segment[seg]->self_time(i->rob_entry);
#endif
	}
    }
}


//
//  This function is called when an instruction from another IQ or from
//  the LSQ gets squashed
//
void
SegmentedIQ::inform_squash(ROBStation *rob)
{
    //
    //  If a chain-head that is no longer in the IQ is being
    //  squashed, release the chain now, since there is no IQ
    //  entry to do it via squash().
    //

    //  Release any registers that this insturcion produced
    for (int i = 0; i < rob->num_outputs; ++i)
	release_register(rob->thread_number, rob->onames[i], rob->seq);

    if (rob->head_of_chain && rob->iq_entry.isnull())
	release_chain(rob);
}


//
//  Remove an instruction from the queue
//
//  (frees output registers first)
//
SegmentedIQ::iq_iterator
SegmentedIQ::squash(iq_iterator &e)
{
    iq_iterator n = e.next();

    //  Release any registers that this insturcion produced
    for (int i = 0; i < e->rob_entry->num_outputs; ++i)
	release_register(e->thread_number(),
			 e->rob_entry->onames[i], e->rob_entry->seq);

    //  We need to free the chain here...
    //  don't bother marking the chained instructions as self-timed
    //  since they will be squashed also.
    if (e->head_of_chain) {
	release_chain(e->rob_entry);
	--iq_heads;
    }

    //  Remove IQ element
    internal_remove(e);

    return n;
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
//  NOTE: EA-Comp operations do not pass through here!
//
//  NOTE: IF YOU ARE CHANGING *THIS* ROUTINE, YOU PROBABLY WANT TO
//        CHANGE "ls_queue::broadcast_result" ALSO!
//
unsigned
SegmentedIQ::writeback(ROBStation *rob, unsigned queue_num)
{
    DepLink *olink, *olink_next;
    unsigned consumers = 0;


    //
    //  If this instruction was the head of a chain:
    //    (1)  Mark the chain as self-timed
    //    (2)  Free the chain
    //
    if (rob->head_of_chain) {
	unsigned chain = rob->head_chain;

	//
	//  Collect stats
	//
	max_chain_length_dist.sample((*chain_info)[chain].chain_depth);

	//
	//  Free the chain
	//
	//  Moves chained instructions from the chain onto the
	//  self_timed_list (causeing stopped insts to restart)
	//
	release_chain(rob);
    }


    //
    //  Now go through output dependency lists and mark operands ready
    //
    for (int i = 0; i < rob->num_outputs; ++i) {

	//
	//  Try to release the register that this instruction produced
	//
	release_register(rob->thread_number, rob->onames[i], rob->seq);


	//  If there are no links...
	if (rob->odep_list[i][queue_num] == 0)
	    return 0;


	for (olink = rob->odep_list[i][queue_num]; olink; olink = olink_next) {

	    //  grab the next link... we may delete this one
	    olink_next = olink->next();

	    if (olink->valid()) {
		res_list<IQStation>::iterator q_entry = olink->consumer();

		//  This is the IQ... ignore LSQ entries!
		if (q_entry->in_LSQ)
		    continue;

		++consumers;

		//  The consuming operation should still be on a chain...
		// assert(!q_entry->idep_info[olink->idep_num].chained);

		if (q_entry->idep_ready[olink->idep_num])
		    panic("output dependence already satisfied");

		// input is now ready
		q_entry->idep_ready[olink->idep_num] = true;
		q_entry->idep_ptr[olink->idep_num] = 0;

		// are all the input operands ready?
		if (q_entry->ops_ready()) {

		    //  We should not become ready before we time-out
		    //	    assert(q_entry->delay == 0);

		    q_entry->ready_timestamp = curTick;

		    //  If we're ready now, and we made a prediction about
		    //  which op would be ready last...
		    if (q_entry->pred_last_op_index != -1) {

			// If we guessed correctly...
			if (olink->idep_num == q_entry->pred_last_op_index) {
			    ++correct_last_op[q_entry->thread_number()];
			} else {
			    wrong_choice_dist[q_entry->thread_number()]
				.sample(curTick - q_entry->first_op_ready);
			    disp_rdy_delay_dist[q_entry->thread_number()]
				.sample(curTick - q_entry->dispatch_timestamp);
			}

			//  if we used the Left/Right predictor AND
			//  we made a prediction for this instruction
			if (cpu->use_lr_predictor &&
			    q_entry->lr_prediction != -1)
			{
			    lr_predictor->record(q_entry->inst->PC >> 2,
						 olink->idep_num,
						 q_entry->lr_prediction);
			}
		    }

		    //
		    //  We queue this instruction to move on ONLY if
		    //  this is segment zero
		    //
		    if (q_entry->segment_number == 0)
			queue_segment[0]->enqueue(q_entry);

		    //
		    //  add the largest delay value to the distribution
		    //
		    unsigned max_delay=0;
		    for (int j = 0; j < TheISA::MaxInstSrcRegs; ++j)
			if (max_delay < q_entry->idep_info[j].delay)
			    max_delay = q_entry->idep_info[j].delay;

		    delay_at_ops_rdy_dist.sample(max_delay);
		    cum_delay += max_delay;
		} else {
		    q_entry->first_op_ready = curTick;
		}
	    }

	    //  Free link elements that belong to this queue
	    delete olink;
	}
    }

    return consumers;
}




bool
SegmentedIQ::check_deadlock()
{

    //
    //  Can't have a deadlock if the IQ is empty...
    //
    if (count() == 0)
	return false;

    //  Queue-specific conditions:
    //
    //   (1) No promotions or issues in the last cycle
    if (dedlk_promotion_count || dedlk_issue_count) {

	dedlk_promotion_count = 0;
	dedlk_issue_count = 0;
	return false;
    }

    //
    //  The LSQ must have no ready instructions
    //
    if (cpu->LSQ->ready_count())
	return false;

    //
    //  The writeback-queue must be empty
    //
    if (!cpu->writebackEventQueue.empty())
	return false;

    //
    //  There must be at least one segment between the first and last segments
    //  that is full, and there are no ready instructions
    //
    bool found = false;
    for (unsigned s = 0; s < num_segments - 1; ++s) {
	if (queue_segment[s]->free_slots() == 0 &&
	    queue_segment[s]->ready_count() == 0) {

	    found = true;
	    break;
	}
    }

    //
    //  The DL-1 cache must not have any outstanding misses
    //
    if (cpu->dcacheInterface->outstandingMisses())
	return false;

    if (!found)
	return false;

    return true;
}

void
SegmentedIQ::regModelStats(unsigned num_threads)
{
    using namespace Stats;

    //    string n = cpu->name() + ".IQ:";
    string n = name() + ":";
    string c = cpu->name() + ".";

    //
    //  Stall statistics
    //

    rob_chain_heads
	.name(n+"rob_chain_heads")
	.desc("Cum count of chain heads in ROB")
	;
    chains_cum
	.init(num_threads)
	.name(n+"chains_cum")
	.desc("chains in use")
	.flags(total)
	;
    chains_peak
	.init(num_threads)
	.name(n+"chains_peak")
	.desc("maximum number of chains in use")
	.flags(total)
	;

    deadlock_events
	.name(n+"deadlock_events")
	.desc("Number of IQ deadlock events")
	;

    deadlock_cycles
	.name(n+"deadlock_cycles")
	.desc("Number of cycles the IQ was deadlocked")
	;

    bypassed_insts.init(num_threads);
    bypassed_segments.init(num_threads);
    if (use_bypassing)
    {
	bypassed_insts
	    .name(n+"bypassed_insts")
	    .desc("number of insts that used bypassing")
	    .flags(total)
	    ;

	bypassed_segments
	    .name(n+"bypassed_segs")
	    .desc("number of segments that were bypassed")
	    .flags(total)
	    ;
    }

    if (s0_st_limit)
    {
	st_limit_events
	    .name(n+"st_limit_events")
	    .desc("number of S/T limit events")
	    ;
    }

    if (use_pushdown)
    {
	total_pd_count
	    .name(n+"pushdown_count")
	    .desc("number of instructions pushed into this segment")
	    ;

	total_pd_events
	    .name(n+"pushdown_events")
	    .desc("number of pushed-down events")
	    ;
    }

    //
    //  Ready instruction stats
    //
    total_ready_count
	.name(n+"ops_rdy_insts")
	.desc("Cumulative count of ops-ready insts")
	;

    //  This should really always be zero!
    seg0_prom_early_count
	.name(n+"00:promoted_early")
	.desc("instructions promoted to Seg-0 early")
	;

    //
    //  Chain prediction stats
    //
    correct_last_op
	.init(num_threads)
	.name(n+"correct_op_pred")
	.desc("number of 2-op insts w/ correct pred")
	.flags(total)
	;

    wrong_choice_dist
	.init(num_threads,0,19,1)
	.name(n+"wrong_choice_dist")
	.desc("how many cycles we were off when we choose wrong")
	.flags(cdf)
	;

    //
    //  Chain-length stats...
    //
    max_chain_length_dist
	.init(0,max_chain_depth,1)
	.name(n+"max_chain_length_dist")
	.desc("Maximum chain lengths")
	.flags(cdf)
	;

    inst_depth_dist
	.init(0,max_chain_depth,1)
	.name(n+"inst_depth_dist")
	.desc("Instruction depth (levels)")
	.flags(cdf)
	;

    inst_depth_lat_dist
	.init(0,total_thresh,1)
	.name(n+"inst_depth_lat_dist")
	.desc("Instruction depth (cycles)")
	.flags(cdf)
	;

    //
    //  These stats tell us how effective this queue is
    //        ready_error_dist = the time between ops actually ready &
    //                           when we *think* they should be ready
    //                           [rdy_time - pred_rdy_time]
    //
    disp_rdy_delay_dist
	.init(num_threads,0,19,1)
	.name(n+"disp_rdy_delay_dist")
	.desc("Delay between inst dispatch and ready")
	.flags(cdf)
	;

    ready_error_dist
	.init(-40,39,1)
	.name(n+"ready_error_dist")
	.desc("Lateness of predicted ready time (cycles)")
	.flags(cdf)
	;

    delay_at_ops_rdy_dist
	.init(0,19,1)
	.name(n+"delay_at_ops_rdy")
	.desc("value of delay when last op becomes ready")
	.flags(cdf)
	;

    cum_delay
	.name(n+"delay_at_ops_rdy_cum")
	.desc("cumumlative delay values")
	;

    seg0_entry_error_dist
	.init(-10,9,1)
	.name(n+"seg0_entry_error")
	.desc("Seg0_entry_time - max(dispatch+n-1, st_time)")
	.flags(cdf)
	;

    pred_issue_error_dist
	.init(-10,99,1)
	.name("pred_issue_error")
	.desc("error in predicted issue times")
	.flags(pdf | cdf)
	;

    //
    //  Predictor stats
    //
    if (cpu->use_hm_predictor)
	hm_predictor->regStats();

    if (cpu->use_lr_predictor)
	lr_predictor->regStats();

    //
    //  Register the segment-specific statistics
    //
    for (int i = 0; i < num_segments; ++i)
	queue_segment[i]->regStats(cpu, n);

}

void
SegmentedIQ::regModelFormulas(unsigned num_threads)
{
    using namespace Stats;

    //    string n = cpu->name() + ".IQ:";
    string n = name() + ":";
    string c = cpu->name() + ".";

    //
    //  Stall statistics
    //

    rob_chain_frac
	.name(n+"rob_chain_frac")
	.desc("Fraction of all chains ONLY in ROB")
	;
    rob_chain_frac = rob_chain_heads / cpu->chain_heads;

    chains_avg
	.name(n+"chains_avg")
	.desc("average number of chains in use")
	;
    chains_avg = chains_cum / cpu->numCycles;

    deadlock_ratio
	.name(n+"deadlock_ratio")
	.desc("Fraction of time IQ was deadlocked")
	;
    deadlock_ratio = deadlock_cycles / cpu->numCycles;

    deadlock_avg_dur
	.name(n+"deadlock_avg_dur")
	.desc("Average duration of deadlock event")
	;
    deadlock_avg_dur = deadlock_cycles / deadlock_events;

    if (use_bypassing) {
	bypass_avg
	    .name(n+"bypass_avg")
	    .desc("average number of segments bypassed (when used)")
	    ;
	bypass_avg = bypassed_segments / bypassed_insts;

	bypass_frac
	    .name(n+"bypass_frac")
	    .desc("fraction of instructions using bypassing")
	    ;
	bypass_frac = 100 * bypassed_insts / cpu->dispatch_count_stat;
    }

    if (s0_st_limit) {
	st_limit_rate
	    .name(n+"st_limit_rate")
	    .desc("average S/T limit events per cycle")
	    ;
	st_limit_rate = st_limit_events / cpu->numCycles;
    }

    if (use_pushdown) {
	pushdown_rate
	    .name(n+"pushdown_rate")
	    .desc("average push-down events per cycle")
	    ;
	pushdown_rate = total_pd_events / cpu->numCycles;

	pd_inst_rate
	    .name(n+"pd_inst_rate")
	    .desc("average push-down insts per event")
	    ;
	pd_inst_rate = total_pd_count / total_pd_events;
    }

    //
    //  Ready instruction stats
    //
    ready_fraction
	.name(n+"00:ready_fraction")
	.desc("Fraction of segment 0 insts ready to issue")
	;
    ready_fraction = 100 * queue_segment[0]->getRQ()->ready_inst
	             / queue_segment[0]->cum_insts;

    frac_of_all_ready
	.name(n+"00:frac_of_all_ready")
	.desc("Fraction of all ready insts that are in seg 0")
	;
    frac_of_all_ready = 100 * queue_segment[0]->getRQ()->ready_inst
	                / total_ready_count;

    //
    //  Chain prediction stats
    //
    correct_pred_rate
	.name(n+"correct_pred_rate")
	.desc("pct of 2-not-rdy-op insts correctly pred")
	;
    correct_pred_rate = 100 * correct_last_op / (  cpu->two_op_inst_count
			    - cpu->one_rdy_inst_count );

    delay_at_rdy_avg
	.name(n+"delay_at_rdy_avg")
	.desc("average delay value when last op becomes ready")
	;
    delay_at_rdy_avg = cum_delay / cpu->exe_inst;

    //
    //  Predictor stats are shared... only register once
    //
    if (thisCluster() == 0) {
	if (cpu->use_hm_predictor)
	    hm_predictor->regFormulas();
	
	if (cpu->use_lr_predictor)
	    lr_predictor->regFormulas();
    }	

    //
    //  Register the segment-specific statistics
    //
    for (int i = 0; i < num_segments; ++i)
	queue_segment[i]->regFormulas(cpu, n);
}

//
//  Free the chain associated with this ROB entry
//
//  Returns TRUE if the chain has been de-allocated
//
bool
SegmentedIQ::release_chain(ROBStation *rob)
{
    bool rv = false;
    unsigned chain_num = rob->head_chain;

    //
    //  "IF" statement MOVED HERE FROM segment_t::release_chain()
    //  FIXME: this should probably do more than just skip the
    //  per-cluster releases...
    //
    //  We don't want to free a chain that really belongs to someone else!
    if (rob->seq == (*chain_info)[chain_num].creator) {
	for (unsigned i = 0; i < num_segments; ++i)
	    queue_segment[i]->release_chain(rob);

	//  this block duplicated out of segment_t::release_chain()
	if (deadlock_slot.notnull()) {
	    unsigned n_chains = 0;
	    bool was_chained = false;

	    //  check it's ideps to see if we need to unchain them
	    for (int j = 0; j < TheISA::MaxInstSrcRegs; ++j) {
		if (deadlock_slot->idep_info[j].chained) {

		    //  note that this inst was chained when we got here
		    was_chained = true;

		    if (deadlock_slot->idep_info[j].follows_chain
			== chain_num)
		    {
			deadlock_slot->idep_info[j].chained = false;
		    } else {
			// chained to another inst
			++n_chains;
		    }
		}
	    }

	    //
	    //  If this inst is no longer on _any_ chain...
	    //
	    if (was_chained && n_chains == 0) {
		//  DO NOT put it on the self-timed list... the
		//  segment add() will do that

		//  We need to set the self-timed flag in any register produced
		//  by this instruction
		ROBStation *rob = deadlock_slot->rob_entry;
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
    } else {
	//
	//  This instruction was not the creator of the chain...
	//
	return rv;
    }

    assert(chain_info->chainsInUseThread(rob->thread_number) > 0);

    //  the release() method returns true if this was the last cluster
    //  to release the chain
    if (chain_info->release(rob->head_chain, rob->thread_number)) {
	//
	//  Ugly nasty hack to make chain stuff work with EXTERNAL
	//  chainWires stuff
	//
	//  (We don't want to be releasing the wire until ALL
	//  writebacks are completed)
	//
	if (cpu->chainWires != 0)
	    for (int clust = 0; clust < cpu->numIQueues; ++clust)
		cpu->chainWires->releaseWire(clust, chain_num);

	rv = true;
    }

    //
    //  We only deallocate register entries from the "home" cluster
    //
    if (rob->queue_num == thisCluster()) {
	//
	//  Look through the register-info table, and set any registers
	//  resulting from instructions on this chain to self-time
	//
	for (int i = 0; i < TotalNumRegs; ++i) {
	    RegInfoElement &r = (*reg_info_table)[rob->thread_number][i];

	    if (r.isChained() && r.chainNum() == rob->head_chain)
		r.unChain();
	}
    }

    return rv;
}


//
//  This (mis-named) function is called as a result of a cache-access
//  being processed
//
//  If the access is a cache-miss, force the chained instructions to
//  stop moving through the queue until writeback occurs
//
void
SegmentedIQ::cachemissevent_handler(Addr pc, int hm_prediction,
				    ROBStation *rob, bool squashed,
				    unsigned ann_value)
{
    unsigned predicted_value, actual_value;

    if (hm_predictor) {
	if (hm_prediction == MA_HIT)
	    predicted_value = 1;  // Hit
	else
	    predicted_value = 0;  // Miss

	if (ann_value == MA_HIT)
	    actual_value = 1;
	else
	    actual_value = 0;

	//  Update the predictor
	hm_predictor->record(pc >> 2, actual_value, predicted_value);
    }

    if (ann_value == MA_CACHE_MISS) {
	if (!squashed) {
#if USE_NEW_SELF_TIME_CODE
	    if (rob->seq == (*chain_info)[rob->head_chain].creator)
		(*chain_info)[rob->head_chain].self_timed = false;
#else
	    for (unsigned i = 0; i < num_segments; ++i)
		queue_segment[i]->stop_self_time(rob);
#endif
	}
    }
}


//===========================================================================

SegmentedIQ::iq_iterator
SegmentedIQ::oldest()
{
    iq_iterator p, q = queue_segment[0]->oldest();

    for (unsigned i = 1; i < num_segments; ++i) {
	p = queue_segment[i]->oldest();

	if (p.notnull()) {
	    if (q.notnull()) {
		if (p->seq < q->seq) {
		    //  neither is null, use sequence
		    q = p;
		}
	    } else {
		//  q is null and p is not
		q = p;
	    }
	}
    }

    if (deadlock_slot.notnull())
	if (q.isnull() || q->seq > deadlock_slot->seq)
	    q = deadlock_slot;

    return q;
}

SegmentedIQ::iq_iterator
SegmentedIQ::oldest(unsigned thread) {
    iq_iterator p, q = queue_segment[0]->oldest(thread);

    for (unsigned i = 1; i < num_segments; ++i) {
	p = queue_segment[i]->oldest(thread);

	if (p.notnull()) {
	    if (q.notnull()) {
		if (p->seq < q->seq) {
		    //  neither is null, use sequence
		    q = p;
		}
	    } else {
		//  q is null and p is not
		q = p;
	    }
	}
    }

    if (deadlock_slot.notnull()) {
	if ((q->seq > deadlock_slot->seq)
	    && (deadlock_slot->thread_number() == thread))
	{
	    q = deadlock_slot;
	}
    }

    return q;
}

//===========================================================================

void
SegmentedIQ::dump_internals()
{
    cout << "  Deadlock Mode: " << deadlock_recovery_mode << endl;
    cout << "        segment: " << deadlock_seg_flag << endl;

    cout << "  Total instructions: " << total_insts << endl;
    for (int t = 0; t < cpu->number_of_threads; ++t)
	cout << "    Thread " << t << ": " << insts[t] << endl;

    cout << "  Chain heads in ROB: " << cpu->chain_heads_in_rob << endl;
    cout << "  Chain heads in IQ:  " << iq_heads << endl;
    for (int t = 0; t < cpu->number_of_threads; ++t)
	cout << "    Active chains (thread " << t << "): "
	     << chain_info->chainsInUseThread(t) << endl;
}


void
SegmentedIQ::dump_chains(unsigned s)
{
    queue_segment[s]->dump_chains();
}


void
SegmentedIQ::dump_chains()
{
    for (int s = 0; s < num_segments; ++s)
	dump_chains(s);
}

void
SegmentedIQ::dump(int mode)
{
    if (mode == 0) {
	short_dump();
	return;
    }

    cprintf("=========================================================\n"
	    "%s full dump (cycle %d)\n"
	    "  Total Instructions: %u\n"
	    "  By thread: [", name(), curTick, total_insts);
    for (unsigned i = 0; i < cpu->number_of_threads; ++i) {
	cprintf("%u", insts[i]);
	if (i == cpu->number_of_threads - 1)
	    cout << "]\n";
	else
	    cout << ", ";
    }
    cout << "---------------------------------------------------------\n"
	"Chain Status:\n";
    chain_info->dump();
    cout << "---------------------------------------------------------\n";
    for (int i = last_segment; i >= 0; --i)
	queue_segment[i]->dump(mode);
}


//
//  Short dump...
//
void
SegmentedIQ::short_dump()
{
    cprintf("=========================================================\n"
	    "%s short dump (cycle %u) (%u instructions)\n"
	    "---------------------------------------------------------\n",
	    name(), curTick, total_insts);

    for (int i = last_segment; i >= 0; --i)
	queue_segment[i]->short_dump();

    if (deadlock_recovery_mode)
	cout << "==> Deadlock recovery mode\n";
}

void
SegmentedIQ::raw_dump()
{
    cout << "No raw dump\n";
}

void
SegmentedIQ::rq_dump()
{
    cprintf("=========================================================\n"
	    "%s RQ dump (cycle %u)\n", name(), curTick);
    for (int i = 0; i < num_segments; ++i)
	queue_segment[i]->rq_dump();
}

void
SegmentedIQ::rq_raw_dump()
{
    cprintf("=========================================================\n"
	    "%s RQ dump (cycle %d)\n", name(), curTick);
    queue_segment[0]->rq_raw_dump();
}


//////////////////////////////////////////////////////////////////////////////
//   Interface to INI file mechanism
//////////////////////////////////////////////////////////////////////////////


BEGIN_DECLARE_SIM_OBJECT_PARAMS(SegmentedIQ)

    Param<unsigned> num_segments;
    Param<unsigned> max_chain_depth;
    Param<unsigned> segment_size;
    Param<unsigned> segment_thresh;
    Param<bool>     en_thread_priority;
    Param<bool>     use_bypassing;
    Param<bool>     use_pushdown;
    Param<bool>     use_pipelined_prom;

END_DECLARE_SIM_OBJECT_PARAMS(SegmentedIQ)


BEGIN_INIT_SIM_OBJECT_PARAMS(SegmentedIQ)

    INIT_PARAM(num_segments,       "number of IQ segments"),
    INIT_PARAM(max_chain_depth,    "max chain depth"),
    INIT_PARAM(segment_size,       "segment size"),
    INIT_PARAM(segment_thresh,     "segment delta threshold"),
    INIT_PARAM(en_thread_priority, "enable thread priority"),
    INIT_PARAM_DFLT(use_bypassing, "enable bypass at dispatch", true),
    INIT_PARAM_DFLT(use_pushdown,  "enable instruction pushdown", true),
    INIT_PARAM_DFLT(use_pipelined_prom, "enable pipelined chain wires", true)

END_INIT_SIM_OBJECT_PARAMS(SegmentedIQ)


CREATE_SIM_OBJECT(SegmentedIQ)
{

    return new SegmentedIQ(getInstanceName(),
			   num_segments,
			   max_chain_depth,
			   segment_size,
			   segment_thresh,
			   en_thread_priority,
			   use_bypassing,
			   use_pushdown,
			   use_pipelined_prom);
}

REGISTER_SIM_OBJECT("SegmentedIQ", SegmentedIQ)
