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

#include <cassert>
#include <string>
#include <vector>

#include "base/cprintf.hh"
#include "base/statistics.hh"
#include "base/trace.hh"
#include "cpu/exec_context.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/readyq.hh"
#include "encumbered/cpu/full/storebuffer.hh"
#include "sim/stats.hh"

using namespace std;

void
StoreBufferEntry::dump()
{
    cprintf("%d: T%d PC=%#x EA=%#x\t%s%s%s\n",
	    seq, thread, pc, virt_addr,
	    FLAG_STRING(queued), FLAG_STRING(issued),
	    FLAG_STRING(writeBarrierFlag));
}


////////////////////////////////////////////////////////////////////////////


StoreBuffer::StoreBuffer(FullCPU *c, const string &n, unsigned _size)
    : cpu(c), _name(n), size(_size)
{
    queue = new res_list<StoreBufferEntry>(size, true, 0);

    ready_list = new ready_queue_t<StoreBufferEntry,
	storebuffer_readyq_policy>(&cpu, name() + ":RQ", true);

    for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	insts[i] = 0;
	writeBarrierPending[i] = false;
    }
}

void
StoreBuffer::tick()
{
    //  Do stats
    for (int i = 0; i < cpu->number_of_threads; ++i) {
	occupancy[i] += insts[i];
	if (writeBarrierPending[i]) {
	    ++writeBarrierPendingCycles[i];
	}
    }

    if (full()) {
	++full_count;
    }
}

void
StoreBuffer::regStats()
{
    using namespace Stats;

    ready_list->regStats(name() + ":RQ", cpu->number_of_threads);

    occupancy
	.init(cpu->number_of_threads)
	.name(name() + ":occupancy")
	.desc("store buffer occupancy (cumulative)")
	.flags(total)
	;

    full_count
	.name(name() + ":full_count")
	.desc("store buffer full events (cumulative)")
	;

    writeBarriers
	.init(cpu->number_of_threads)
	.name(name() + ":write_barriers")
	.desc("number of write barrier operations")
	.flags(total)
	;

    writeBarrierPendingCycles
	.init(cpu->number_of_threads)
	.name(name() + ":write_barrier_pending_cycles")
	.desc("number of cycles write barriers were pending")
	;
}

void
StoreBuffer::regFormulas(FullCPU *cpu)
{
    using namespace Stats;

    ready_list->regFormulas(name() + ":RQ", cpu->number_of_threads);
    occ_rate
	.name(name() + ":occ_rate")
	.desc("store buffer occupancy rate")
	.flags(total)
	;
    occ_rate = occupancy / cpu->numCycles;

/** @todo Is this the right formula occ/sec/sec?? */
    full_rate
	.name(name() + ":full_rate")
	.desc("store buffer full rate")
	;
    full_rate = full_count / cpu->numCycles;
}

bool
StoreBuffer::add(unsigned thread_number, unsigned asid, int size,
		 IntReg data,
		 ExecContext *xc, Addr vaddr, Addr paddr,
		 unsigned mem_req_flags, Addr pc, InstSeqNum seq,
		 InstSeqNum fseq, unsigned queue_num)
{
    if (full())
	return false;

    iterator p = queue->add_tail();

    p->init_entry(thread_number, asid, size, xc,
		  vaddr, paddr, mem_req_flags, pc, seq, fseq,
		  queue_num);
    p->data = new uint8_t[size];
    memcpy(p->data, &data, size);

    ++insts[thread_number];

    if (!writeBarrierPending[thread_number]) {
	//  Queue it immediately if there's no pending barrier
	ready_list_enqueue(p);
    } else {
	DPRINTF(WriteBarrier, "deferring store seq %d T%d due to wmb\n",
		p->seq, thread_number);
    }

    return true;
}

bool
StoreBuffer::addCopy(unsigned thread_number, unsigned asid, int size,
		     ExecContext *xc, Addr vaddr, Addr paddr,
		     Addr src_vaddr, Addr src_paddr,
		     unsigned mem_req_flags, Addr pc, InstSeqNum seq,
		     InstSeqNum fseq, unsigned queue_num)
{
    if (full())
	return false;

    iterator p = queue->add_tail();

    p->initCopy(thread_number, asid, size, xc, vaddr, paddr, src_vaddr, 
		src_paddr, mem_req_flags, pc, seq, fseq, queue_num);

    ++insts[thread_number];

    if (!writeBarrierPending[thread_number]) {
	//  Queue it immediately if there's no pending barrier
	ready_list_enqueue(p);
    } else {
	DPRINTF(WriteBarrier, "deferring store seq %d T%d due to wmb\n",
		p->seq, thread_number);
    }

    return true;
}

