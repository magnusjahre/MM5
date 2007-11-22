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
#include <iostream>
#include <iomanip>
#include <string>

#include "base/callback.hh"
#include "base/cprintf.hh"
#include "base/misc.hh"
#include "base/statistics.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/fetch.hh"
#include "encumbered/cpu/full/floss_reasons.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/issue.hh"
#include "encumbered/cpu/full/thread.hh"
#include "mem/mem_cmd.hh"

using namespace std;

#define MAYBE_INLINE

#define ENABLE_COUNTER_CHECKING 0

#if ENABLE_COUNTER_CHECKING
static void check_counters(FullCPU *, FlossState *state);
#endif

static const
char * CommitEndDesc[NUM_COMMIT_END_CAUSES] = {
    "ROB_Empty",
    "Commit_BW",
    "StoreBuf",
    "MemBarrier",
    "meta: FU",
    "meta: DCache"
};

static const
char * IssueEndDesc[NUM_ISSUE_END_CAUSES] = {
    "NoInsts",
    "Queue",
    "IssueBW",
    "TooYoung",
    "IssueInorder",
    "meta: Deps",
    "meta: FU",
    "meta: Mem"
};

static const
char * DisEndDesc[NUM_DIS_END_CAUSES] = {
    "IREG_Full",
    "FPREG_Full",
    "No_Inst",
    "ROB_cap",
    "IQ_cap",
    "BW",
    "Policy",
    "Serializing",
    "Broken",
    "meta: IQ full",
    "meta: LSQ full",
    "meta: ROB full"
};

static const
char * FetchEndDesc[NUM_FETCH_END_CAUSES] = {
    "None",
    "Bandwidth",
    "BrLim",
    "InvPC",
    "BTB_Miss",
    "BrRecover",
    "FaultFlush",
    "Sync",
    "LowConf",
    "Policy",
    "Unknown",
    "Zero_Prio",
    "meta: ICache",
    "meta: QFull"
};



//
// These *_split parameters are no longer supported in the simulator
// itself, but we give them dummy values here so we can leave the
// related floss_reasons intact, just in case we re-implement the
// parameters at some later date.
//
const int fetch_split = 1;
const int issue_split = 1;
const int decode_split = 1;
const int commit_split = 1;

static const char **FUDesc;


//
//  FlossState::clear()
//
//  This routine is called prior to the begining of each cycle
//
void
FlossState::clear()
{
    for (int t = 0; t < SMT_MAX_THREADS; ++t) {
        commit_end_cause[t] = COMMIT_CAUSE_NOT_SET;
        issue_end_cause[t] = ISSUE_CAUSE_NOT_SET;

	//  this has to be a "policy" since this will be the "end cause"
	//  for all the threads that don't get to dispatch this cycle
        dispatch_end_cause = FLOSS_DIS_POLICY;

	//  Can't do this, because we need the cause from last cycle...
	//      machine_floss_state.fetch_end_cause[t] = FLOSS_CAUSE_NOT_SET;

	for (int j = 0; j < TheISA::MaxInstSrcRegs; ++j) {
	    commit_fu[t][j] = No_OpClass;
	    issue_fu[t][j]  = No_OpClass;
	}

	// this will serve as "not set"
	commit_mem_result[t] = MA_NOT_PREDICTED;
	issue_mem_result[t]  = MA_NOT_PREDICTED;
	fetch_mem_result[t]  = MA_NOT_PREDICTED;
    }
}



/*-------------------------------------------------------------------*/

static MAYBE_INLINE void
blame_fu(FullCPU *cpu, int thread, double total_loss, OpClass *fu_classes, 
	 FlossType type)
{
    double fu_count = 0;

    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	if (fu_classes[i] != No_OpClass) {
	    ++fu_count;
	}
    }
    double loss = total_loss / fu_count;

    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
	if (fu_classes[i] != No_OpClass) {
	    int fu = fu_classes[i];
	    switch (type) {
		case FLOSS_IQ_FU:
		  cpu->floss_iqfull_fu[thread][fu] += loss;
		  break;
		case FLOSS_LSQ_FU:
		  cpu->floss_lsqfull_fu[thread][fu] += loss;
		  break;
		case FLOSS_IQ_DEPS:
		  cpu->floss_iqfull_deps[thread][fu] += loss;
		  break;
		case FLOSS_LSQ_DEPS:
		  cpu->floss_lsqfull_deps[thread][fu] += loss;
		  break;
		case FLOSS_ROB:
		  cpu->floss_robfull_fu[thread][fu] += loss;
		  break;
	    }
	}
    }
}

