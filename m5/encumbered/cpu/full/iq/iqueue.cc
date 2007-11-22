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

#include <string>
#include <climits>

#include "base/statistics.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/create_vector.hh"
#include "encumbered/cpu/full/dep_link.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/reg_info.hh"
#include "encumbered/cpu/full/rob_station.hh"
#include "sim/param.hh"
#include "sim/stats.hh"

using namespace std;

//==========================================================================
//
//  These functions are common to all IQ types
//
//
//
//
//==========================================================================

BaseIQ::BaseIQ(const string &name)
    : SimObject(name)   // base class
{
    last_cycle_add = 0;
    last_cycle_issue = 0;

    lastFullMark = 0;

    //  no caps here...
    cap_values.reserve(SMT_MAX_THREADS);
    for (int i = 0; i < SMT_MAX_THREADS; ++i)
	cap_values[i] = UINT_MAX;
}

BaseIQ::BaseIQ(const string &name, bool caps_valid, vector<unsigned> caps)
    : SimObject(name)   // base class
{
    last_cycle_add = 0;
    last_cycle_issue = 0;

    lastFullMark = 0;

    cap_values.reserve(SMT_MAX_THREADS);
    for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	if (caps_valid) {
	    //  fill in values... empty & zeros => large positive numbers
	    if (i < caps.size() && caps[i] != 0)
		cap_values[i] = caps[i];
	    else
		cap_values[i] = UINT_MAX;  // very large positive number
	} else {
	    cap_values[i] = UINT_MAX;  // very large positive number
	}
    }
}


ClusterSharedInfo *
BaseIQ::buildSharedInfo()
{
    return new ClusterSharedInfo;
}

void
BaseIQ::registerLSQ(iterator &p, iterator &lsq) {
	p->lsq_entry = lsq;
}

BaseIQ::CacheMissEvent::CacheMissEvent(ROBStation *rob, BaseIQ *iq)
    : Event(&mainEventQueue)
{
    iq_ptr = iq;
    rob_entry = rob;

    pc = rob_entry->inst->PC;
    hm_prediction = rob_entry->hm_prediction;

    // When this event gets processed, or squashed, delete it.
    setFlags(AutoDelete);
}

void
BaseIQ::CacheMissEvent::process()
{
    iq_ptr->cachemissevent_handler(pc, hm_prediction,
				   rob_entry, squashed(), annotation());

    rob_entry->cache_event_ptr = 0;
}

const char *
BaseIQ::CacheMissEvent::description()
{
    return "cache miss notification";
}

//==========================================================================

void
BaseIQ::pre_tick()
{
    avail_add_bw = local_dispatch_width;
    avail_issue_bw = local_issue_width;
}


//  Add an instruction to the queue
BaseIQ::iterator
BaseIQ::add(DynInst *inst, InstSeqNum seq, ROBStation *rob,
	    RegInfoElement *ri, NewChainInfo *new_chain)
{
    BaseIQ::iterator rv;

    //  if dispatch_width is zero, don't worry about it...
    if (avail_add_bw > 0 || local_dispatch_width == 0) {
	//
	//  basic initialization of register info
	//
	if (ri != 0) {
	    for (unsigned i = 0; i < rob->num_outputs; ++i) {
		ri[i].clear();
		ri[i].setProducer(rob);
	    }
	}

	//  call the virtual function that is specific for this IQ type
	rv = add_impl(inst, seq, rob, ri, new_chain);
	if (rv.notnull())
	    --avail_add_bw;
    } else {
//	panic("BaseIQ: out of add bandwidth");
	rv = 0;
    }

    return rv;
}

//  Issue an instruction...
BaseIQ::rq_iterator
BaseIQ::issue(BaseIQ::rq_iterator &inst)
{
    BaseIQ::rq_iterator rv;

    //  if issue_width is zero, don't worry about it
    if (avail_issue_bw > 0 || local_issue_width == 0) {
	//  call the virtual function that is specific for this IQ type
	rv = issue_impl(inst);
	if (rv.notnull())
	    --avail_issue_bw;
    } else
	panic("BaseIQ: out of issue bandwidth");

    return rv;
}


//==========================================================================


/* link RS onto the output chain number of whichever operation will next
   create the architected register value IDEP_NAME */

