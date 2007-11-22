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

#ifndef __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_IQ_SEG_HH__
#define __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_IQ_SEG_HH__

#include <string>

#include "base/statistics.hh"
#include "encumbered/cpu/full/iq/iq_station.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/readyq.hh"

class FullCPU;
class RQ_Seg_Policy;
class RQ_Issue_Policy;
class SegmentedIQ;

struct RegInfoTable;
class SegChainInfoTable;

///////////////////////////////////////////////////////////////////////////////
//
//  These segments make up the instruction storage part of the segmented IQ
//
//
class segment_t
{
  public:
    //
    //  Types
    //
    typedef res_list<IQStation>                     iq_list;
    typedef res_list<IQStation>::iterator           iq_iterator;
    typedef res_list< iq_iterator >                  iq_iterator_list;
    typedef res_list< iq_iterator >::iterator        iq_it_list_iterator;
    typedef ready_queue_base_t<IQStation>::iterator rq_iterator;

  private:
    SegmentedIQ *seg_queue;

    std::string name_string;
    unsigned segment_number;
    unsigned num_segments;
    unsigned num_chains;
    unsigned size;
    unsigned threshold;
    bool pipelined_promotion;

    unsigned total_insts;
    unsigned insts[SMT_MAX_THREADS];
    unsigned current_ops_ready;

    iq_iterator_list *queue;
    ready_queue_base_t<IQStation> *ready_list;

    //  This is the list of instructions by chain for this segment
    iq_iterator_list **chains;

    //  This is the list of instructions that have no chain
    iq_iterator_list *self_timed_list;

  public:
    //
    //  Statistics, etc
    //
    Stats::Vector<> cum_insts;
    Stats::Vector<> cum_chained;
    Stats::Vector<> cum_ops_ready;
    Stats::Scalar<> segment_full;
    Stats::Scalar<> segment_empty;

    Stats::Scalar<> cycles_low_ready;        // number of cycles
    Stats::Scalar<> insts_fanout_loads;      // number of loads involved
    Stats::Scalar<> insts_waiting_for_loads; // number of insts waiting for loads
    Stats::Scalar<> insts_waiting_for_unissued;

    Stats::Formula occ_rate;
    Stats::Formula full_frac;
    Stats::Formula empty_frac;
    Stats::Formula prom_rate;
    Stats::Formula seg_residency;
    Stats::Formula rate_loads;
    Stats::Formula rate_wait_loads;
    Stats::Formula rate_wait_uniss;
    Stats::Formula avg_wait_load;
    Stats::Formula frac_low_ready;
    Stats::Formula chained_frac;
    Stats::Formula ops_ready_rate;
    Stats::Formula pd_rate;
    Stats::Formula pd_inst_rate;

    unsigned number_of_threads;

  public:
    Stats::Vector<> cum_promotions;   // set in promote_insts()
    RegInfoTable *reg_info_table;
    SegChainInfoTable *chain_info;

  public:
    //  Constructor
    segment_t(SegmentedIQ *iq, std::string n, unsigned seg_num,
	      unsigned num_segs, unsigned sz, unsigned n_chains,
	      unsigned thresh, bool pipelined, bool en_pri);

    const std::string name() { return name_string; }

    void regStats(FullCPU *cpu, std::string &n);
    void regFormulas(FullCPU *cpu, std::string &n);

    ready_queue_base_t<IQStation> *getRQ(){return ready_list;}

    bool promotable(iq_iterator &inst);

    bool full() {return queue->full();}
    bool empty() {return queue->empty();}

    iq_iterator add(iq_iterator &q);

    iq_iterator remove(iq_iterator &e);

    void self_time(ROBStation *rob);
    void stop_self_time(ROBStation *rob);
    void release_chain(ROBStation *rob);

    rq_iterator issuable_list() {
	return ready_list->head();
    }

    void enqueue(iq_iterator &p);

    unsigned proper_segment(iq_iterator &i);