/*-------------------------------------------------------------------*/

static MAYBE_INLINE void
blame_commit_stage(FullCPU *cpu, FlossState *state, int thread,
		   double total_loss)
{
    double blame_fraction[SMT_MAX_THREADS];
    double loss_part;
    int commit_per_thread;
    int t, cause;
    int blame_threads = 1;


    //
    //  If we are splitting bandwidth, we may blame several causes
    //
    if (commit_split > 1) {
        commit_per_thread = cpu->commit_width / commit_split;

        for (t = 0; t < cpu->number_of_threads; t++) {
	    blame_fraction[t] =
		(double)(commit_per_thread - cpu->n_committed[t]) /
		(double)(cpu->commit_width - cpu->n_committed_total);
        }

	blame_threads = cpu->number_of_threads;
    } else {
        //  we only use the zero-th element
        blame_fraction[0] = 1;
    }

    //  Assign loss due to thread "t" to thread "thread"
    for (t=0; t<blame_threads; t++) {
        loss_part = total_loss * blame_fraction[t];

	cause = state->commit_end_cause[t];

        switch (cause) {
	  case COMMIT_FU:
	    blame_fu(cpu, thread, loss_part, state->commit_fu[t], FLOSS_ROB);
            break;
	  case COMMIT_DMISS:
	    cpu->floss_robfull_dcache[thread][state->commit_mem_result[t]] 
		+= loss_part;
            break;
	  default:
	    cpu->floss_robfull_other[thread][cause] += loss_part;
            break;
        }
    }
}

/*-------------------------------------------------------------------*/

static MAYBE_INLINE void
blame_issue_stage(FullCPU *cpu, FlossState *state, int thread, 
		  double total_loss, bool lsq)
{
    double blame_fraction[SMT_MAX_THREADS];
    double loss_part;
    int issue_per_thread;
    int t, cause;
    int blame_threads = 1;

    //
    //  If we are splitting bandwidth, we may blame several causes
    //
    if (issue_split > 1) {
        issue_per_thread = cpu->issue_width / issue_split;

        for (t = 0; t < cpu->number_of_threads; t++) {
	    blame_fraction[t] =
		(double)(issue_per_thread - cpu->n_issued[t]) /
		(double)(cpu->issue_width - cpu->n_issued_total);
	    blame_threads = cpu->number_of_threads;
	}
    } else {
        //  we only use the zero-th element
        blame_fraction[0] = 1;
    }

    //  Assign loss due to thread "t" to thread "thread"
    for (t = 0; t < blame_threads; t++) {
        loss_part = total_loss * blame_fraction[t];

	cause = state->issue_end_cause[t];

        switch (cause) {
	  case ISSUE_DEPS:
	    blame_fu(cpu, thread, loss_part, state->issue_fu[t], 
		     lsq ? FLOSS_LSQ_DEPS : FLOSS_IQ_DEPS);
            break;
	  case ISSUE_FU:
	    blame_fu(cpu, thread, loss_part, state->issue_fu[t], 
		     lsq ? FLOSS_LSQ_FU : FLOSS_IQ_FU);
            break;
	  case ISSUE_MEM_BLOCKED:
	    if (!lsq) {
		cpu->floss_iqfull_dcache[thread][state->issue_mem_result[t]] 
		    += loss_part;
	    }
	    else {
		cpu->floss_lsqfull_dcache[thread][state->issue_mem_result[t]] 
		    += loss_part;
	    }
	    break;
	  default:
	    if (!lsq) {
		cpu->floss_iqfull_other[thread][cause] += loss_part;
	    }
	    else {
		cpu->floss_iqfull_other[thread][cause] += loss_part;
	    }
            break;
        }
    }
}


/*-------------------------------------------------------------------*/

static MAYBE_INLINE void
blame_dispatch_stage(FullCPU *cpu, FlossState *state, int thread, double loss) {

    //
    //  Having an MT front-end means that a thread can only be held
    //  responsible for fetch loss is _itself_... threads do not interact
    //  in the front-end (except for fetch bandwidth)
    //

    switch (state->dispatch_end_cause) {
      case FLOSS_DIS_IQ_FULL:
        //  Blame the issue stage
        blame_issue_stage(cpu, state, thread, loss, true);
        break;
      case FLOSS_DIS_LSQ_FULL:
        //  Blame the issue stage
        blame_issue_stage(cpu, state, thread, loss, false);
        break;
      case FLOSS_DIS_ROB_FULL:
        //  Blame the commit stage
        blame_commit_stage(cpu, state, thread, loss);
        break;
      case FLOSS_DIS_CAUSE_NOT_SET:
	panic("FLOSS: dispatch blamed but no cause set");
	break;
      default:
        //  Just blame simple cause
        cpu->floss_qfull_other[thread][state->dispatch_end_cause] += loss;
        break;
    }
}

