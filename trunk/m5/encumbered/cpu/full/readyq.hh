/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005
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

#ifndef __ENCUMBERED_CPU_FULL_READYQ_HH__
#define __ENCUMBERED_CPU_FULL_READYQ_HH__

#include <cassert>
#include <string>

#include "base/fast_alloc.hh"
#include "base/res_list.hh"
#include "base/statistics.hh"
#include "cpu/smt.hh"

// forward decls
class FullCPU;

///////////////////////////////////////////////////////////////////////
//
//  NOTE:
//
//   This thing is broken into three pieces so that we could separate
//   out the counting functions, and provide a "base" class iterator
//   that doesn't require knowledge of the sorting policy...
//
//
//
//
///////////////////////////////////////////////////////////////////////


//
//
//
class ready_queue_base_base_t // crappy name, I know
{
  protected:

    FullCPU **cpu_ptr_ptr;  // a pointer to a pointer to the cpu
    std::string name;

    unsigned total_insts;
    unsigned thread_insts[SMT_MAX_THREADS];

    Counter ready_inst_squares[SMT_MAX_THREADS];
    Counter total_rdy_inst;
    Counter total_rdy_squares;

  public:
    //
    //  Stat values
    //
    Stats::Vector<> ready_inst;
    Stats::VectorStandardDeviation<> rdy_inst_stdev_stat;

    Stats::Vector<> trans_ready_count;
    Stats::Scalar<> max_trans_ready;

    Stats::Formula rdy_rate;
    Stats::Formula rdy_x_rate;

  protected:
    Counter num_inst_became_ready;

    //
    //  Constructor
    //
    ready_queue_base_base_t(FullCPU** _cpu, std::string n)
	: cpu_ptr_ptr(_cpu), name(n)
    {
	for (int i = 0; i < SMT_MAX_THREADS; ++i)
	    thread_insts[i] = 0;

	total_insts = 0;
	num_inst_became_ready = 0;
    }

    virtual ~ready_queue_base_base_t() {}

    virtual void queue_dump() = 0;
    virtual void queue_raw_dump() = 0;

    void add_inst(unsigned thread) {
	++total_insts;
	++thread_insts[thread];
	++num_inst_became_ready;
	++trans_ready_count[thread];
    }

    void remove_inst(unsigned thread) {
	--total_insts;
	--thread_insts[thread];
    }

  public:

    void regStats(std::string name, unsigned threads);
    void regFormulas(std::string name, unsigned threads);

    unsigned count()		{ return total_insts; }
    unsigned count(unsigned t)	{ return thread_insts[t]; }

    void tick() {}

    // Do every-tick kinds of things:
    void tick_stats();

    void dump();

    void raw_dump();
};


template<class T>
class ready_queue_base_t : public ready_queue_base_base_t
{
  public:
    typedef typename res_list<typename res_list<T>::iterator>::
        iterator iterator;

  protected:
    res_list<typename res_list<T>::iterator> *queue;

    virtual void queue_dump()
    {
	for (iterator i = queue->head(); i.notnull(); i = i.next())
	    (*i)->dump();
    }

    virtual void queue_raw_dump()
    {
	queue->raw_dump();
    }

    ready_queue_base_t(FullCPU** cpu, std::string n, unsigned size);

    virtual ~ready_queue_base_t() {}

    // the only function that depends on the policy
    virtual iterator internal_enqueue(typename res_list<T>::iterator q) = 0;

  public:
    // updates per-thread counts
    iterator enqueue(typename res_list<T>::iterator q)
    {
	iterator p;

	assert(q.notnull());
	assert(!q->queued);

	add_inst(q->thread_number());

	// This routine doesn't touch counters, etc.
	p = internal_enqueue(q);

	(*p)->rq_entry = p;

	return p;
    }

    void resort();

    iterator head() { return queue->head(); }


    iterator remove(iterator i)
    {
	assert(i.notnull());

	remove_inst((*i)->thread_number());

	return queue->remove(i);
    }
};


//
//  The data_type must be the templated class like of a "res_list<T>"
//  (an IQStation, for instance)
//

//
//  "T" is the base data type
//  "P" is the policy class
//
template<class T, class P>
class ready_queue_t : public ready_queue_base_t<T>
{
  private:
    bool prioritize;
    FullCPU** cpu_ptr_ptr;

  public:
    //  somewhat redundant, but useful for removing warning messages
    typedef typename res_list<typename res_list<T>::iterator>::iterator
               iterator;

    //
    //  Constructors
    //
    ready_queue_t(FullCPU** cpu, std::string n, unsigned size=20, bool _prio=false)
	: ready_queue_base_t<T>(cpu, n, size)
    {
	prioritize = _prio;
	cpu_ptr_ptr = cpu;
    }

    //  Required virtual destructor
    virtual ~ready_queue_t() {}

  protected:
    //
    //  This internal routine just places the entry into the queue
    //
    iterator internal_enqueue(typename res_list<T>::iterator q)
    {
	iterator p;

	if (this->queue->head().notnull()) {
	    //  Find the correct place for this entry and return an iterator
	    //  to it...
	    for (iterator i = this->queue->head(); i.notnull(); i = i.next()) {
		if (!P::goes_before(*cpu_ptr_ptr, *i, q, prioritize)) {
		    p = this->queue->insert_after(i.prev());
		    break;
		}
	    }

	    if (p.isnull()) {
		//  Must go at the end of the list
		p = this->queue->add_tail();
	    }
	} else {
	    //  Insert this element at the head
	    p = this->queue->insert_after(0);
	}

	*p = q;
	return p;
    }

};


#endif // __ENCUMBERED_CPU_FULL_READYQ_H__