    unsigned ready_count() { return ready_list->count(); }
    unsigned count() { return total_insts; }
    unsigned count(unsigned t) { return insts[t]; }

    iq_it_list_iterator head() { return queue->head();}
    iq_it_list_iterator tail() { return queue->tail();}

    unsigned ops_ready_count() { return current_ops_ready; }

    unsigned free_slots() { return size - queue->count(); }

    //  Determine which instructions in this segment are promotable
    void check_promotable();

    void tick();

    void tick_stats();
    void tick_ready_stats();
    void collect_inst_stats();

    void decrement_delay(iq_iterator i, int op_num);

    /////////////////////////////////////////////////////////////////////
    iq_iterator oldest() {
	iq_it_list_iterator old = 0;
	iq_it_list_iterator i = queue->head();

	for (; i.notnull(); i = i.next())
	    if (old.isnull() || (*old)->seq > (*i)->seq)
		old = i;

	if (old.notnull())
	    return *old;

	return 0;
    }

    iq_iterator oldest(unsigned thread) {
	iq_it_list_iterator old = 0;
	iq_it_list_iterator i;

	for (i = queue->head(); i.notnull(); i = i.next())
	    if ((*i)->thread_number() == thread &&
		(old.isnull() || (*old)->seq > (*i)->seq))
		    old = i;

	if (old.notnull())
	    return *old;

	return 0;
    }

    iq_iterator youngest() {
	iq_it_list_iterator young = 0;
	iq_it_list_iterator i = queue->tail();

	for (; i.notnull(); i = i.prev())
	    if (young.isnull() || (*young)->seq < (*i)->seq)
		young = i;

	if (young.notnull())
	    return *young;

	return 0;
    }

    iq_iterator youngest(unsigned thread) {
	iq_it_list_iterator young = 0;
	iq_it_list_iterator i = queue->tail();

	for (; i.notnull(); i = i.prev())
	    if ((*i)->thread_number() == thread)
		if (young.isnull() || (*young)->seq < (*i)->seq)
		    young = i;

	if (young.notnull())
	    return *young;

	return 0;
    }

    iq_iterator lowest_dest() {
	iq_it_list_iterator low = 0;
	iq_it_list_iterator i = queue->tail();

	for (; i.notnull(); i = i.prev())
	    if (low.isnull() || (*low)->dest_seg < (*i)->dest_seg)
		low = i;

	if (low.notnull())
	    return *low;

	return 0;
    }

    iq_iterator lowest_dest(unsigned thread) {
	iq_it_list_iterator low = 0;
	iq_it_list_iterator i = queue->tail();

	for (; i.notnull(); i = i.prev())
	    if ((*i)->thread_number() == thread)
		if (low.isnull() || (*low)->dest_seg < (*i)->dest_seg)
		    low = i;

	if (low.notnull())
	    return *low;

	return 0;
    }

    /////////////////////////////////////////////////////////////////////

    void dump_chain(unsigned c);
    void dump_chains();

    //
    //  Normal segment dump
    //
    void dump(int length = 0);

    //
    //  Special one-line dump for looking at massive amounts of data
    //
    void short_dump();

    void rq_dump() { ready_list->dump(); }
    void rq_raw_dump() { ready_list->raw_dump(); }
};
typedef segment_t * segment_ptr_t;



///////////////////////////////////////////////////////////////////////////////
//
//  This ready-list policy used for all segments EXCEPT Segment #0
//
class RQ_Seg_Policy {
  public:

    static bool goes_before(FullCPU *cpu,
			    res_list<IQStation>::iterator &first,
			    res_list<IQStation>::iterator &second,
			    bool use_thread_priorities);
};

//
//  This ready-list policy used ONLY for Segment #0
//
class RQ_Issue_Policy {
  public:

    static bool goes_before(FullCPU *cpu,
			    res_list<IQStation>::iterator &first,
			    res_list<IQStation>::iterator &second,
			    bool use_thread_priorities);
};

#endif // __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_IQ_SEG_HH__