/*-------------------------------------------------------------------*/

void
FullCPU::flossRecord(FlossState *state, int num_fetched[])
{
    int thread;
    double loss_to_blame = 0;                // Per-thread
    int cause;
    int counter;

    //
    //  Generally, we should only have one fetch_end_cause set...
    //  However, if this is not the case, then the fetch stage is
    //  indicating that we should split the loss across the threads
    //  that have a reason specified.
    //
    //  If fetch_split is active, use "max_to_fetch", otherwise, split
    //  the loss evenly between threads with a reason specified.
    //
    if (fetch_split < 2) {
	int total_fetched = 0;

	counter = 0;
        for (thread = 0; thread < number_of_threads; thread++) {
            if (state->fetch_end_cause[thread] != FLOSS_FETCH_CAUSE_NOT_SET)
                counter++;

	    total_fetched += num_fetched[thread];
        }

	if (!counter)
	    panic("No threads have their fetch-loss cause set!");

	loss_to_blame = (double)(fetch_width - total_fetched) / counter;
    }


    counter = 0;
    for (thread = 0; thread < number_of_threads; thread++) {

        //
        //  Only process this thread if we set a FLOSS cause
        //
        if (state->fetch_end_cause[thread] != FLOSS_FETCH_CAUSE_NOT_SET) {

	    if (fetch_split > 1) {
		loss_to_blame = (fetch_width / fetch_split) -
		    num_fetched[thread];
	    }

       	    assert(loss_to_blame >= 0);

#if DEBUG_CAUSES
            floss_total += loss_to_blame;
            floss_this_cycle++;
#endif

            //  If we lost slots in this thread...
            if (loss_to_blame) {

                cause = state->fetch_end_cause[thread];

                switch (cause) {
		  case FLOSS_FETCH_IMISS:
		    floss_icache[thread][state->fetch_mem_result[thread]] 
			+= loss_to_blame;
		    break;
		  case FLOSS_FETCH_QFULL:
		    blame_dispatch_stage(this, state, thread, loss_to_blame);
		    break;
		  default:
		    floss_other[thread][cause] += loss_to_blame;
		    break;
                }
            }
        }
    }


#if DEBUG_CAUSES
    if (curTick && (curTick % 100) && floss_total != total_floss())
	panic("curTick = %\ncolumn doesn't total correctly!", curTick);
#endif

#if ENABLE_COUNTER_CHECKING
    check_counters(this, state);
#endif

}

//------------------------------------------------------------------------


#if ENABLE_COUNTER_CHECKING

static double
total_floss(FullCPU *cpu)
{
    double total = 0;
    int i;

    for (int t=0; t<cpu->number_of_threads; ++t) {
	for (i=0; i<NUM_MEM_ACCESS_RESULTS; ++i) {
	    total += (cpu->floss_icache[t][i] 
		+ cpu->floss_iqfull_dcache[t][i] 
		+ cpu->floss_lsqfull_dcache[t][i] 
		+ cpu->floss_robfull_dcache[t][i]);
	}

	for (i=0; i<Num_OpClasses; ++i) {
	    total += cpu->floss_iqfull_deps[t][i]
		+ cpu->floss_lsqfull_deps[t][i]
		+ cpu->floss_iqfull_fu[t][i]
		+ cpu->floss_lsqfull_fu[t][i]
		+ cpu->floss_robfull_fu[t][i];
	}

	for (i=0; i<NUM_ISSUE_END_CAUSES; ++i) {
	    total += cpu->floss_iqfull_other[t][i]
		+ cpu->floss_lsqfull_other[t][i];
	}

	for (i=0; i<NUM_COMMIT_END_CAUSES; ++i) {
	    total += cpu->floss_robfull_other[t][i];
	}

	for (i=0; i<NUM_DIS_END_CAUSES; ++i) {
	    total += cpu->floss_qfull_other[t][i];
	}

	for (i=0; i<NUM_FETCH_END_CAUSES; ++i) {
	    total += cpu->floss_other[t][i];
	}
    }

    return total;
}