StoreBuffer::iterator
StoreBuffer::findPrevInThread(int thread, iterator start)
{
    // handle default case: start from tail
    iterator p = start.notnull() ? start.prev() : queue->tail();
    for (; p.notnull(); p = p.prev()) {
	if (p->thread == thread) {
	    return p;
	}
    }    

    return 0;
}


void
StoreBuffer::addWriteBarrier(int thread)
{
    // Look for the most recent write from this thread.
    iterator entry = findPrevInThread(thread);

    if (entry.notnull()) {
	// Found... mark it as the barrier, and set the barrier
	// flag on this thread to keep new stores from being
	// marked ready.
	DPRINTF(WriteBarrier, "wmb T%d: marking seq %d\n", thread, entry->seq);
	entry->writeBarrierFlag = true;
	writeBarrierPending[thread] = true;
    } else {
	// Not found: no outstanding writes from this thread in the
	// buffer.  The write barrier is just a no-op then.
	DPRINTF(WriteBarrier, "wmb T%d: store buffer empty\n", thread);
    }

    ++writeBarriers[thread];
}


StoreBuffer::iterator
StoreBuffer::remove(iterator e)
{
    iterator next = 0;

    if (e.notnull()) {
	next = e.next();

	--insts[e->thread_number()];

	queue->remove(e);

	assert(!e->queued);
	if (e->data) {
	    delete [] e->data;
	}
    }

    return next;
}


//  For debugging purposes, we don't want these functions inlined

void
StoreBuffer::dump()
{
    cprintf("======================================================\n"
	    "%s Dump (cycle %n)\n"
	    "------------------------------------------------------\n"
	    "  Total instruction: %u\n", name(), curTick, queue->count());

    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cprintf("  Thread %d instructions: %u\n", i, insts[i]);

    cout << "------------------------------------------------------\n";
    queue->dump();
    cout << "======================================================\n\n";
}

void
StoreBuffer::raw_dump()
{
    cprintf("======================================================\n"
	    "%s RAW Dump (cycle %n)\n"
	    "------------------------------------------------------\n"
	    "  Total instruction: %u\n", name(), curTick, queue->count());

    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cprintf("  Thread %d instructions: %u\n", i, insts[i]);

    cout << "------------------------------------------------------\n";
    queue->raw_dump();
    cout << "======================================================\n\n";
}

void
StoreBuffer::rq_raw_dump()
{
    ready_list->raw_dump();
}



////////////////////////////////////////////////////////////////////////////


void
StoreBuffer::completeStore(iterator entry)
{
    assert(entry->issued);

    if (entry->writeBarrierFlag) {
	// There's a write barrier after this write. If there are
	// older writes outstanding, transfer the barrier flag to the
	// next older write.  If not, then the barrier is satisfied,
	// and we can mark all younger writes as ready (up to the next
	// barrier, if any).
	
	DPRINTF(WriteBarrier, "wmb store seq %d completed\n", entry->seq);

	int thread = entry->thread;
	iterator p = findPrevInThread(thread, entry);
	if (p.notnull()) {
	    DPRINTF(WriteBarrier, "wmb moved to seq %d\n", p->seq);
	    p->writeBarrierFlag = true;
	}
	else {
	    bool stillPending = false; // assume we're going to clear flag
	    p = entry.next();
	    while (p.notnull()) {
		if (p->thread == thread) {
		    DPRINTF(WriteBarrier, "waking store seq %d\n", p->seq);
		    ready_list_enqueue(p);
		    if (p->writeBarrierFlag) {
			// another barrier: stop waking up stores,
			// and don't clear thread flag either
			DPRINTF(WriteBarrier, "wmb encountered seq %d\n",
				p->seq);
			stillPending = true;
			break;
		    }
		}
		p = p.next();
	    }

	    assert(writeBarrierPending[thread]);
	    writeBarrierPending[thread] = stillPending;
	}
    }

    remove(entry);
}


void
SimCompleteStoreEvent::process()
{
    sb->completeStore(entry);
}


const char *
SimCompleteStoreEvent::description()
{
    return "store completion";
}
