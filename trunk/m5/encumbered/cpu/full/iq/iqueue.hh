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

#ifndef __ENCUMBERED_CPU_FULL_IQ_IQUEUE_HH__
#define __ENCUMBERED_CPU_FULL_IQ_IQUEUE_HH__

/*
 *  @file
 *  The abstract base class for instruction queues and the load-store queue
 *
 */
#include <vector>

#include "base/res_list.hh"
#include "base/statistics.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/readyq.hh"
#include "encumbered/cpu/full/reg_info.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"

// Forward declarations to break dependancy chains between header files
struct ROBStation;
struct IQStation;
class FullCPU;
struct NewChainInfo;
struct GenericPredictor;
class ChainInfoTableBase;

/*
 *  This structure holds information that needs to be shared by all
 *  instruction queues.  In the CPU constructor (after the IQ's have
 *  been created) IQ[0] is called to build the shared info via the
 *  buildSharedInfo() method. Then, the remaining (if any) IQ's are
 *  told about it via the setSharedInfo() method.
 */
struct ClusterSharedInfo
{
    //! Pointer to the register information table
    RegInfoTable * ri_table;

    //! Pointer to the chain information table
    ChainInfoTableBase * ci_table;

    //! The maximum number of chains available
    unsigned     total_chains;

    //  The number of chain-heads per thread
    unsigned 	 chain_heads[SMT_MAX_THREADS];

    //! The load hit/miss predictor
    GenericPredictor *hm_predictor;

    //! The left/right operand predictor
    GenericPredictor *lr_predictor;


    //! Constructor
    ClusterSharedInfo() {
	//
	//  We know how to build the Register Info Table
	//
	ri_table = new RegInfoTable;
	ri_table->init(SMT_MAX_THREADS, TotalNumRegs);

	//
	// If you want this info, build it yourself...
	//
	ci_table = 0;

	for (int i=0; i<SMT_MAX_THREADS; ++i) {
	    chain_heads[i] = 0;
	}

	hm_predictor = 0;
	lr_predictor = 0;
    }
};


//  The abstract base class from which all Instruction Queue objects
//  and the load-store queue object dereive
class BaseIQ : public SimObject
{
  public:
    //!  Iterator to the instructions in the queue
    typedef res_list<IQStation>::iterator iterator;
    //!  Ready queue iterator.
    //!  This is an iterator into a list of instruction iterators
    typedef ready_queue_base_t<IQStation>::iterator rq_iterator;

    enum IQType {
	Standard,
	Segmented,
	LSQ,
	Seznec
    };

  private:
    //!  The capacity of the queue
    unsigned max_num_insts;

    //!  Queue identifier. In a clustered machine, indicates which cluster
    //!  this queue is part of
    unsigned queue_num;

    //!  Queue occupancy-cap values
    std::vector<unsigned> cap_values;

    //
    //  Statistics
    //
    Stats::VectorDistribution<> occ_dist;

    Stats::Vector<> inst_count;
    Stats::Vector<> peak_inst_count;
    Stats::Scalar<> empty_count;
    Stats::Scalar<> current_count;
    Stats::Scalar<> fullCount;

    Stats::Formula occ_rate;
    Stats::Formula avg_residency;
    Stats::Formula empty_rate;
    Stats::Formula full_rate;

    Tick lastFullMark;

    Tick last_cycle_add;
    Tick last_cycle_issue;
    unsigned avail_add_bw;
    unsigned avail_issue_bw;
    unsigned local_dispatch_width;
    unsigned local_issue_width;

    //  These should be implemented by the sub-classed model
    virtual void regModelStats(unsigned num_threads) {};
    virtual void regModelFormulas(unsigned num_threads) {};
    virtual void tick_model_stats() {};

    //  Implementation-specific add-to-IQ method
    virtual iterator add_impl(DynInst *, InstSeqNum seq,
			      ROBStation *, RegInfoElement *,
			      NewChainInfo *) = 0;

    //  Implementation-specific add-to-IQ method
    virtual rq_iterator issue_impl(rq_iterator &) = 0;

  protected:
    void set_size(unsigned size) {max_num_insts = size;};

  public:

    // backpointer to CPU this IQ is part of
    FullCPU *cpu;

    ClusterSharedInfo *shared_info;

    unsigned size() {return max_num_insts;};

    //  Add an instruction to the queue
    iterator add(DynInst *inst, InstSeqNum seq, ROBStation *rob,
		 RegInfoElement *, NewChainInfo *new_chain);

    virtual void registerLSQ(iterator &p, iterator &lsq);

