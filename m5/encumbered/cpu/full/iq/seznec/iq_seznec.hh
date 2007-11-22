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
//

#ifndef __ENCUMBERED_CPU_FULL_IQ_SEZNEC_HH__
#define __ENCUMBERED_CPU_FULL_IQ_SEZNEC_HH__

#include "base/sat_counter.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/iq/iq_station.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/readyq.hh"

class RegInfoElement;

extern int use_hm_predictor;

#define MAX_INSNS (issue_buf_size + (num_lines * line_size))


class RQSeznecPolicy;

class SeznecIQ : public BaseIQ
{

  public:
    typedef res_list<IQStation>::iterator iq_iterator;
    typedef res_list<iq_iterator> iq_iterator_list;
    typedef res_list<iq_iterator>::iterator iq_it_list_iterator;
    typedef ready_queue_base_t<IQStation>::iterator rq_iterator;
    typedef iq_iterator_list *iq_it_list_ptr;


  private:
    res_list<IQStation> *inst_store;
    iq_iterator_list *issue_buffer;
    iq_iterator_list **presched_buffer;

    ready_queue_t<IQStation, RQSeznecPolicy> *ready_list;

    unsigned num_lines;
    unsigned line_size;
    unsigned issue_buf_size;
    unsigned use_hm_predictor;

    unsigned active_line_index;

    unsigned cache_hit_latency;

    unsigned presched_insts;
    unsigned presched_thread_insts[SMT_MAX_THREADS];

    unsigned total_insts;
    unsigned thread_insts[SMT_MAX_THREADS];

    struct new_slot_t
    {
	unsigned schedule_line;
	unsigned result_line;
	bool     valid;
	int      reg;

	new_slot_t() : valid(false) { }
    };

    struct reg_info_t
    {
	unsigned use_line;
	bool scheduled;

	reg_info_t() : scheduled(false) { }
    };

    reg_info_t reg_info[TotalNumRegs];

    SaturatingCounterPred *hm_predictor;

  public:
    /////////////////////////////////////////////////////////////////////
    //
    //  Constructor
    //
    SeznecIQ(std::string _name,
	     unsigned _num_lines,
	     unsigned _line_size,
	     unsigned _issue_buf_size,
	     bool     _use_hm_pred);

    ~SeznecIQ();

    virtual IQType type() { return Seznec; }

    //  Add an instruction to the queue
    virtual iq_iterator add_impl(DynInst *, InstSeqNum seq,
				 ROBStation *, RegInfoElement *,
				 NewChainInfo *);

    //  Remove an instruction from the queue
    virtual iq_iterator squash(iq_iterator &i) {
	iq_iterator n = i.next();

	if (i.notnull()) {
	    if (i->in_issue_buffer) {
		issue_buffer->remove(i->queue_entry);
	    } else {
		--presched_insts;
		--presched_thread_insts[i->thread_number()];
		presched_buffer[i->presched_line]->remove(i->queue_entry);
	    }

	    if (i->queued) {
		ready_list->remove(i->rq_entry);
		i->queued = false;
		i->rq_entry = 0;
	    }

	    --total_insts;
	    --thread_insts[i->thread_number()];

	    inst_store->remove(i);
	}

	return n;
    }

    //  Broadcast result of executed instruction back to IQ
    virtual unsigned writeback(ROBStation *, unsigned queue_num);

    virtual void set_cache_hit_latency(unsigned lat)
    {
	cache_hit_latency = lat;
    }

    virtual void regModelStats(unsigned num_threads)
    {
	ready_list->regStats(SimObject::name() + ":RQ", num_threads);
    }

    virtual void regModelFormulas(unsigned num_threads)
    {
	ready_list->regFormulas(SimObject::name() + ":RQ", num_threads);
    }

    virtual void tick_model_stats() {};

    //  Get list of issuable instructions. The IQ must be notified of
    //  instructions chosen for issue via a call to issue()
    //
    //  The issuable list is allocated by the call to issuable_list()
    //  and will remain valid until the next time this call is made
    virtual rq_iterator issuable_list() { return ready_list->head(); }

    virtual unsigned ready_count() { return ready_list->count(); }

    //
    //  Handle the removal of the specified instruction from both
    //  the instruction queue and the ready queue
    //
    virtual rq_iterator issue_impl(rq_iterator &i) {
	rq_iterator n = 0;

	if (i.notnull()) {
	    n = i.next();
	    squash(*i);
	}
	return n;
    }

    //  For things which must be done every cycle
    void tick();

    //  Returns the number of available slots for new instructions
    virtual unsigned free_slots() {
	return (num_lines*line_size) - presched_insts;
    }

    //  Returns count of all instructions, or count of a particular thread
    virtual unsigned count() { return total_insts; }
    virtual unsigned count(unsigned thread) { return thread_insts[thread]; }

    virtual void dump();
    virtual void raw_dump() {};
    virtual void rq_dump() {};
    virtual void rq_raw_dump() {};

    //  Provide some method of moving through the queue...
    //
    //  NOTE:  You should assume NOTHING about the order of the elements
    //         returned by these methods!
    //
    virtual iq_iterator head() { return inst_store->head(); }
    virtual iq_iterator tail() { return inst_store->tail(); }

    virtual iq_iterator oldest();
    virtual iq_iterator oldest(unsigned thread);

    virtual void ready_list_enqueue(iq_iterator &i) {
	if (i.notnull()) {
	    i->rq_entry = ready_list->enqueue(i);
	    i->queued = true;
	}
    }



  private:
    new_slot_t schedule_inst(iterator &);
    bool later_than(unsigned, unsigned);

  public:
    //
    //  This function is called as a result of a cache-access
    //  being processed
    //
    void cachemissevent_handler(Addr pc, int hm_prediction,
				ROBStation *rob, bool squashed,
				unsigned ann_value) {
	int pred_val = 0;

	if (ann_value == MA_HIT)
	    pred_val = 1;

	//  Update the predictor
	hm_predictor->record(pc, pred_val, (ann_value==hm_prediction));
    }


    //
    //  Allocate new events via this function, since the event
    //  processing function needs access to our data
    //
    CacheMissEvent *new_cm_event(ROBStation *rob) {
	return new CacheMissEvent(rob, this);
    }

};



#endif // __ENCUMBERED_CPU_FULL_IQ_SEZNEC_HH__
