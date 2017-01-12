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

#ifndef __ENCUMBERED_CPU_FULL_IQ_IQ_STATION_HH__
#define __ENCUMBERED_CPU_FULL_IQ_IQ_STATION_HH__

#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"

/*
 * input dependencies for loads & stores in the LSQ:
 *   idep #0 - effective address input
 *   idep #1 - operand input (store data value for stores, nothing for loads)
 */
#define MEM_ADDR_INDEX		0
#define STORE_DATA_INDEX	1

#define STORE_DATA_READY(RS)	((RS)->idep_ready[STORE_DATA_INDEX])
#define STORE_ADDR_READY(RS)	((RS)->idep_ready[MEM_ADDR_INDEX])

struct DepLink;
struct ROBStation;

struct IQStation
{
    typedef res_list<IQStation>::iterator iterator;
    typedef res_list< iterator >::iterator list_iterator;

    struct IDEP_info
    {
	bool chained;               // This inst follows a chain
	unsigned follows_chain;     // Chain number this producer follows
	unsigned delay;             // Number of cycles behind the head
	list_iterator chain_entry;  // Iterator to the entry in the chain list
	unsigned chain_depth;

	unsigned source_cluster;

	Tick op_pred_ready_time;

	IDEP_info() {
	    chained = false;
	    follows_chain = 0;
	    delay = 0;
	    chain_entry = 0;
	    chain_depth = 0;
	    op_pred_ready_time = 0;
	    source_cluster = 0;
	}
    };


    IQStation() {
	in_LSQ = ea_comp = false;
	queued = squashed = false;
	//  blocked = false;

	lsq_entry = iq_entry = 0;
	rob_entry = 0;
	rq_entry = 0;

	seq = 0;
	tag = 0;

	dispatch_timestamp = ready_timestamp = pred_issue_cycle = 0;

	for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	    idep_ptr[i] = 0;
	    idep_reg[i] = 0;
	    idep_ready[i] = false;
	}

	dependence_depth = 0;

	seg_init();
	sez_init();
    }

    /* inst info */
    struct DynInst *inst;
    bool in_LSQ;		/* non-zero if op is in LSQ */
    bool ea_comp;		/* non-zero if op is an addr comp */

    // If this is the EA-comp part of a mem reference, it really needs
    // an adder, not a mem port, so don't ask the DynInst object.
    OpClass opClass()
    {
	return ea_comp ? IntAluOp : inst->opClass();
    }

    /* instruction status */
    bool queued;		/* operands ready and queued */
    bool squashed;		/* operation has been squashed */
    //    bool blocked;

    MemAccessResult mem_result;

    iterator lsq_entry;
    ROBStation *rob_entry;
    iterator iq_entry;
    list_iterator rq_entry;


    InstSeqNum seq;		/* instruction sequence, used to
				 * sort the ready list and tag inst */
    InstTag tag;          // for validity checking

    Tick dispatch_timestamp;
    Tick ready_timestamp;
    Tick pred_issue_cycle;

    int hm_prediction;

    /* input dependent links, the output chains rooted above use these
     * fields to mark input operands as ready, when all these fields have
     * been set non-zero, the IQ operation has all of its register
     * operands, it may commence execution as soon as all of its memory
     * operands are known to be read (see lsq_refresh() for details on
     * enforcing memory dependencies) */
    int num_ideps;
    DepLink *idep_ptr[TheISA::MaxInstSrcRegs];	// links to producing ops
    int idep_ready[TheISA::MaxInstSrcRegs];	// input operand ready?
    // arch reg idx: looks like it's unused in the code, but left here for
    // debugging purposes
    TheISA::RegIndex idep_reg[TheISA::MaxInstSrcRegs];

    bool ops_ready();

    int dependence_depth;

    unsigned thread_number() {return inst->thread_number;}

    void dump(int length=1);

    bool high_priority() {
	return (in_LSQ || inst->isControl());
	// was (MD_OP_FLAGS(op) & (F_LONGLAT|F_CTRL)))
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //   These data elements are used in the Segmented IQ
    //
    //   (see notes below for the Seznec IQ, also)
    //
    ////////////////////////////////////////////////////////////////////////
    unsigned segment_number;        // we need to know where we are...
    list_iterator queue_entry;

    IDEP_info idep_info[TheISA::MaxInstSrcRegs];

    bool head_of_chain;         // This inst the the "head" of a chain
    unsigned head_chain;        // Number of this chain
    list_iterator head_entry;   // Iterator to entry in chain this inst heads

    unsigned dest_seg;

    Tick pred_ready_time;     // Based on the self-timer value
    Tick seg0_entry_time;

    Tick st_zero_time;        // When does the self-timer go to zero

    Tick first_op_ready;

    int pred_last_op_index;
    int lr_prediction;

    void seg_init() {
	segment_number = 0;
	queue_entry = 0;

	head_of_chain = false;
	head_chain = 0;
	head_entry = 0;

	pred_ready_time =
	    seg0_entry_time =
	    first_op_ready =
	    st_zero_time = 0;
    }

    unsigned max_delay() {
	unsigned max = 0;

	for (int i = 0; i < num_ideps; ++i)
	    if (max < idep_info[i].delay)
		max = idep_info[i].delay;
	return max;
    }

    void iq_segmented_dump(int length = 1);
    void iq_segmented_short_dump();

    ////////////////////////////////////////////////////////////////////////
    //
    //   For the Seznec IQ
    //
    ////////////////////////////////////////////////////////////////////////

    bool in_issue_buffer;
    unsigned presched_line;

    //  we re-use the queue_entry member

    void sez_init() {
	in_issue_buffer = false;
	presched_line = 0;
    }
};


#endif // __ENCUMBERED_CPU_FULL_IQ_IQ_STATIOIN_H__