    unsigned add_bw() { return avail_add_bw; }
    unsigned issue_bw() { return avail_issue_bw; }

    virtual IQType type() = 0;

    virtual void set_cache_hit_latency(unsigned lat) {};

    virtual ClusterSharedInfo * buildSharedInfo();
    virtual void setSharedInfo(ClusterSharedInfo *p) {shared_info = p;}

    //  Remove an instruction from the queue
    virtual iterator squash(iterator &) = 0;

    //  Broadcast result of executed instruction back to IQ
    virtual unsigned writeback(ROBStation *, unsigned queue_num) = 0;

    //  Register necessary statistics
    void regStats(unsigned threads);
    void regFormulas(unsigned threads);

    //  Get list of issuable instructions. The IQ must be notified of
    //  instructions chosen for issue via a call to issue()
    //
    //  The issuable list is allocated by the call to issuable_list()
    //  and will remain valid until the next time this call is made
    virtual rq_iterator issuable_list() = 0;

    //
    //  Handle the removal of the specified instruction from both
    //  the instruction queue and the ready queue
    //
    rq_iterator issue(rq_iterator &);

    //  For things which must be done every cycle
    virtual void tick() {};      // between issue and dispatch
    void pre_tick();             // before _anything_ in a cycle
    void tick_stats();
    virtual void tick_ready_stats() {};

    //  Returns the number of available slots for new instructions
    virtual unsigned free_slots() = 0;

    //  Returns count of all instructions, or count of a particular thread
    virtual unsigned count() = 0;
    virtual unsigned count(unsigned t) = 0;

    // number of instructions in the issue-window
    virtual unsigned iw_count() {return count();}
    virtual unsigned iw_count(unsigned t) {return count(t);}

    virtual void set_cap(unsigned thread, unsigned cap_val) {
	cap_values[thread] = cap_val;
    }
    virtual bool cap_met(unsigned thread) {
	return count(thread) >= cap_values[thread];
    }

    virtual void dump() = 0;
    virtual void dump(int mode) { dump(); }
    virtual void raw_dump() {};
    virtual void rq_dump() {};
    virtual void rq_raw_dump() {};

    //  Provide some method of moving through the queue...
    //
    //  NOTE:  You should assume NOTHING about the order of the elements
    //         returned by these methods!
    //
    virtual iterator head() = 0;
    virtual iterator tail() = 0;

    virtual iterator oldest() = 0;
    virtual iterator oldest(unsigned thread) = 0;

    virtual void ready_list_enqueue(iterator &) = 0;
    virtual unsigned ready_count() = 0;

    Tick link_idep(iterator &i, TheISA::RegIndex reg);
    // Degenerate version for ea_comp->lsq dependences
    Tick link_idep(iterator &i);

    //  Allow the queue to be informed that an instruction was issued
    virtual void inform_dispatch(iterator i) {};
    virtual void inform_issue(iterator i) {};
    virtual void inform_squash(ROBStation *) {};

    virtual unsigned proper_segment(iterator &) {return 0;}

    // constructor
    BaseIQ(const std::string &name);
    BaseIQ(const std::string &name, bool _caps_valid,
	   std::vector<unsigned> _caps);

    //  We need a virtual destructor!
    virtual ~BaseIQ() {};

    // to get the values into the queue after creation
    virtual void init(FullCPU *_cpu, unsigned dw, unsigned iw, unsigned qn) {
	cpu = _cpu;

	local_dispatch_width = dw;
	local_issue_width = iw;

	queue_num = qn;
    }

    unsigned thisCluster() { return queue_num; }

    class CacheMissEvent : public Event {
	ROBStation *rob_entry;  // this may point to a squashed instruction!
	BaseIQ *iq_ptr;

	Addr pc;
	int       hm_prediction;

      public:
	// constructor
	CacheMissEvent(ROBStation *rob, BaseIQ *iq);

	//  This function will be called on a data-cache miss
	virtual void process();

	virtual const char *description();

	// inline destructor for FastAlloc
	~CacheMissEvent() {}
    };

    virtual CacheMissEvent *new_cm_event(ROBStation *) {return 0;}
    virtual void cachemissevent_handler(Addr pc, int pred,
					ROBStation *rob, bool squashed,
					unsigned annot) {};

    virtual void reg_dump(unsigned thread, unsigned reg) {}

    void markFull() {
	if (curTick > lastFullMark) {
	    ++fullCount;
	    lastFullMark = curTick;
	}
    }
};

#endif // __ENCUMBERED_CPU_FULL_IQ_IQUEUE_HH__
