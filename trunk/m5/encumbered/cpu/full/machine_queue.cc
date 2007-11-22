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
#include <string>

#include "base/cprintf.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/machine_queue.hh"
#include "encumbered/cpu/full/rob_station.hh"

MachineQueueEntry::MachineQueueEntry()
    : next_free(0), thread_number(0), tag(0)
{
}

//
// for debugging
//
void
MachineQueueBase::dump()
{
    int i;
    MachineQueueEntry *el;

    cprintf("num_active: %d\n", number_active);
    for (i = 0; i < cpu->number_of_threads; ++i)
	cprintf("  num_thread[%d]: %d\n", i, number_thread[i]);

    for (i = 0, el = head(); el != NULL; el = next(el), ++i) {
	cprintf("[%d]: ", i);
	el->dump();
    }
}


template<class T> void
MachineQueue<T>::init(FullCPU *_cpu, unsigned size)
{
    cpu = _cpu;

    // allocate array of entries
    T *vec = new T[size];
    // remember start & end pointer for membership tests
    vec_start = &vec[0];
    vec_end = &vec[size];

    // put entries on the free list
    free_list = &vec[0];

    for (unsigned int i = 0; i < size - 1; ++i)
	vec[i].next_free = &vec[i + 1];

    number_free = size;
    number_active = 0;

    for (unsigned int i = 0; i < SMT_MAX_THREADS; ++i)
	number_thread[i] = 0;
}

template<class T> void
MachineQueue<T>::init(FullCPU *_cpu, unsigned size, unsigned init_param)
{
    cpu = _cpu;

    // allocate array of entries
    T *vec = new T[size];
    // remember start & end pointer for membership tests
    vec_start = &vec[0];
    vec_end = &vec[size];

    // put entries on the free list
    free_list = &vec[0];

    for (unsigned int i = 0; i < size - 1; ++i)
	vec[i].next_free = &vec[i + 1];

    for (unsigned int i = 0; i < size; ++i) {
	// do per-enty initilization here
	vec[i].init(init_param);
    }

    number_free = size;
    number_active = 0;

    for (unsigned int i = 0; i < SMT_MAX_THREADS; ++i)
	number_thread[i] = 0;
}


//
// sanity checker... for debugging, call this liberally
//
void
MachineQueueBase::check()
{
    unsigned i = 0;
    MachineQueueEntry *el;

    assert(is_member(head()));
    assert(is_member(tail()));
    assert(is_member(free_list));

    for (el = free_list; el != NULL; el = el->next_free) {
	assert(is_member(el));
	assert(++i <= number_free);
    }
    assert(i == number_free);

    i = 0;
    for (el = head(); el != NULL; el = next(el)) {
	assert(is_member(el));
	assert(++i <= number_active);
	assert(is_member(el->next_free));
    }
    assert(i == number_active);
}

template void
MachineQueue<ROBStation>::init(FullCPU *, unsigned, unsigned);
