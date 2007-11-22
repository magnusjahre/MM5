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

#ifndef __ENCUMBERED_CPU_FULL_LS_QUEUE_HH__
#define __ENCUMBERED_CPU_FULL_LS_QUEUE_HH__

#include <string>

#include "cpu/smt.hh"
#include "encumbered/cpu/full/iq/iq_station.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/readyq.hh"

class FullCPU;
class RegInfoElement;
class NewChainInfo;
class lsq_readyq_policy
{
  public:
    static bool goes_before(FullCPU *cpu,  BaseIQ::iterator &first,
			    BaseIQ::iterator &second,
			    bool use_thread_priorities) {
	return (first->seq < second->seq);
    }
};


class load_store_queue : public BaseIQ
{
  private:
    res_list<IQStation> *queue;

    unsigned total_insts;
    unsigned insts[SMT_MAX_THREADS];

    unsigned total_loads;
    unsigned loads[SMT_MAX_THREADS];
    unsigned total_stores;
    unsigned stores[SMT_MAX_THREADS];

    bool pri_issue;
    ready_queue_t<IQStation, lsq_readyq_policy> *ready_list;

  public:
    typedef BaseIQ::iterator iterator;
    typedef BaseIQ::rq_iterator rq_iterator;

    //  Constructor
    load_store_queue(FullCPU *_cpu, const std::string &_name, unsigned _size,
		     bool pri);
    ~load_store_queue();

    virtual IQType type() { return LSQ; }

    //  We've got to have a way to add/remove instructions
    virtual iterator add_impl(DynInst *, InstSeqNum seq, ROBStation *rob,
			      RegInfoElement *, NewChainInfo *);

    virtual void regModelStats(unsigned threads);
    virtual void regModelFormulas(unsigned threads);

    virtual void tick_model_stats() {};
    virtual void tick_ready_stats() {ready_list->tick_stats();};

    virtual unsigned free_slots()
    {
	return queue->size() - total_insts;
    }

    virtual unsigned writeback(ROBStation *, unsigned queue_num);

    virtual unsigned ready_count() { return ready_list->count(); }

    virtual rq_iterator issuable_list() { return ready_list->head(); }

    virtual rq_iterator issue_impl(rq_iterator &p)
    {
	rq_iterator next;

	if (p.notnull()) {
	    next = p.next();

	    squash(*p);
	}
	return next;
    }

    virtual iterator squash(iterator &e);

    //  We also need this one method because the simulator can't use the
    //  normal "broadcast" method for the effective-address calculation
    virtual void ready_list_enqueue(iterator &q)
    {
	q->rq_entry = ready_list->enqueue(q);
	q->ready_timestamp = curTick;
	q->queued = true;
    }


    //  Find the oldest IQ instruction
    virtual iterator oldest()
    {
	return(queue->head());
    };

    virtual iterator oldest(unsigned thread)
    {
	iterator i;
	for (i=queue->head();
	     i.notnull() && (i->thread_number() != thread);
	     i = i.next());

	return( i );
    };


    //  Access the number of things in the queueu
    virtual unsigned count() {return total_insts;};
    virtual unsigned count(unsigned t) {return insts[t];};

    unsigned load_count() { return total_loads; }
    unsigned load_count(unsigned t) { return loads[t]; }
    unsigned store_count() { return total_stores; }
    unsigned store_count(unsigned t) { return stores[t]; }

    virtual void dump();
    virtual void raw_dump();
    virtual void rq_dump();
    virtual void rq_raw_dump();

    virtual iterator head() {return queue->head();}
    virtual iterator tail() {return queue->tail();}

    virtual void tick() {
	ready_list->tick();
    }
};

#endif // __ENCUMBERED_CPU_FULL_LS_QUEUE_HH__
