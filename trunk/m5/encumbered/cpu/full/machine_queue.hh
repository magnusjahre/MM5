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

#ifndef __ENCUMBERED_CPU_FULL_MACHINE_QUEUE_HH__
#define __ENCUMBERED_CPU_FULL_MACHINE_QUEUE_HH__

#include <cassert>

#include "base/dbl_list.hh"
#include "cpu/inst_seq.hh"
#include "cpu/smt.hh"

class FullCPU;

class MachineQueueEntry : public DblListEl
{
  public:
    MachineQueueEntry();

    // free_list uses a separate pointer from the DblListEl pointers
    // so we don't have to worry about using prev & next after element
    // is freed (within reason)
    MachineQueueEntry *next_free;

    // thread_number is kept here so MachineQueue can keep
    // thread-based occupancy counts
    int thread_number;

    // sequence number for RS_link: increment to invalidate all
    // current links to this entry.  Also used by memory system to
    // detect callbacks on squashed loads.
    InstTag tag;

#if 0
    void copy_to(MachineQueueEntry *e) {
	//  Don't copy *next_free
	e->thread_number = thread_number;
	e->tag = tag;
    }
#endif

    // print contents for debugging
    virtual void dump() {}

    virtual ~MachineQueueEntry() {}
};


//
// MachineQueueBase enhances DblList<MachineQueueEntry> to include:
//
// - static allocation of an array of elements (the hardware queue)
// - membership checking based on bounds of said array
// - maintenance of a free list of all array elements not
//   on the DblList
// - occupancy tracking on a per-thread basis
// - management of per-entry instance tags, used to check stale
//   references to squashed entries (inherited from good old
//   SimpleScalar RS_LINK code)
// - diagnostic dumping of queue contents using dump()
//   method on MachineQueueEntry objects
//
class MachineQueueBase : public DblList<MachineQueueEntry>
{
  protected:

    // pointer to containing CPU
    FullCPU *cpu;

    // pointer to start of array of elements (derived from
    // MachineQueueEntry).  Don't subscript this, as the actual array
    // is of a derived (and thus larger) type!
    MachineQueueEntry *vec_start;

    // pointer to end of array (for membership tests)
    MachineQueueEntry *vec_end;

    // list of free queue entries (through next_free pointer)
    MachineQueueEntry *free_list;

    unsigned number_active;
    unsigned number_free;

    unsigned number_thread[SMT_MAX_THREADS];

    MachineQueueEntry *new_tail(int thread) {
	// get new element off of free list
	assert(free_list != NULL);
	MachineQueueEntry *new_el = free_list;
	free_list = free_list->next_free;

	// link in as tail
	append(new_el);

	number_active++;
	number_thread[thread]++;
	number_free--;

	// bump tag just to be sure stale links/loads are detected
	new_el->tag++;
	new_el->thread_number = thread;

	return new_el;
    }

  public:

    void dump();

    void check();

    bool is_member(MachineQueueEntry *el) {
	return (el == NULL || (vec_start <= el && el < vec_end));
    }

    void remove(MachineQueueEntry *el) {
	assert(is_member(el));

	// unlink from queue
	DblList<MachineQueueEntry>::remove(el);

	// push onto free list
	el->next_free = free_list;
	free_list = el;

	number_active--;
	number_thread[el->thread_number]--;
	number_free++;
    }

    unsigned num_free() { return number_free; }
    unsigned num_active() { return number_active; }
    unsigned num_thread(int thread) { return number_thread[thread]; }
};


//
// actual MachineQueue that implements a queue of things of type T.
// T must be derived from MachineQueueEntry.
//
template<class T> class MachineQueue : public MachineQueueBase {

  public:

    // initialization: argument specifies total # of entries
    void init(FullCPU *cpu, unsigned size);
    void init(FullCPU *cpu, unsigned size, unsigned init_param);

    //
    // these functions are inherited directly from MachineQueueBase,
    // but we can safely cast the return values to T* because that's
    // the only type we have in this queue
    //
    T *head() { return (T *)MachineQueueBase::head(); }
    T *tail() { return (T *)MachineQueueBase::tail(); }
    T *next(T *el) { return (T *)MachineQueueBase::next(el); }
    T *prev(T *el) { return (T *)MachineQueueBase::prev(el); }
    T *new_tail(int thread) { return (T *)MachineQueueBase::new_tail(thread); }

    T *get(unsigned index) {
	T *el;

	unsigned i = 0;
	for (el=head(); el != NULL; el=next(el), ++i) {
	    if (i == index) {
		return el;
	    }
	}

	return 0;
    }

    T *operator[](unsigned index) {
	T *el;

	unsigned i = 0;
	for (el=head(); el != NULL; el=next(el), ++i) {
	    if (i == index) {
		return el;
	    }
	}

	return 0;
    }
};

#endif // __ENCUMBERED_CPU_FULL_MACHINE_QUEUE_HH__
