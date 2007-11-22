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

#include <iostream>

#include "base/cprintf.hh"
#include "base/statistics.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/iq/iq_station.hh"
#include "encumbered/cpu/full/readyq.hh"
#include "encumbered/cpu/full/storebuffer.hh"
#include "sim/stats.hh"

using namespace std;


//
//  Do every-tick kinds of things:
//
void
ready_queue_base_base_t::tick_stats()
{
    Counter all_threads = 0;

    if (num_inst_became_ready > max_trans_ready.value())
	max_trans_ready = num_inst_became_ready;

    num_inst_became_ready = 0;


    //  Update statistics
    for (int t = 0; t < (* cpu_ptr_ptr)->number_of_threads; ++t) {
	Counter i = thread_insts[t];

	rdy_inst_stdev_stat[t].sample(i);

	ready_inst[t] += i;
	ready_inst_squares[t] += i * i;

	all_threads += i;
    }

    total_rdy_inst += all_threads;
    total_rdy_squares += (all_threads * all_threads);
}

void
ready_queue_base_base_t::regStats(string name, unsigned threads)
{
    using namespace Stats;

    ready_inst
	.init(threads)
	.name(name + ":rdy_inst")
	.desc("Number of ready instructions (cum)")
	.flags(total)
	;

    trans_ready_count
	.init(threads)
	.name(name + ":rdy_x_count")
	.desc("number of insts that become ready (cum)")
	.flags(total)
	;

    max_trans_ready
	.name(name + ":rdy_x_max")
	.desc("largest number of insts that become ready")
	;

    rdy_inst_stdev_stat
	.init(threads)
	.name(name + ":rdy_inst_dist")
	.desc("standard deviation of ready rate")
	.flags(total)
	;

    for (int t = 0; t < threads; ++t) {
	ready_inst_squares[t] = 0;
    }

    total_rdy_inst = 0;
    total_rdy_squares = 0;
}

void
ready_queue_base_base_t::regFormulas(string name, unsigned threads)
{
    using namespace Stats;

    rdy_rate
	.name(name + ":rdy_rate")
	.desc("Number of ready insts per cycle")
	.flags(total)
	;
    rdy_rate = ready_inst / (*cpu_ptr_ptr)->numCycles;

    rdy_x_rate
	.name(name + ":rdy_x_rate")
	.desc("number of insts that become ready per cycle")
	.flags(total)
	;
    rdy_x_rate = trans_ready_count / (*cpu_ptr_ptr)->numCycles;
}

void
ready_queue_base_base_t::dump()
{
    cprintf("======================================================\n"
	    "%s Dump (cycle %n)\n"
	    "------------------------------------------------------\n"
	    "  Total instruction: %u\n", name, curTick, total_insts);

    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cprintf("  Thread %d instructions: %u\n", i, thread_insts[i]);

    cout << "------------------------------------------------------\n";
    queue_dump();
    cout << "======================================================\n\n";
}

void
ready_queue_base_base_t::raw_dump()
{
    cprintf("======================================================\n"
	    "%s RAW Dump (cycle %n)\n"
	    "------------------------------------------------------\n"
	    "  Total instruction: %u\n", name, curTick, total_insts);

    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cprintf("  Thread %d instructions: %u\n", i, thread_insts[i]);

    cout << "------------------------------------------------------\n";
    queue_raw_dump();
    cout << "======================================================\n\n";
}


//=======================================================================



//
//  Constructor
//
template<class T>
ready_queue_base_t<T>::ready_queue_base_t(FullCPU** cpu, string n,
					  unsigned size)
    : ready_queue_base_base_t(cpu, n)
{
    queue = new res_list<typename res_list<T>::iterator>(size, true, 4);
}


template<class T>
void
ready_queue_base_t<T>::resort()
{
    res_list<typename res_list<T>::iterator> *old_queue;

    //  We'll need this later
    old_queue = queue;

    //  Create a new list for the new queue
    queue = new res_list<typename res_list<T>::iterator>(20, true, 4);

    //  Walk the old queue, placing the entries into the new queue
    //  in the correct order
    for (iterator i = old_queue->head(); i.notnull(); i = i.next()) {
	//  use the internal routine so that we don't mess up the
	//  counters, etc.
	(*i)->rq_entry = internal_enqueue(*i);
    }

    //  Remove the old queue
    delete old_queue;
}




//=======================================================================



//
// template instantiation
//
#define INSTANTIATE_READYQ_BASE(T)					\
template void ready_queue_base_t<T>::resort();				\
template ready_queue_base_t<T>::ready_queue_base_t(FullCPU **c, string n,   \
    unsigned size);

INSTANTIATE_READYQ_BASE(IQStation)
INSTANTIATE_READYQ_BASE(StoreBufferEntry)

