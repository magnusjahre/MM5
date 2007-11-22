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

#ifndef __ENCUMBERED_CPU_FULL_STOREBUFFER_HH__
#define __ENCUMBERED_CPU_FULL_STOREBUFFER_HH__

#include <string>

#include "base/res_list.hh"
#include "base/statistics.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/readyq.hh"
#include "sim/eventq.hh"

#define FLAG_STRING(f)	((f) ? #f " " : "")

class FullCPU;
class StoreBuffer;
class storebuffer_readyq_policy;
class ExecContext;

class StoreBufferEntry {
  public:
    int thread;
    int asid;
    int size;

    uint8_t *data;

    ExecContext *xc;
    
    bool isCopy;
    
    Addr virt_addr;
    Addr phys_addr;
    Addr srcVirtAddr;
    Addr srcPhysAddr;
    unsigned mem_req_flags;
    Addr pc;
    bool queued;	// ready to issue: set as soon as added
    bool issued;	// been issued to cache yet?
    bool writeBarrierFlag; // enforce barrier after this write

    unsigned queue_num;

    InstSeqNum seq;
    InstSeqNum fetch_seq;

    res_list< res_list<StoreBufferEntry>::iterator >::iterator rq_entry;

    StoreBufferEntry()
	: thread(0), asid(0), size(0), data(0), xc(0), isCopy(false), 
	  virt_addr(0), phys_addr(0), srcVirtAddr(0), srcPhysAddr(0),
	  mem_req_flags(0), pc(0), queued(false), issued(false), 
	  writeBarrierFlag(false), seq(0), fetch_seq(0)
    {
    }

    void init_entry(unsigned thread_number, unsigned _asid, int _size,
		    ExecContext *_xc,
		    Addr _vaddr, Addr _paddr,
		    unsigned _mem_req_flags,
		    Addr _pc, InstSeqNum _seq,
		    InstSeqNum _fseq, unsigned _queue)
    {
	thread = thread_number;
	asid = _asid;
	size = _size;
	xc = _xc;
	virt_addr = _vaddr;
	phys_addr = _paddr;
	mem_req_flags = _mem_req_flags;
	pc = _pc;
	seq = _seq;
	fetch_seq = _fseq;

	queue_num = _queue;
	
	isCopy = false;
	issued = false;
	queued = false;
	writeBarrierFlag = false;
    }

    void initCopy(unsigned thread_number, unsigned _asid, int _size,
		   ExecContext *_xc,
		   Addr _vaddr, Addr _paddr,
		   Addr src_vaddr, Addr src_paddr,
		   unsigned _mem_req_flags,
		   Addr _pc, InstSeqNum _seq,
		   InstSeqNum _fseq, unsigned _queue)
    {
	init_entry(thread_number, _asid, _size, _xc, _vaddr, _paddr,
		   _mem_req_flags, _pc, _seq, _fseq, _queue);
	srcVirtAddr = src_vaddr;
	srcPhysAddr = src_paddr;
	isCopy = true;
    }

    unsigned thread_number() {return thread;}

    void dump();
};


class storebuffer_readyq_policy {
  public:
    static bool goes_before(FullCPU *cpu,
			    res_list<StoreBufferEntry>::iterator first,
			    res_list<StoreBufferEntry>::iterator second,
			    bool use_thread_priorities) {
	return (first->seq < second->seq);
    }
};



class StoreBuffer
{
  public:
    typedef res_list<StoreBufferEntry>::iterator iterator;
    typedef ready_queue_t<StoreBufferEntry,
			  storebuffer_readyq_policy>::iterator rq_iterator;

  private:
    FullCPU *cpu;

    std::string _name;
    unsigned size;

    res_list<StoreBufferEntry> *queue;
    ready_queue_t<StoreBufferEntry, storebuffer_readyq_policy> *ready_list;

    unsigned insts[SMT_MAX_THREADS];

    bool writeBarrierPending[SMT_MAX_THREADS];

    Stats::Vector<> occupancy;
    Stats::Scalar<> full_count;
    Stats::Vector<> writeBarriers;
    Stats::Vector<> writeBarrierPendingCycles;

    Stats::Formula occ_rate;
    Stats::Formula full_rate;

    // Find previous (older) store in buffer belonging to specified
    // thread.  Starts at tail unless 'start' is given specifying
    // alternate starting point.
    StoreBuffer::iterator findPrevInThread(int thread, iterator start = 0);

    // For DPRINTF
    const std::string name() const { return _name; }

  public:
    //
    //  Constructor
    //
    StoreBuffer(FullCPU *c, const std::string &n, unsigned _size);

    void regStats();
    void regFormulas(FullCPU *cpu);

    bool add(unsigned _thread_number, unsigned _asid, int _size,
	     IntReg data,
	     ExecContext *_xc,
	     Addr vaddr, Addr paddr,
	     unsigned mem_req_flags,
             Addr _pc, InstSeqNum _seq,
	     InstSeqNum _fseq, unsigned _queue);
    
    bool addCopy(unsigned _thread_number, unsigned _asid, int _size,
		 ExecContext *_xc,
		 Addr vaddr, Addr paddr,
		 Addr src_vaddr, Addr src_paddr,
		 unsigned mem_req_flags,
		 Addr _pc, InstSeqNum _seq,
		 InstSeqNum _fseq, unsigned _queue);

    // Insert a write barrier at this point in the buffer for the
    // specified thread.
    void addWriteBarrier(int thread);

    unsigned count() {return queue->count();}
    unsigned count(unsigned t) {return insts[t];}
    bool full() {return queue->full();};

    void ready_list_enqueue(iterator p)
    {
	p->rq_entry = ready_list->enqueue(p);
	p->queued = true;
    }

    rq_iterator issuable_list()
    {
	return ready_list->head();
    }

    //
    //  For SB: issue leaves entry in the queue
    //  (removed on completion)
    //
    rq_iterator issue(rq_iterator p)
    {
	assert((*p)->queued);
	(*p)->queued = false;
	return ready_list->remove((*p)->rq_entry);
    }

    iterator remove(iterator e);

    // mark store as complete
    void completeStore(iterator entry);

    //  Called AFTER events, BEFORE commit
    void pre_tick() {};

    void tick();

    void tick_stats() {};   //  don't have a use for this yet
    void tick_ready_stats() { ready_list->tick_stats(); }

    void dump();
    void raw_dump();
    void rq_raw_dump();
};


// Schedulable event to invoke sim_complete_store at a particular cycle
class SimCompleteStoreEvent : public Event
{
    // event data fields
    StoreBuffer *sb;
    StoreBuffer::iterator entry;
    MemReqPtr req;

  public:
    // constructor
    SimCompleteStoreEvent(StoreBuffer *_sb, StoreBuffer::iterator _entry,
			  MemReqPtr &_req)
	: Event(&mainEventQueue),
	  sb(_sb), entry(_entry), req(_req)
    {
	setFlags(AutoDelete);
    }

    // event execution function
    void process();

    virtual const char *description();
};

#endif // __ENCUMBERED_CPU_FULL_STOREBUFFER_HH__
