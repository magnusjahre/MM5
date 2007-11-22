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

//==========================================================================
//
//  This is the segmented IQ
//
//  CAPS are not currently implemented
//

#ifndef __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_IQ_SEGMENTED_HH__
#define __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_IQ_SEGMENTED_HH__

#include "base/hybrid_pred.hh"
#include "base/sat_counter.hh"
#include "base/statistics.hh"
#include "encumbered/cpu/full/iq/iq_station.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/iq/segmented/iq_seg.hh"
#include "encumbered/cpu/full/readyq.hh"

class RegInfoElement;

#define STARTING_VALUE           0
#define MISS_ADDITIONAL_LATENCY  3



//////////////////////////////////////////////////////////////////////////////
//
//
//    The Segmented IQ Definition
//
//
//
//
//
//////////////////////////////////////////////////////////////////////////////
class SegmentedIQ : public BaseIQ
{
  public:
    //
    //  Useful types
    //
    typedef res_list<IQStation>                     iq_list;
    typedef res_list<IQStation>::iterator           iq_iterator;
    typedef res_list< iq_iterator >                  iq_iterator_list;
    typedef res_list< iq_iterator >::iterator        iq_it_list_iterator;
    typedef ready_queue_base_t<IQStation>::iterator rq_iterator;

    unsigned *seg_thresholds;

    //  so they can be accessed by the segments
    Stats::Scalar<> *pushdown_events;
    Stats::Scalar<> *pushdown_count;

    RegInfoTable *reg_info_table;

  private:

    //
    //  Configuration parameters
    //
    unsigned num_segments;
    unsigned last_segment;
    unsigned segment_size;
    unsigned segment_thresh;
    unsigned total_size;

    bool use_bypassing;
    bool use_pushdown;
    bool use_pipelined_prom;

    bool enable_priority;

    unsigned num_chains;
    unsigned max_chain_depth;
    unsigned total_thresh;

    unsigned cache_hit_latency;

    //
    // Internal stuff
    //

    SegChainInfoTable *chain_info;

    unsigned *free_slot_info;

    //  The array of queue segments
    segment_t   **queue_segment;

    bool deadlock_recovery_mode;
    iq_iterator deadlock_slot;
    unsigned deadlock_seg_flag;  // what to set "seg" to if
                                 // instruction is in the slot

    unsigned last_new_chain;

    unsigned total_insts;
    unsigned insts[SMT_MAX_THREADS];

    // used to help detect deadlock condition
    unsigned dedlk_promotion_count;
    unsigned dedlk_issue_count;

    //
    //  Statistics, etc.
    //
    Stats::Scalar<> total_pd_events;
    Stats::Scalar<> total_pd_count;

    Stats::Vector<> chains_cum;
    Stats::Vector<> chains_peak;

    Stats::Vector<> bypassed_segments;
    Stats::Vector<> bypassed_insts;

    Stats::Scalar<> deadlock_events;
    Stats::Scalar<> deadlock_cycles;
    Tick    last_deadlock;

    Stats::Scalar<> total_ready_count;
    Stats::Scalar<> cum_delay;

    Stats::Scalar<> st_limit_events;

    Stats::Scalar<> rob_chain_heads;  // cum count of chain heads ONLY in ROB
    unsigned  iq_heads;         // number of chain heads in the IQ

    Stats::Distribution<> pred_issue_error_dist;
    Stats::Distribution<> seg0_entry_error_dist;
    Stats::Distribution<> delay_at_ops_rdy_dist;
    Stats::Distribution<> ready_error_dist;
    Stats::Distribution<> max_chain_length_dist;
    Stats::Distribution<> inst_depth_dist;     // Instruction depth (in instructions)
    Stats::Distribution<> inst_depth_lat_dist; // Instruction depth (in cycles)

    Stats::Vector<> correct_last_op;

    Stats::VectorDistribution<> wrong_choice_dist;
    Stats::VectorDistribution<> disp_rdy_delay_dist;

    Stats::Scalar<> seg0_prom_early_count;

    Stats::Formula rob_chain_frac;
    Stats::Formula chains_avg;
    Stats::Formula deadlock_ratio;
    Stats::Formula deadlock_avg_dur;
    Stats::Formula bypass_avg;
    Stats::Formula bypass_frac;
    Stats::Formula st_limit_rate;
    Stats::Formula pushdown_rate;
    Stats::Formula pd_inst_rate;
    Stats::Formula ready_fraction;
    Stats::Formula frac_of_all_ready;
    Stats::Formula correct_pred_rate;
    Stats::Formula delay_at_rdy_avg;

    ///////////////////////////////////////////////////////
    //
    //  This is where the IQStations will actually live
    //  All we do is pass around iterators to these elements
    //
    iq_list *active_instructions;

    //  Promote instructions which were selected last cycle
    void promote_insts(unsigned src_segment);

    iq_iterator internal_remove(iq_iterator &e);

    bool check_deadlock();

