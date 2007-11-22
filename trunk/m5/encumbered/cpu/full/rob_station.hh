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

/*
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 *
 * Copyright (C) 1997, 1998 by Todd M. Austin
 *
 * This source file is distributed "as is" in the hope that it will be
 * useful.  The tool set comes with no warranty, and no author or
 * distributor accepts any responsibility for the consequences of its
 * use.
 *
 * Everyone is granted permission to copy, modify and redistribute
 * this tool set under the following conditions:
 *
 *    This source code is distributed for non-commercial use only.
 *    Please contact the maintainer for restrictions applying to
 *    commercial use.
 *
 *    Permission is granted to anyone to make or distribute copies
 *    of this source code, either as received or modified, in any
 *    medium, provided that all copyright notices, permission and
 *    nonwarranty notices are preserved, and that the distributor
 *    grants the recipient permission for further redistribution as
 *    permitted by this document.
 *
 *    Permission is granted to distribute this file in compiled
 *    or executable form under the same conditions that apply for
 *    source code, provided that either:
 *
 *    A. it is accompanied by the corresponding machine-readable
 *       source code,
 *    B. it is accompanied by a written offer, with no time limit,
 *       to give anyone a machine-readable copy of the corresponding
 *       source code in return for reimbursement of the cost of
 *       distribution.  This written offer must permit verbatim
 *       duplication by anyone, or
 *    C. it is distributed by someone who received only the
 *       executable form, and is accompanied by a copy of the
 *       written offer of source code that they received concurrently.
 *
 * In other words, you are welcome to use, share and improve this
 * source file.  You are forbidden to forbid anyone else to use, share
 * and improve what you give them.
 */

#ifndef __ENCUMBERED_CPU_FULL_ROB_STATION_HH__
#define __ENCUMBERED_CPU_FULL_ROB_STATION_HH__

#include <vector>


#include "cpu/inst_seq.hh"
#include "encumbered/cpu/full/cv_spec_state.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/ls_queue.hh"
#include "encumbered/cpu/full/machine_queue.hh"
#include "sim/eventq.hh"

class FullCPU;

class WritebackEvent;
class DelayedWritebackEvent;

struct ROBStation : public MachineQueueEntry
{
    DynInst *inst;
    bool eaCompPending;		// mem refs only: is effective address
				// computation pending (not complete)? 
				// will be false for non-mem-ref insts
    bool squashed;		/* operation has been squashed */
    bool issued;		/* operation is/was executing */
    bool completed;		/* operation has completed execution */

    //
    //  Dispatch-Chainer support...
    //
    unsigned chain_id;
    int chainer_lr_prediction;  // used by chainer (NOT segmented IQ)
    int last_op;                // last op to became ready

    unsigned queue_num;   // index to the IQ where this inst lived

    unsigned output_reg;  // used by the dispatch chainer...

    //  initialization when machine-queue is created
    void init(unsigned num_iqueues);

    //  initialized at each instruction's dispatch
    void init(DynInst *inst, InstSeqNum dispatch_seq, unsigned n);

    void setMemOp(BaseIQ::iterator lsq);

    class BranchRecoveryEvent *recovery_event;
    //    CreateVecSpecState *spec_state;
    SpecStateList::ss_iterator spec_state;

    load_store_queue::iterator lsq_entry;

    BaseIQ::iterator iq_entry;

    InstSeqNum seq;		/* instruction sequence, used to
				 * sort the ready list and tag inst */

    WritebackEvent *wb_event;
    DelayedWritebackEvent *delayed_wb_event;

    Tick due_to_complete;
    Tick pred_issue_cycle;
    Tick pred_wb_cycle;

    MemAccessResult mem_result;

    // output operand dependency list, these lists are used to limit
    // the number of associative searches into the IQ when
    // instructions complete and need to wake up dependent insts
    int num_outputs;
    int onames[TheISA::MaxInstDestRegs];	// output logical
						// names (NA=unused)
#if 0
    // chains to consuming operations
    DepLink **odep_list[TheISA::MaxInstDestRegs];
#endif

    std::vector< std::vector<DepLink *> > odep_list;

    // write back completed result to this entry...
    // returns the number of consumers written-back to...
    //  (CODE IS IN writeback.cc)
    unsigned writeback(FullCPU *cpu, unsigned q);

    //  squash this entry
    void squash();

    // print contents for debugging
    void dump();
    void dump_odeps();
    void dump_odep_list(int);


    ///////////////////////////////////////////////////////////////////
    //
    //  This information only used by the segmented IQ
    //
    bool head_of_chain;
    unsigned head_chain;
    int hm_prediction;
    unsigned pdata;

    Event *cache_event_ptr;

    ROBStation() : inst(0), eaCompPending(false),
			squashed(false), issued(false), completed(false),
			recovery_event(0), spec_state(0), lsq_entry(0),
			seq(0), due_to_complete(INT_MAX),
			// [DAG] I think these are only used by the segmented
			// IQ so initialize them to zero to prevent gratuitous
			// smt-test errors.
			pred_issue_cycle(0), pred_wb_cycle(0),
			mem_result(MA_NOT_ISSUED) {};

    virtual ~ROBStation() {};
};

#endif // __ENCUMBERED_CPU_FULL_ROB_STATION_HH__