// This would be more natural as an IQStation method rather than an
// IQ method, but that's not as easy as it sounds...

Tick
BaseIQ::link_idep(BaseIQ::iterator &i, TheISA::RegIndex reg)
{
    int number = i->num_ideps++;

    i->idep_reg[number] = reg;

    /* locate creator of operand */
    CVLink producer = cpu->create_vector[i->thread_number()].entry(reg);

    /* any creator? */
    if (!producer.rs) {
	/* no active creator, use value available in architected reg file,
	 * indicate the operand is ready for use */
	i->idep_ready[number] = true;
	i->idep_ptr[number] = NULL;
	return curTick;
    }
    /* else, creator operation will make this value sometime in the future */

    /* indicate value will be created sometime in the future, i.e., operand
     * is not yet ready for use */
    i->idep_ready[number] = false;

    /* link onto the top of the creator's output list of dependant operand */
    i->idep_ptr[number] = new DepLink(producer.rs, producer.odep_num,
				      i, number, queue_num);

    return producer.rs->pred_wb_cycle;
}


Tick
BaseIQ::link_idep(BaseIQ::iterator &i)
{
    int number = i->num_ideps++;
    i->idep_reg[number] = (TheISA::RegIndex)-1;
    i->idep_ready[number] = false;
    i->idep_ptr[number] = NULL;
    return curTick;
}

// =========================================================================
void
BaseIQ::regStats(unsigned threads)
{
    using namespace Stats;

    //  First, register the stats common to all IQ models
    string prefix(name() + ":");

    occ_dist
	.init(threads, 0, max_num_insts, 2)
	.name(prefix + "occ_dist")
	.desc("IQ Occupancy per cycle")
	.flags(total | cdf)
	;

    inst_count
	.init(threads)
	.name(prefix + "cum_num_insts")
	.desc("Total occupancy")
	.flags(total)
	;

    peak_inst_count
	.init(threads)
	.name(prefix + "peak_occupancy")
	.desc("Peak IQ occupancy")
	.flags(total)
	;

    current_count
	.name(prefix + "current_count")
	.desc("Occupancy this cycle")
	;

    empty_count
	.name(prefix + "empty_count")
	.desc("Number of empty cycles")
	;

    fullCount
	.name(prefix + "full_count")
	.desc("Number of full cycles")
	;

    regModelStats(threads);
}

void
BaseIQ::regFormulas(unsigned threads)
{
    using namespace Stats;

//  First, register the stats common to all IQ models
    string prefix(name() + ":");


    occ_rate
	.name(prefix + "occ_rate")
	.desc("Average occupancy")
	.flags(total)
	;
    occ_rate = inst_count / cpu->numCycles;

    avg_residency
	.name(prefix + "avg_residency")
	.desc("Average IQ residency")
	.flags(total)
	;
    avg_residency = occ_rate / cpu->issue_rate;

    empty_rate
	.name(prefix + "empty_rate")
	.desc("Fraction of cycles empty")
	;
    empty_rate = 100 * empty_count / cpu->numCycles;

    full_rate
	.name(prefix + "full_rate")
	.desc("Fraction of cycles full")
	;
    full_rate = 100 * fullCount / cpu->numCycles;

    regModelFormulas(threads);
}

void
BaseIQ::tick_stats()
{
    // only useful with ptrace...
    current_count = count();

    // First, tick any common stats...
    if (count() == 0) {
	++empty_count;
	for (int i = 0; i < cpu->number_of_threads; ++i)
	    occ_dist[i].sample(0);
    } else {
	for (int i = 0; i < cpu->number_of_threads; ++i) {
	    unsigned c = count(i);
	    inst_count[i] += c;

	    occ_dist[i].sample(c);

	    if (c > peak_inst_count[i].value())
		peak_inst_count[i] = c;
	}
    }

    // Finally, tick the model-specific stats
    tick_model_stats();
}



//==========================================================================

//
// need to declare this here since there is no concrete BaseIQ type
// that can be constructed (i.e., no REGISTER_SIM_OBJECT() macro call,
// which is where these get declared for concrete types).
//
DEFINE_SIM_OBJECT_CLASS_NAME("BaseIQ", BaseIQ)