    SaturatingCounterPred *hm_predictor;
    SaturatingCounterPred *lr_predictor;

  public:
    //
    //  Constructor
    //
    SegmentedIQ(std::string name,
		unsigned _num_segments,
		unsigned _max_chain_depth,
		unsigned _segment_size,
		unsigned _segment_thresh,
		bool     _en_pri,
		bool     _use_bypassing,
		bool     _use_pushdown,
		bool     _use_pipelined_prom);

    ~SegmentedIQ();

    virtual IQType type() { return Segmented; }

    //  Return the "cumulative threshold" value for a thread
    unsigned get_thresh(int seg) {return seg_thresholds[seg];}

    unsigned number_of_segments() {return num_segments;}

    //////////////////////////////////////////////////////////////////////
    //
    //  Methods required by the base class:
    //
    /////////////////////////////////////////////////////////////////////

    //  Add an instruction to the queue
    virtual iq_iterator add_impl(DynInst *, InstSeqNum seq,
			 ROBStation *, RegInfoElement *, NewChainInfo *);

    void init(FullCPU *_cpu, unsigned dw, unsigned iw, unsigned qn);

    virtual ClusterSharedInfo * buildSharedInfo();
    virtual void setSharedInfo(ClusterSharedInfo *p);

    virtual void registerLSQ(iq_iterator &p, BaseIQ::iterator &lsq);

    virtual unsigned free_slots() {
	unsigned slots = 0;

	// Since we only add instructions to the last segment
	slots = queue_segment[last_segment]->free_slots();

	//  Only subtract off for the deadlock slot instruction
	//  if we have a non-zero value... bad things happen to
	//  unsigned values otherwise.
	if (slots > 0 && deadlock_slot.notnull())
	    slots = slots - 1;

	return slots;
    }

    virtual void set_cache_hit_latency(unsigned lat)
    {
	cache_hit_latency = lat;
    }

    //  Register necessary statistics
    virtual void regModelStats(unsigned num_threads);
    virtual void regModelFormulas(unsigned num_threads);

    //  Broadcast result of executed instruction back to IQ
    virtual unsigned writeback(ROBStation *, unsigned queue_num);

    bool release_register(unsigned t, unsigned reg, InstSeqNum rob_seq);

    //
    //  We only actually enqueue instructions in Segment #0
    //
    virtual void ready_list_enqueue(iq_iterator &p) {
	if (p->segment_number == 0)
	    queue_segment[0]->enqueue(p);
    }

    //  Get list of issuable instructions. The IQ must be notified of
    //  instructions chosen for issue via a call to issue()
    //
    //  The issuable list is allocated by the call to issuable_list()
    //  and will remain valid until the next time this call is made
    virtual rq_iterator issuable_list() {
	return queue_segment[0]->issuable_list();
    }

    virtual unsigned ready_count() {
	return queue_segment[0]->ready_count();
    }



    //
    //  Allocate new events via this function, since the event
    //  processing function needs access to our data
    //
    CacheMissEvent *new_cm_event(ROBStation *rob) {
	return new CacheMissEvent(rob, this);
    }

    void cachemissevent_handler(Addr pc, int hm_prediction,
				ROBStation *rob, bool squashed,
				unsigned ann_value);

    unsigned choose_dest_segment(iq_iterator &p);


    //  Stuff we want to do every cycle
    void tick();
    void tick_model_stats();
    void tick_ready_stats();

    virtual void inform_dispatch(iq_iterator i);
    virtual void inform_issue(iq_iterator i);

    virtual rq_iterator issue_impl(rq_iterator &p);
    virtual void inform_squash(ROBStation *rob);
    virtual iq_iterator squash(iq_iterator &e);


    //  Provide some method of moving through the entire queue...
    //
    //  NOTE:  You should assume NOTHING about the order of the elements
    //         returned by these methods!
    //
    virtual iq_iterator head() {return active_instructions->head();}
    virtual iq_iterator tail() {return active_instructions->tail();}


    //  Returns count of all instructions, or count of a particular thread
    virtual unsigned count() { return total_insts; }
    virtual unsigned count(unsigned thread) { return insts[thread]; }

    virtual unsigned iw_count() { return queue_segment[0]->count(); }
    virtual unsigned iw_count(unsigned t) {
	return queue_segment[0]->count(t);
    }


    void dump_chains(unsigned s);
    void dump_chains();
    void dump_internals();

    //
    //  Normal dump
    //
    virtual void dump() { dump(0); }

    unsigned sanity_check();

    virtual void dump(int mode);
    void short_dump();
    virtual void raw_dump();
    virtual void rq_dump();
    virtual void rq_raw_dump();

    //
    //  The fetch-loss counting requires us to be able to find the oldest
    //  instruction (sometimes by thread)
    //
    virtual iq_iterator oldest();
    virtual iq_iterator oldest(unsigned thread);

  private:

    void parse_parameters(char *params);

    bool release_chain(ROBStation *rob);
};

#endif // __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_IQ_SEGMENTED_HH__
