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
//  This is the standard "collapsing" instruction queue
//

#ifndef __ENCUMBERED_CPU_FULL_IQ_STANDARD_HH__
#define __ENCUMBERED_CPU_FULL_IQ_STANDARD_HH__

#include <string>
#include <vector>

#include "encumbered/cpu/full/iq/iq_station.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/readyq.hh"
#include "encumbered/cpu/full/thread.hh"

class RegInfoElement;

//
//  This is the "new and improved" policy:
//
//  NON-Prioritized:  Strictly sort by age
//
//  Prioritized:  Strictly sort by priority.
//                Sort by age if priorities are the same
//
class iq_standard_readyq_policy
{
  public:

    static bool goes_before(FullCPU *cpu,
			    BaseIQ::iterator &first,
			    BaseIQ::iterator &second,
			    bool use_thread_priorities) {

	bool rv = false;

	unsigned first_p = cpu->thread_info[first->thread_number()].priority;
	unsigned second_p = cpu->thread_info[second->thread_number()].priority;

	if (use_thread_priorities) {

	    // if first has higher priority...
	    if (first_p > second_p)
		rv = true;
	    else if (second_p > first_p)
		// second higher priority
		rv = false;

	    else if (first->seq < second->seq)
		//  same priority
		rv = true;
	} else if (first->seq < second->seq)
	    rv = true;

	return rv;
    }
};

//
//  This is the old policy
//
class old_iq_standard_readyq_policy
{
  public:

    static bool goes_before(BaseIQ::iterator &first,
			    BaseIQ::iterator &second,
			    bool use_thread_priorities) {

#ifdef PRIORITIES_NEED_FIXING
	if (use_thread_priorities) {
	    unsigned first_priority =
		thread_info[first->thread_number()].priority;
	    unsigned second_priority =
		thread_info[second->thread_number()].priority;

	    if (first->high_priority()) {
		// goes before if 'second' is not high priority OR sequence
		//   OR if my priority is greater than his
		return !second->high_priority() ||
		    first_priority > second_priority ||
		    first->seq < second->seq;
	    } else {
		// goes before only if 'i' is not high-priority AND sequence
		//   OR if my priority is greater than his
		return first_priority > second_priority ||
		    !second->high_priority() && first->seq < second->seq;
	    }
	} else {
#endif
	    if (first->high_priority()) {
		// goes before if 'i' is not high priority OR sequence
		return !second->high_priority() || first->seq < second->seq;
	    } else {
		// goes before only if 'i' is not high-priority AND sequence
		return !second->high_priority() &&  first->seq < second->seq;
	    }
#ifdef PRIORITIES_NEED_FIXING
	}
#endif
    }
};


class StandardIQ : public BaseIQ
{
  private:
    std::string name;

    res_list<IQStation> *queue;
    unsigned total_insts;
    unsigned insts[SMT_MAX_THREADS];

    bool pri_issue;

    ready_queue_t<IQStation, iq_standard_readyq_policy> *ready_list;
    typedef BaseIQ::rq_iterator rq_iterator;
    typedef BaseIQ::iterator    iterator;


  public:
    //  Constructor
    StandardIQ(std::string _name, unsigned _size, int pri,
	       bool _caps_valid, std::vector<unsigned> _caps);

    ~StandardIQ();

    virtual IQType type() { return Standard; }

    //  We've got to have a way to add/remove instructions
    virtual iterator add_impl(DynInst *, InstSeqNum seq,
			      ROBStation *rob, RegInfoElement *,
			      NewChainInfo *);


    virtual unsigned writeback(ROBStation *, unsigned queue_num);

    virtual unsigned free_slots() { return queue->size() - total_insts; }

    virtual rq_iterator issuable_list() { return ready_list->head(); }

    virtual rq_iterator issue_impl(rq_iterator &p) {
	rq_iterator next = 0;

	if (p.notnull()) {
	    next = p.next();

	    //  Remove IQ element (& associated RQ entry)
	    squash(*p);
	}
	return next;
    }

    virtual unsigned ready_count() {return ready_list->count(); }

    virtual void StandardIQ::regModelStats(unsigned num_threads);
    virtual void StandardIQ::regModelFormulas(unsigned num_threads);

    virtual void StandardIQ::tick_model_stats();

    virtual iterator squash(iterator &e);


    //  Find the oldest IQ instruction
    virtual iterator oldest() { return(queue->head()); }
    virtual iterator oldest(unsigned thread) {
	iterator i;
	for (i=queue->head();
	     i.notnull() && (i->thread_number() != thread);
	     i = i.next());

	return( i );
    }


    //  Access the number of things in the queueu
    virtual unsigned count() { return total_insts; }
    virtual unsigned count(unsigned t) { return insts[t]; }

    virtual void dump();
    virtual void raw_dump();
    virtual void rq_dump();
    virtual void rq_raw_dump();

    virtual iterator head() { return queue->head(); }
    virtual iterator tail() { return queue->tail(); }

    virtual void tick_ready_stats() { ready_list->tick_stats(); }

    //
    //
    //
    virtual void ready_list_enqueue(iterator &q) {
	q->rq_entry = ready_list->enqueue(q);
	q->queued = true;
    }
};

#endif // __ENCUMBERED_CPU_FULL_IQ_STANDARD_HH__