static void
check_counters(FullCPU *cpu, FlossState *state)
{
    /**
     * @todo We get the total number fetched from the stats counters
     * even though we really shouldn't. stats aren't really meant to
     * be used like this.  They can be, but it's more ideal to keep
     * track of these values outside of the stats package. (same goes
     * for grabbing the number of cycles)
     */
    double insts_fetched = rint(cpu->fetched_inst.total());
    double total_cycles = rint(cpu->numCycles.value());
    double total_slots = rint(cpu->fetch_width * total_cycles);
    double total_loss  = rint(total_floss(cpu));

    if (total_slots != (insts_fetched + total_loss)) {
	ccprintf(cerr,
		 "Failed Counter Check!:\n"
		 "total_cycles=%f\n"
		 "     total_slots   = %f\n"
		 "     total_fetched = %f\n"
		 "     total_loss    = %f\n"
		 "didn't account for = %f\n",
		 total_cycles, total_slots, insts_fetched, total_loss,
		 total_slots - (insts_fetched + total_loss));

	state->dump(cpu->number_of_threads);

	panic("fix this!");
    }
}

#endif

/////////////////////////////////////////////////////////////////////////////
//
//  Debugging....
//
//
//


void
FlossState::dump(unsigned number_of_threads)
{
    cout << "FETCH End Causes:" << endl;
    for (int t = 0; t < number_of_threads; ++t) {
	cout << "  " << t << ": ";
	int cause = fetch_end_cause[t];

	if (cause == -1) {
	    cout << "Not Set" << endl;
	} else {
	    int mem_res = fetch_mem_result[t];
	    cout << FetchEndDesc[cause]
		 << " Mem=" << MemCmd::memAccessDesc[mem_res]
		 << endl;
	}
    }

    cout << "DISPATCH End Cause:" << endl;
    if (dispatch_end_cause != -1)
	cout << "  " << DisEndDesc[dispatch_end_cause];
    else
	cout << "  NOT SET!!!!";
    cout << endl;

    cout << "ISSUE End Causes:" << endl;
    for (int t = 0; t < number_of_threads; ++t) {
	cout << "  " << t << ": ";
	int cause = issue_end_cause[t];

	if (cause == -1) {
	    cout << "Not Set" << endl;
	} else {
	    int mem_res = issue_mem_result[t];
	    cout << IssueEndDesc[cause]
		 << " Mem=" << MemCmd::memAccessDesc[mem_res]
		 << " FU=";
	    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
		int fu_class = issue_fu[t][i];
		cout << opClassStrings[fu_class] << " ";
	    }
	}
	cout << endl;
    }
    cout << "  End Thread = " << issue_end_thread << endl;

    cout << "COMMIT End Causes:" << endl;
    for (int t = 0; t < number_of_threads; ++t) {
	cout << "  " << t << ": ";
	int cause = commit_end_cause[t];

	if (cause == COMMIT_CAUSE_NOT_SET) {
	    cout << "Not Set" << endl;
	} else {
	    int mem_res = commit_mem_result[t];
	    cout << CommitEndDesc[cause]
		 << " Mem=" << MemCmd::memAccessDesc[mem_res]
		 << " FU=";
	    for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
		int fu_class = commit_fu[t][i];
		cout << opClassStrings[fu_class] << " ";
	    }
	}
	cout << endl;
    }
    cout << "  End Thread = " << commit_end_thread << endl;
}

inline void
initv(std::vector<std::vector<double> > &vec, int x, int y)
{
    vec.resize(x);
    for (int i = 0; i < x; ++i) {
	vec[i].resize(y);
	for (int j = 0; j < y; ++j)
	    vec[i][j] = 0.0;
    }
}

void
FullCPU::flossRegStats()
{
    using namespace Stats;

    FUDesc = (const char **) new char *[Num_OpClasses+1];

    // initialize FU labels by copying from opClassStrings[]
    for (int i = 0; i < Num_OpClasses; ++i) {
	FUDesc[i] = opClassStrings[i];
    }
    FUDesc[Num_OpClasses] = "No_FU";

    string name = this->name() + ".floss";

    //
    //  I-Cache
    //
    initv(floss_icache, number_of_threads, NUM_MEM_ACCESS_RESULTS);
    stat_floss_icache
	.init(number_of_threads, NUM_MEM_ACCESS_RESULTS)
	.name(name + ".icache")
	.flags(dist)
	;
    stat_floss_icache.ysubnames(MemCmd::memAccessDesc);


    //
    //  Queue-full deps
    //
    initv(floss_iqfull_deps, number_of_threads, Num_OpClasses);
    stat_floss_iqfull_deps
	.init(number_of_threads, Num_OpClasses)
	.name(name + ".iq_full_deps")
	.flags(dist)
	;
    stat_floss_iqfull_deps.ysubnames(FUDesc);

    initv(floss_lsqfull_deps, number_of_threads, Num_OpClasses);
    stat_floss_lsqfull_deps
	.init(number_of_threads, Num_OpClasses)
	.name(name + ".lsq_full_deps")
	.flags(dist)
	;
    stat_floss_lsqfull_deps.ysubnames(FUDesc);


    //
    //  Queue-full FU
    //
    initv(floss_iqfull_fu, number_of_threads, Num_OpClasses);
    stat_floss_iqfull_fu
	.init(number_of_threads, Num_OpClasses)
	.name(name + ".iq_full_fu")
	.flags(dist)
	;
    stat_floss_iqfull_fu.ysubnames(FUDesc);

    initv(floss_lsqfull_fu, number_of_threads, Num_OpClasses);
    stat_floss_lsqfull_fu
	.init(number_of_threads, Num_OpClasses)
	.name(name + ".lsq_full_fu")
	.flags(dist)
	;
    stat_floss_lsqfull_fu.ysubnames(FUDesc);

    initv(floss_robfull_fu, number_of_threads, Num_OpClasses);
    stat_floss_robfull_fu
	.init(number_of_threads, Num_OpClasses)
	.name(name + ".rob_full_fu")
	.flags(dist)
	;
    stat_floss_robfull_fu.ysubnames(FUDesc);

    //
    //  Queue-full D-Cache
    //
    initv(floss_iqfull_dcache, number_of_threads, NUM_MEM_ACCESS_RESULTS);
    stat_floss_iqfull_dcache
	.init(number_of_threads, NUM_MEM_ACCESS_RESULTS)
	.name(name + ".iq_full_dcache")
	.flags(dist)
	;
    stat_floss_iqfull_dcache.ysubnames(MemCmd::memAccessDesc);

    initv(floss_lsqfull_dcache, number_of_threads, NUM_MEM_ACCESS_RESULTS);
    stat_floss_lsqfull_dcache
	.init(number_of_threads, NUM_MEM_ACCESS_RESULTS)
	.name(name + ".lsq_full_dcache")
	.flags(dist)
	;
    stat_floss_lsqfull_dcache.ysubnames(MemCmd::memAccessDesc);

    initv(floss_robfull_dcache, number_of_threads, NUM_MEM_ACCESS_RESULTS);
    stat_floss_robfull_dcache
	.init(number_of_threads, NUM_MEM_ACCESS_RESULTS)
	.name(name + ".rob_full_dcache")
	.flags(dist)
	;
    stat_floss_robfull_dcache.ysubnames(MemCmd::memAccessDesc);


    //
    //  Queue-full Other
    //
    initv(floss_iqfull_other, number_of_threads, NUM_ISSUE_END_CAUSES);
    stat_floss_iqfull_other
	.init(number_of_threads, NUM_ISSUE_END_CAUSES)
	.name(name + ".iq_full")
	.flags(dist)
	;
    stat_floss_iqfull_other.ysubnames(IssueEndDesc);

    initv(floss_lsqfull_other, number_of_threads, NUM_ISSUE_END_CAUSES);
    stat_floss_lsqfull_other
	.init(number_of_threads, NUM_ISSUE_END_CAUSES)
	.name(name + ".lsq_full")
	.flags(dist)
	;
    stat_floss_lsqfull_other.ysubnames(IssueEndDesc);

    initv(floss_robfull_other, number_of_threads, NUM_COMMIT_END_CAUSES);
    stat_floss_robfull_other
	.init(number_of_threads, NUM_COMMIT_END_CAUSES)
	.name(name + ".rob_full")
	.flags(dist)
	;
    stat_floss_robfull_other.ysubnames(CommitEndDesc);


    //
    //  QueueFull causes
    //
    initv(floss_qfull_other, number_of_threads, NUM_DIS_END_CAUSES);
    stat_floss_qfull_other
	.init(number_of_threads, NUM_DIS_END_CAUSES)
	.name(name + ".qfull")
	.flags(dist)
	;
    stat_floss_qfull_other.ysubnames(DisEndDesc);


    //
    //  Other random stuff...
    //
    initv(floss_other, number_of_threads, NUM_FETCH_END_CAUSES);
    stat_floss_other
	.init(number_of_threads, NUM_FETCH_END_CAUSES)
	.name(name + ".fetch")
	.flags(dist)
	;
    stat_floss_other.ysubnames(FetchEndDesc);
}

