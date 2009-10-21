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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "base/callback.hh"
#include "base/cprintf.hh"
#include "base/statistics.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/dd_queue.hh"
#include "encumbered/cpu/full/rob_station.hh"
#include "mem/cache/cache.hh" // for dynamic cast
#include "mem/mem_interface.hh"
#include "sim/builder.hh"
#include "sim/sim_events.hh"
#include "sim/sim_exit.hh"
#include "sim/stats.hh"

#if FULL_SYSTEM
#include "base/remote_gdb.hh"
#include "cpu/exec_context.hh"
#include "mem/functional/memory_control.hh"
#include "mem/functional/physical.hh"
#include "sim/system.hh"
#include "targetarch/alpha_memory.hh"
#include "targetarch/pseudo_inst.hh"
#endif

using namespace std;


////////////////////////////////////////////////////////////////////////////
//
//  Support for Multiple IQ's & Function-Unit Pools
//
////////////////////////////////////////////////////////////////////////////
unsigned
FullCPU::IQLeastFull()
{
    unsigned iq_idx = 0;
    unsigned least_count = IQ[0]->count();

    for (int i = 1; i < numIQueues; ++i) {
	unsigned num = IQ[i]->count();

	if (least_count > num) {
	    iq_idx = i;
	    least_count = num;
	}
    }

    return iq_idx;
}

unsigned
FullCPU::IQMostFull()
{
    unsigned iq_idx = 0;
    unsigned most_count = IQ[0]->count();

    for (int i = 1; i < numIQueues; ++i) {
	unsigned num = IQ[i]->count();

	if (most_count < num) {
	    iq_idx = i;
	    most_count = num;
	}
    }

    return iq_idx;
}

void
FullCPU::ROBDump()
{
    ROB.dump();
}

void
FullCPU::fuDump(int p)
{
    FUPools[p]->dump();
}


void
FullCPU::fuDump()
{
    for (int i = 0; i < numFUPools; ++i)
	FUPools[i]->dump();
}


unsigned
FullCPU::IQNumInstructions()
{
    unsigned num = 0;

    for (int i = 0; i < numIQueues; ++i)
	num += IQ[i]->count();

    return num;
}


unsigned
FullCPU::IQNumInstructions(unsigned thread)
{
    unsigned num = 0;

    for (int i = 0; i < numIQueues; ++i)
	num += IQ[i]->count(thread);

    return num;
}


unsigned
FullCPU::IQNumReadyInstructions()
{
    unsigned num = 0;

    for (int i = 0; i < numIQueues; ++i)
	num += IQ[i]->ready_count();

    return num;
}

void
FullCPU::dumpIQ()
{
    for (int i = 0; i < numIQueues; ++i)
	IQ[i]->dump();
    cout << "done with dump!\n";
}

// Returns true only if _ALL_ queues have their caps met
bool
FullCPU::IQCapMet(unsigned thread)
{
    switch (dispatch_policy) {
      case MODULO_N:
      case DEPENDENCE:
	for (int i = 0; i < numIQueues; ++i)
	    if (!IQ[i]->cap_met(thread))
		return false;
	break;
      case THREAD_PER_QUEUE:
	return IQ[thread % numIQueues]->cap_met(thread);
	break;
    }

    return true;
}

//  Returns the number of free slots in the queue with the largest number
//  of slots available --> does not take caps into account
unsigned
FullCPU::IQFreeSlots()
{
    unsigned max_slots = 0;

    for (int i = 0; i < numIQueues; ++i) {
	unsigned fs = IQ[i]->free_slots();
	if (fs > max_slots)
	    max_slots = fs;
    }

    return max_slots;
}


//  Returns the number of free slots in all EXCEPT queue
//  'idx'
unsigned
FullCPU::IQFreeSlotsX(unsigned idx)
{
    unsigned slots = 0;

    for (unsigned i = 0; i < numIQueues; ++i)
	if (i != idx)
	    slots += IQ[i]->free_slots();

    return slots;
}


//  Returns the number of free slots in the queue with the largest number
//  of slots available -> queues with the cap met for this thread have NO
//  slots available
unsigned
FullCPU::IQFreeSlots(unsigned thread)
{
    unsigned max_slots = 0;

    for (int i = 0; i < numIQueues; ++i) {
	if (!IQ[i]->cap_met(thread)) {
	    unsigned fs = IQ[i]->free_slots();
	    if (fs > max_slots)
		max_slots = fs;
	}
    }

    return max_slots;
}


BaseIQ::iterator
FullCPU::IQOldestInstruction()
{
    BaseIQ::iterator oldest = 0;

    for (int i = 0; i < numIQueues; ++i) {
	if (oldest.notnull()) {
	    if (IQ[i]->oldest().notnull()) {
		if ((*oldest).seq > (*IQ[i]->oldest()).seq)
		    oldest = IQ[i]->oldest();
	    }
	} else
	    oldest = IQ[i]->oldest();
    }

    return oldest;
}


BaseIQ::iterator
FullCPU::IQOldestInstruction(unsigned thread)
{
    BaseIQ::iterator oldest = 0;

    for (int i = 0; i < numIQueues; ++i) {
	if (oldest.notnull()) {
	    if (IQ[i]->oldest(thread).notnull()) {
		if ((*oldest).seq > (*IQ[i]->oldest(thread)).seq)
		    oldest = IQ[i]->oldest(thread);
	    }
	} else
	    oldest = IQ[i]->oldest(thread);
    }

    return oldest;
}


////////////////////////////////////////////////////////////////////////////
//
//   CPU constructor
//
////////////////////////////////////////////////////////////////////////////

FullCPU::FullCPU(Params *p,
#if FULL_SYSTEM
		 AlphaITB *itb, AlphaDTB *dtb, FunctionalMemory *mem,
#else
		 vector<Process *> workload,
#endif // FULL_SYSTEM

		 //
		 //  Caches
		 //
		 MemInterface *_icache_interface,
		 MemInterface *_dcache_interface,
		 SoftwarePrefetchPolicy _softwarePrefetchPolicy,

		 //
		 //  Internal structures
		 //
		 vector<BaseIQ *> _IQ,
		 unsigned _ROB_size,
		 unsigned _LSQ_size,
		 unsigned _storebuffer_size,

		 //
		 //  Fetch
		 //
		 int _fetch_width,
		 int _lines_to_fetch,
		 int _num_icache_ports,
		 int _fetch_branches,
		 int _ifq_size,
		 int _decode_dispatch_latency,

		 BranchPred *_bpred,

		 int _fetch_policy,
		 bool _fetch_priority_enable,
		 vector<unsigned> _icount_bias,

		 //
		 //  Decode/Dispatch
		 //
		 bool _mt_frontend,
		 int _decode_width,
		 int _dispatch_to_issue_latency,
		 vector<unsigned> _rob_caps,
		 DispatchPolicyEnum disp_policy,
		 bool _loose_mod_n,

		 bool _use_hm_predictor,
		 bool _use_lr_predictor,
		 bool _use_lat_predictor,
		 unsigned _max_chains,
		 unsigned _max_wires,
		 ChainWireInfo::MappingPolicy _chainWirePolicy,


		 //
		 //  Issue
		 //
		 int _issue_width,
		 vector<unsigned> _issue_bandwidth,
		 bool _inorder_issue,
		 MemDisambiguationEnum _disambig_mode,
		 bool _prioritize_issue,
		 vector<FuncUnitPool *> fupools,
		 vector<unsigned> _thread_weights,

		 //
		 //  Writeback
		 //
		 int _mispred_fixup_penalty,
		 int _fault_handler_delay,
		 unsigned _iq_comm_latency,

		 //
		 //  Commit
		 //
		 int _commit_width,
		 bool _prioritized_commit,
		 CommitModelEnum _commit_model,

		 //
		 //  Other
		 //
		 int _pc_sample_interval,
		 PipeTrace *_ptrace,
                 AdaptiveMHA* _amha)
    : BaseCPU(p),
      ROB_size(_ROB_size),
      LSQ_size(_LSQ_size),
      storebuffer_size(_storebuffer_size),

      icacheInterface(_icache_interface),
      dcacheInterface(_dcache_interface),
      softwarePrefetchPolicy(_softwarePrefetchPolicy),

      fetch_width(_fetch_width),
      lines_to_fetch(_lines_to_fetch),
      num_icache_ports(_num_icache_ports),
      fetch_branches(_fetch_branches),
      ifq_size(_ifq_size),
      decode_dispatch_latency(_decode_dispatch_latency),

      branch_pred(_bpred),

      fetch_policy(_fetch_policy),
      fetch_priority_enable(_fetch_priority_enable),

      mt_frontend(_mt_frontend),
      decode_width(_decode_width),
      dispatch_to_issue_latency(_dispatch_to_issue_latency),
      loose_mod_n_policy(_loose_mod_n),
      use_hm_predictor(_use_hm_predictor),
      use_lr_predictor(_use_lr_predictor),
      use_lat_predictor(_use_lat_predictor),
      max_chains(_max_chains),
      max_wires(_max_wires),
      chainWirePolicy(_chainWirePolicy),


      issue_width(_issue_width),
      inorder_issue(_inorder_issue),
      disambig_mode(_disambig_mode),
      prioritize_issue(_prioritize_issue),
      FUPools(fupools),

      mispred_fixup_penalty(_mispred_fixup_penalty),
      fault_handler_delay(_fault_handler_delay),
      iq_comm_latency(_iq_comm_latency),

      commit_width(_commit_width),
      commit_model(_commit_model),
      prioritized_commit(_prioritized_commit),

      ptrace(_ptrace),

      writebackEventQueue("writeback event queue"),

      tickEvent(this),

      pcSampleEvent(NULL)
{
    //Initialize Counters to 0
    lsq_fcount = 0;
    sim_invalid_addrs = 0;
    dependence_depth_count = 0;
    for (int i=0; i < SMT_MAX_THREADS; i++) {
	used_int_physical_regs[i] = 0;
	used_fp_physical_regs[i] = 0;
    }

    expected_inorder_seq_num = 1;

    //
    //  check options for sanity
    //
    if (mispred_fixup_penalty < 1)
	fatal("mis-prediction penalty must be at least 1 cycle");

    if (ifq_size < 1)
	fatal("inst fetch queue size must be positive > 0");

    if (fetch_width < 1)
	fatal("fetch width must be positive non-zero and a power of two");

    if (lines_to_fetch < 1)
	fatal("lines to fetch must be positive non-zero");

    if (decode_width < 1)
	fatal("decode width must be positive non-zero");

    if (issue_width < 1)
	fatal("issue width must be positive non-zero");

    if (commit_width < 1)
	fatal("commit width must be positive non-zero");

    if (prioritized_commit && commit_model != COMMIT_MODEL_SMT)
	fatal("Cannot do prioritized commit with this commit model");

    if (use_lat_predictor && use_lr_predictor)
	fatal("SegmentedIQ: You really don't want to use BOTH the latency "
	      "and L/R predictors");

    mod_n_queue_idx = 0;

    //  dispatch_width is not (currently) a user-modifiable value
    dispatch_width = decode_width;

    // note number of IQ's, copy IQ pointers from vector to array
    // let IQ know which CPU it belongs to
    numIQueues = _IQ.size();
    if (numIQueues == 0)
	fatal("You must specify at least one IQ");

    if (numIQueues > 1) {
	if (issue_width % numIQueues)
	    panic("issue bandwidth doesn't distribute evenly");

	if (dispatch_width % numIQueues)
	    panic("dispatch bandwidth doesn't distribute evenly");
    }

    static_icount_bias.resize(SMT_MAX_THREADS);
    static_icount_bias = _icount_bias;

    first_decode_thread = 0;


    IQ = new BaseIQ *[numIQueues];

    IQNumSlots = 0;
    for (int i = 0; i < numIQueues; ++i) {
	IQ[i] = _IQ[i];
	IQNumSlots += IQ[i]->size();

	IQ[i]->init(this, dispatch_width / numIQueues,
		    issue_width / numIQueues, i);
    }

    //
    //  Create whatever information is going to be used by the IQ's
    //  and pass the pointer to the structure around...
    //
    clusterSharedInfo = IQ[0]->buildSharedInfo();
    for (int q = 1; q < numIQueues; ++q)
	IQ[q]->setSharedInfo(clusterSharedInfo);

    hmp_func = HMP_HEAD_SEL;

    //
    //  Issue prioritization
    //
    if (prioritize_issue) {
	issue_thread_weights.resize(SMT_MAX_THREADS);
	issue_thread_weights = _thread_weights;

	hp_thread_change = issue_thread_weights[0];  // schedule first change
	hp_thread = 0;  // first thread is HP
    }


    //
    //  Configure the fetch-to-decode queue
    //
    //
    if (decode_dispatch_latency < 2)
	fatal("decode/dispatch latency must be at least 2 cycles");

    decodeQueue = new DecodeDispatchQueue(this, decode_dispatch_latency,
					 dispatch_width, mt_frontend);

    dispatch_policy = disp_policy;

    next_fetch_seq = 1;

    for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	dispatch_starting_iqueue[i] = i % numIQueues;
	lastDispatchTime[i] = 0;
	correctPathSeq[i] = 1;
    }
    issue_starting_iqueue = 0;

    //  Function unit pools
    numFUPools = FUPools.size();
    if (numFUPools == 0)
	fatal("You must specify at least one FU pool");

    issue_starting_fu_pool = 0;
    issue_current_fupool_for_sb = 0;

    issue_bandwidth.resize(SMT_MAX_THREADS);
    issue_bandwidth = _issue_bandwidth;

    // let bpred know which CPU it belongs to
    if (branch_pred)
      branch_pred->setCPU(this);

    rr_commit_last_thread = 0;

    dispatch_seq = 0;

    lastChainUsed = 0;

    int total_int_physical_regs =
      ROB_size + SMT_MAX_THREADS * TheISA::NumIntRegs;
    int total_fp_physical_regs =
      ROB_size + SMT_MAX_THREADS * TheISA::NumFloatRegs;

    chain_heads_in_rob = 0;

    free_int_physical_regs = total_int_physical_regs;
    free_fp_physical_regs = total_fp_physical_regs;

    LSQ = new load_store_queue(this, name() + ".LSQ", LSQ_size, false);

    //  The ROB is a MachineQueue<ROBStation>
    ROB.init(this, ROB_size, numIQueues);

    rob_cap.resize(SMT_MAX_THREADS);
    rob_cap = _rob_caps;

    storebuffer = new StoreBuffer(this, name() + ".SB", storebuffer_size);

    // not sure why, but copied this from fetch.cc
    if (ifq_size < (number_of_threads * fetch_width))
	fatal("ifetch queue size must be > number_of_threads * fetch_width");

    //
    //  Initialize fetch list
    //
    initialize_fetch_list(true);

    /* allocate and initialize register file */
    for (int i = 0; i < number_of_threads; i++) {

#if FULL_SYSTEM
	assert(i == 0);	// can't handle SMT full-system yet
	thread[i] = new SpecExecContext(this, i, system, itb, dtb, mem);
	//
	// stuff below happens in SimpleCPU constructor too... should
	// put all this in some common place
	//
	SpecExecContext *xc = thread[i];

	// initialize CPU, including PC
	TheISA::initCPU(&xc->regs);

	change_thread_state(i, true, 100);
#else
	if (i < workload.size()) {
	    // we've got a process to initialize this thread from
	    thread[i] = new SpecExecContext(this, i, workload[i], i);
	    change_thread_state(i, true, 100);
	} else
	    // idle context... can't deal with this yet, but will someday
	    fatal("uninitialized thread context not supported");
#endif // FULL_SYSTEM

	// Add this context to the context list in BaseCPU as well.
	execContexts.push_back(thread[i]);

	// clear commitPC in case we look at it before we commit the
	// first instruction
	commitPC[i] = 0;
    }

    cv_init();
    dispatch_init();
    fetch_init();

    //
    //  The segmented IQ needs to know the latency of a cache hit
    //
    for (int i = 0; i < numIQueues; ++i)
	IQ[i]->set_cache_hit_latency(dcacheInterface->getHitLatency());

    issue_init();

    // Set up PC sampling event if necessary
    if (_pc_sample_interval > 0) {
	pcSampleEvent = new PCSampleEvent(_pc_sample_interval, this);
	Callback *cb =
	    new MakeCallback<FullCPU, &FullCPU::dumpPCSampleProfile>(this);
	registerExitCallback(cb);
    }

    if(_amha != NULL){
        _amha->registerFullCPU(p->cpu_id, this);
    }
    amha = _amha;

    noCommitCycles = 0;
    string tracename = name() + "BlockedTrace.txt";
    ofstream tracefile(tracename.c_str());
    tracefile << "Tick;Blocked Fraction\n";
    tracefile.flush();
    tracefile.close();

    tmpBlockedCycles = 0;
    l1MissStallCycles = 0;

    hasDumpedStats = false;
    statsOrderFileName = "statsDumpOrder.txt";
    ofstream statDumpFile(statsOrderFileName.c_str());
    statDumpFile << "";
    statDumpFile.flush();
    statDumpFile.close();

    useInExitDesicion = true;
}


//
//  Destructor
//
//  Not strictly necessary, but useful for determining exactly what _is_ a
//  memory leak...
//
FullCPU::~FullCPU()
{
    //
    //  First, delete the stuff that the constructor explicitly allocated
    //
    delete[] IQ;
    delete LSQ;
    delete storebuffer;

    for (int i = 0; i < number_of_threads; ++i)
	delete thread[i];

    //
    //  Second, delete the SimObjects that we received as a pointer
    //
#if 0
    delete icache;
    delete dcache;
#endif

    unsigned num_fupools = FUPools.size();
    for (int i = 0; i < num_fupools; ++i)
	delete FUPools[i];

    delete branch_pred;

    delete ptrace;
}

void
FullCPU::takeOverFrom(BaseCPU *oldCPU)
{

    BaseCPU::takeOverFrom(oldCPU);

#if FULL_SYSTEM
    /**
     * @todo this here is a hack to make sure that none of this crap
     * happens in full_cpu.  The quiesce especically screws up full cpu.
     */
    using namespace AlphaPseudo;
    doStatisticsInsts = false;
    doCheckpointInsts = false;
    doQuiesce = false;
#endif

    assert(!tickEvent.scheduled());
    assert(oldCPU->execContexts.size() == execContexts.size());

    // Magnus: This comment is wrong
    // Set all status's to active, schedule the
    // CPU's tick event.
    tickEvent.schedule(curTick);
    for (int i = 0; i < execContexts.size(); ++i) {

        // Fix by Magnus
        // The processor state should remain the same when the processor is switched
        if(execContexts[i]->status() == ExecContext::Suspended){
            execContexts[i]->suspend();
        }
        else if(execContexts[i]->status() == ExecContext::Active){
            execContexts[i]->activate();
        }
        else if(execContexts[i]->status() == ExecContext::Halted){
            execContexts[i]->halt();
        }
        else if(execContexts[i]->status() == ExecContext::Unallocated){
            execContexts[i]->unallocate();
        }
        else{
            fatal("Unimplemented execution status transfer (by Magnus)");
        }
    }
}


// post-unserialization initialization callback
void
FullCPU::startup()
{
    // schedule initial sampling event here since curTick could get
    // warped by unserialization.  event was created (if needed) in
    // FullCPU contructor.
    if (pcSampleEvent) {
	pcSampleEvent->schedule(curTick);
    }
}


void
FullCPU::regStats()
{
    using namespace Stats;

    BaseCPU::regStats();
    commitRegStats();
    fetchRegStats();
    storebuffer->regStats();
    decodeQueue->regStats();
    dispatchRegStats();
    issueRegStats();
    writebackRegStats();

    flossRegStats();

    reg_int_thrd_occ
	.init(number_of_threads)
	.name(name() + ".REG:int:occ")
	.desc("Cumulative count of INT register usage")
	.flags(total)
	;

    reg_fp_thrd_occ
	.init(number_of_threads)
	.name(name() + ".REG:fp:occ")
	.desc("Cumulative count of FP register usage")
	.flags(total)
	;

    ROB_fcount
	.name(name() + ".ROB:full_count")
	.desc("number of cycles where ROB was full")
	;

    ROB_count
	.init(number_of_threads)
	.name(name() + ".ROB:occupancy")
	.desc(name() + ".ROB occupancy (cumulative)")
	.flags(total)
	;

    currentROBCount
	.name(name() + ".ROB:current_count")
	.desc("Current ROB occupancy")
	;

    IFQ_count
	.name(name() + ".IFQ:count")
	.desc("cumulative IFQ occupancy")
	;

    IFQ_fcount
	.init(number_of_threads)
	.name(name() + ".IFQ:full_count")
	.desc("cumulative IFQ full count")
	.flags(total)
	;

    ROB_occ_dist
	.init(number_of_threads,0,ROB_size,2)
	.name(name() + ".ROB:occ_dist")
	.desc("ROB Occupancy per cycle")
	.flags(total | cdf)
	;

    for (int i = 0; i < numIQueues; ++i)
	IQ[i]->regStats(number_of_threads);

    LSQ->regStats(number_of_threads);
}

void
FullCPU::regFormulas()
{
    using namespace Stats;

    storebuffer->regFormulas(this);
    decodeQueue->regFormulas();
    dispatchRegFormulas();
    fetchRegFormulas();
    writebackRegFormulas();
    commitRegFormulas();
    issueRegFormulas();

    ROB_full_rate
	.name(name() + ".ROB:full_rate")
	.desc("ROB full per cycle")
	;
    ROB_full_rate = ROB_fcount / numCycles;

    ROB_occ_rate
	.name(name() + ".ROB:occ_rate")
	.desc("ROB occupancy rate")
	.flags(total)
	;
    ROB_occ_rate = ROB_count / numCycles;

    IFQ_occupancy
	.name(name() + ".IFQ:occupancy")
	.desc("avg IFQ occupancy (inst's)")
	;
    IFQ_occupancy = IFQ_count / numCycles;

    IFQ_latency
	.name(name() + ".IFQ:latency")
	.desc("avg IFQ occupant latency (cycle's)")
	.flags(total)
	;
    IFQ_latency = IFQ_occupancy / dispatch_rate;

    IFQ_full_rate
	.name(name() + ".IFQ:full_rate")
	.desc("fraction of time (cycle's) IFQ was full")
	.flags(total);
	;
    IFQ_full_rate = IFQ_fcount * constant(100) / numCycles;

    for (int i = 0; i < numIQueues; ++i)
	IQ[i]->regFormulas(number_of_threads);

    LSQ->regFormulas(number_of_threads);
}

void
FullCPU::remove_LSQ_element(BaseIQ::iterator i)
{
    LSQ->squash(i);
}

void
FullCPU::remove_ROB_element(ROBStation *rob_entry)
{
    if (rob_entry->cache_event_ptr)
	rob_entry->cache_event_ptr->squash();

    int thread = rob_entry->thread_number;

    //  These pointers should have been cleared either when
    //  this entry was squashed, or when the event was processed
    assert(rob_entry->wb_event == 0);
    assert(rob_entry->delayed_wb_event == 0);
    assert(rob_entry->cache_event_ptr == 0);
    assert(rob_entry->recovery_event == 0);


    if (rob_entry->spec_state.notnull()) {
	state_list.dump(rob_entry->spec_state);
	rob_entry->spec_state = 0;
    }

    //
    //  Only free physical registers when instruction is removed
    //  from the ROB.
    //
    //

    unsigned num_fp_regs = rob_entry->inst->numFPDestRegs();
    unsigned num_int_regs = rob_entry->inst->numIntDestRegs();

    free_fp_physical_regs += num_fp_regs;
    free_int_physical_regs += num_int_regs;

    used_fp_physical_regs[thread] -= num_fp_regs;
    used_int_physical_regs[thread] -= num_int_regs;

    if (rob_entry->head_of_chain) {
	--chain_heads_in_rob;
    }

    ROB.remove(rob_entry);

    // do this after ROB.remove() since that function needs the thread number
    delete rob_entry->inst;
    rob_entry->inst = NULL;
}


void
FullCPU::activateContext(int thread_num, int delay)
{
    assert(thread[thread_num] != NULL);

    if (tickEvent.squashed())
	tickEvent.reschedule(curTick + delay);
    else if (!tickEvent.scheduled())
	tickEvent.schedule(curTick + delay);
}


void
FullCPU::tick()
{
    numCycles++;

    floss_this_cycle = 0;
    floss_state.clear();

    for (int i = 0; i < number_of_threads; i++) {
	iq_cap_active[i] = 0;
	rob_cap_active[i] = 0;
    }

    if (ptrace && ptrace->newCycle(curTick))
	new SimExitEvent("ptrace caused exit");

    //  Handle every-cycle stuff for the IQ, LSQ, &storebuffer
    for (int i = 0; i < numIQueues; ++i)
	IQ[i]->pre_tick();

    LSQ->pre_tick();
    storebuffer->pre_tick();

    /* commit entries from IQ/LSQ to architected register file */
    commit();

    /* service function unit release events */
    for (int i = 0; i < numFUPools; ++i)
	FUPools[i]->tick();

    /* ==> may have ready queue entries carried over from previous cycles */

    /* service result completions, also readies dependent operations */
    /* ==> inserts operations into ready queue --> register deps resolved */
    writeback();

    /* try to locate memory operations that are ready to execute */
    /* ==> inserts operations into ready queue --> mem deps resolved */
    lsq_refresh();

    for (int i = 0; i < numIQueues; ++i)
	IQ[i]->tick_ready_stats();

    LSQ->tick_ready_stats();
    storebuffer->tick_ready_stats();

    /* issue operations ready to execute from a previous cycle */
    /* <== drains ready queue <-- ready operations commence execution */
    issue();


    //  Handle every-cycle stuff for the IQ, LSQ, &storebuffer
    for (int i = 0; i < numIQueues; ++i)
	IQ[i]->tick();

    LSQ->tick();
    storebuffer->tick();


    /* decode and dispatch new operations */
    /* ==> insert ops w/ no deps or all regs ready --> reg deps resolved */
    dispatch();

    decodeQueue->tick();

    start_decode();

    fetch();

    //==================================================================
    //
    //  End-Of-Cycle
    //
    //==================================================================

    //
    //  Do every-cycle statistics here
    //
    //  We do them here because the machine is in a consistent state
    //  at this point (unlike prior to this)
    //
    for (int i = 0; i < numIQueues; ++i)
	IQ[i]->tick_stats();

    LSQ->tick_stats();

    decodeQueue->tick_stats();

    currentROBCount = ROB.num_active();

    for (int i = 0; i < number_of_threads; ++i) {
	// update buffer occupancy stats
	IFQ_count += ifq[i].num_total();
	IFQ_fcount[i] += ((ifq[i].num_total() == ifq_size) ? 1 : 0);

	if (!thread_info[i].active)
	    continue;

	ROB_count[i] += ROB.num_thread(i);
	ROB_occ_dist[i].sample(ROB.num_thread(i));

	//  Update register occ stats
	reg_int_thrd_occ[i] += used_int_physical_regs[i];
	reg_fp_thrd_occ[i] += used_fp_physical_regs[i];
    }

#if DEBUG_CAUSES
    if (floss_this_cycle <= 0)
	ccprintf(cerr, "Didn't FLOSS this cycle! (%n)\n\n", curTick);
#endif

    if (!tickEvent.scheduled())
	tickEvent.schedule(curTick + cycles(1));
}


BranchPred *
FullCPU::getBranchPred()
{
    return branch_pred;
}


BEGIN_DECLARE_SIM_OBJECT_PARAMS(FullCPU)

    Param<int> num_threads;
    Param<int> clock;
    Param<int> cpu_id;

#if FULL_SYSTEM
    SimObjectParam<AlphaITB *> itb;
    SimObjectParam<AlphaDTB *> dtb;
    SimObjectParam<FunctionalMemory *> mem;
    SimObjectParam<System *> system;
//     Param<int> cpu_id;
    Param<bool> interval_stats;
#else
    SimObjectVectorParam<Process *> workload;
#endif // FULL_SYSTEM

    Param<Counter> max_insts_any_thread;
    Param<Counter> max_insts_all_threads;
    Param<Counter> max_loads_any_thread;
    Param<Counter> max_loads_all_threads;

    SimObjectParam<BaseCache *> icache;
    SimObjectParam<BaseCache *> dcache;
    SimpleEnumParam<FullCPU::SoftwarePrefetchPolicy> sw_prefetch_policy;

    SimObjectVectorParam<BaseIQ *> iq;
    Param<int> rob_size;
    Param<int> lsq_size;
    Param<int> storebuffer_size;

    Param<int> fetch_width;
    Param<int> lines_to_fetch;
    Param<int> num_icache_ports;
    Param<int> fetch_branches;
    Param<int> ifq_size;
    Param<int> decode_to_dispatch;

    SimObjectParam<BranchPred *> branch_pred;

    SimpleEnumParam<FetchPolicyEnum> fetch_policy;
    Param<bool> fetch_pri_enable;
    VectorParam<unsigned> icount_bias;

    Param<bool> mt_frontend;
    Param<int> decode_width;
    Param<int> dispatch_to_issue;
    VectorParam<unsigned> rob_caps;
    SimpleEnumParam<FullCPU::DispatchPolicyEnum> dispatch_policy;
    Param<bool> loose_mod_n_policy;
    Param<bool> use_hm_predictor;
    Param<bool> use_lr_predictor;
    Param<bool> use_lat_predictor;
    Param<unsigned> max_chains;
    Param<unsigned> max_wires;
    SimpleEnumParam<ChainWireInfo::MappingPolicy> chain_wire_policy;

    Param<int> issue_width;
    VectorParam<unsigned> issue_bandwidth;
    Param<bool> inorder_issue;
    SimpleEnumParam<FullCPU::MemDisambiguationEnum> disambig_mode;
    Param<bool> prioritized_issue;
    SimObjectVectorParam<FuncUnitPool *> fupools;

    VectorParam<unsigned> thread_weights;

    Param<int> mispred_recover;
    Param<int> fault_handler_delay;
    Param<unsigned> iq_comm_latency;

    Param<int> commit_width;
    Param<bool> prioritized_commit;
    SimpleEnumParam<FullCPU::CommitModelEnum> commit_model;

    Param<int> pc_sample_interval;
    Param<bool> function_trace;
    Param<Tick> function_trace_start;

    SimObjectParam<PipeTrace *> ptrace;

    Param<int> width;

    Param<bool> defer_registration;

    SimObjectParam<AdaptiveMHA *>  adaptiveMHA;
    Param<Counter> min_insts_all_cpus;

END_DECLARE_SIM_OBJECT_PARAMS(FullCPU)

static const char *fetch_policy_strings[] = {
    "RR", "IC", "ICRC", "Ideal", "Conf",
    "Redundant", "RedIC", "RedSlack", "Rand"
};

static const char *commit_model_strings[] = {
    "smt", "perthread", "sscalar", "rr"
};

const char *dispatch_policy_strings[] = {
    "mod_n", "perqueue", "dependence"
};

static const char *sw_prefetch_policy_strings[] = {
    "enable", "disable", "squash"
};

static const char *disambig_mode_strings[] = {
    "conservative", "normal", "oracle"
};

static const char *chainWirePolicyStrings[] = {
    "OneToOne", "Static", "StaticStall", "Dynamic"
};

BEGIN_INIT_SIM_OBJECT_PARAMS(FullCPU)

    INIT_PARAM(num_threads,
	       "number of HW thread contexts (dflt = # of processes)"),

    INIT_PARAM(clock, "clock speed"),
    INIT_PARAM(cpu_id, "processor ID"),

#if FULL_SYSTEM
    INIT_PARAM(itb, "Instruction TLB"),
    INIT_PARAM(dtb, "Data TLB"),
    INIT_PARAM(mem, "memory"),
    INIT_PARAM(system, "system object"),
//     INIT_PARAM(cpu_id, "processor ID"),
    INIT_PARAM_DFLT(interval_stats, "Turn on interval stats for this cpu",
		    false),
#else
    INIT_PARAM(workload, "processes to run"),
#endif // FULL_SYSTEM

    INIT_PARAM_DFLT(max_insts_any_thread,
		    "terminate when any thread reaches this inst count",
		    0),
    INIT_PARAM_DFLT(max_insts_all_threads,
		    "terminate when all threads have reached this inst count",
		    0),
    INIT_PARAM_DFLT(max_loads_any_thread,
		    "terminate when any thread reaches this load count",
		    0),
    INIT_PARAM_DFLT(max_loads_all_threads,
		    "terminate when all threads have reached this load count",
		    0),


    INIT_PARAM(icache, "L1 instruction cache object"),
    INIT_PARAM(dcache, "L1 data cache object"),
    INIT_ENUM_PARAM_DFLT(sw_prefetch_policy,
		    "software prefetch policy",
		    sw_prefetch_policy_strings, FullCPU::SWP_ENABLE),

    INIT_PARAM(iq, "instruction queue object"),
    INIT_PARAM(rob_size, "reorder buffer size"),
    INIT_PARAM(lsq_size, "load/store queue size"),
    INIT_PARAM(storebuffer_size, "store buffer size"),

    INIT_PARAM(fetch_width, "instruction fetch BW (insts/cycle)"),
    // Give a large default so this doesn't play into choking fetch.
    INIT_PARAM_DFLT(lines_to_fetch, "instruction fetch BW (lines/cycle)", 999),
    INIT_PARAM(num_icache_ports, "number of icache ports"),
    INIT_PARAM(fetch_branches, "stop fetching after 'n'-th branch"),
    INIT_PARAM(ifq_size, "instruction fetch queue size (in insts)"),
    INIT_PARAM(decode_to_dispatch, "decode to dispatch latency (cycles)"),

    INIT_PARAM(branch_pred, "branch predictor object"),

    INIT_ENUM_PARAM_DFLT(fetch_policy, "SMT fetch policy",
			 fetch_policy_strings, IC),
    INIT_PARAM_DFLT(fetch_pri_enable, "use thread priorities in fetch", false),
    INIT_PARAM(icount_bias, "per-thread static icount bias"),

    INIT_PARAM_DFLT(mt_frontend, "use the multi-threaded IFQ and FTDQ", true),
    INIT_PARAM(decode_width, "instruction decode BW (insts/cycle)"),
    INIT_PARAM_DFLT(dispatch_to_issue,
		    "minimum dispatch to issue latency (cycles)", 1),
    INIT_PARAM(rob_caps, "maximum per-thread ROB occupancy"),
    INIT_ENUM_PARAM_DFLT(dispatch_policy,
			 "method for selecting destination IQ",
			 dispatch_policy_strings, FullCPU::MODULO_N),
    INIT_PARAM_DFLT(loose_mod_n_policy, "loosen the Mod-N dispatch policy",
		    true),
    INIT_PARAM_DFLT(use_hm_predictor,  "enable hit/miss predictor", false),
    INIT_PARAM_DFLT(use_lr_predictor,  "enable left/right predictor", true),
    INIT_PARAM_DFLT(use_lat_predictor, "enable latency predictor", false),
    INIT_PARAM_DFLT(max_chains, "maximum number of dependence chains", 64),
    INIT_PARAM_DFLT(max_wires, "maximum number of dependence chain wires", 64),
    INIT_ENUM_PARAM_DFLT(chain_wire_policy,
			 "chain-wire assignment policy",
			 chainWirePolicyStrings, ChainWireInfo::OneToOne),

    INIT_PARAM(issue_width, "instruction issue B/W (insts/cycle)"),
    INIT_PARAM(issue_bandwidth, "maximum per-thread issue rate"),
    INIT_PARAM_DFLT(inorder_issue, "issue instruction inorder", false),
    INIT_ENUM_PARAM_DFLT(disambig_mode,
		    "memory address disambiguation mode",
		    disambig_mode_strings, FullCPU::DISAMBIG_NORMAL),
    INIT_PARAM_DFLT(prioritized_issue, "issue HP thread first", false),
    INIT_PARAM(fupools, "list of FU pools"),

    INIT_PARAM(thread_weights, "issue priority weights"),

    INIT_PARAM(mispred_recover, "branch misprediction recovery latency"),
    INIT_PARAM_DFLT(fault_handler_delay,
		    "latency from commit of faulting inst to fetch of handler",
		    5),
    INIT_PARAM_DFLT(iq_comm_latency,
	       "writeback communication latency (cycles) for multiple IQ's",1),

    INIT_PARAM(commit_width, "instruction commit BW (insts/cycle)"),
    INIT_PARAM_DFLT(prioritized_commit, "use thread priorities in commit",
		    false),
    INIT_ENUM_PARAM_DFLT(commit_model, "commit model", commit_model_strings,
			 FullCPU::COMMIT_MODEL_SMT),

    INIT_PARAM(pc_sample_interval, "PC sample interval"),
    INIT_PARAM_DFLT(function_trace, "Enable function trace", false),
    INIT_PARAM_DFLT(function_trace_start, "Cycle to start function trace", 0),

    INIT_PARAM(ptrace, "pipeline tracing object"),

    INIT_PARAM(width, "default machine width"),

    INIT_PARAM_DFLT(defer_registration, "defer registration with system "
		    "(for sampling)", false),

    INIT_PARAM_DFLT(adaptiveMHA, "adaptive mha pointer", 0),

    INIT_PARAM_DFLT(min_insts_all_cpus, "Number of instructions to dump stats. If all CPUs have reached this inst count, simulation is terminated.", 0)

END_INIT_SIM_OBJECT_PARAMS(FullCPU)


CREATE_SIM_OBJECT(FullCPU)
{
#if FULL_SYSTEM
    // full-system only supports a single thread for the moment
    int actual_num_threads = 1;
#else
    // in non-full-system mode, we infer the number of threads from
    // the workload if it's not explicitly specified
    int actual_num_threads =
	num_threads != 0 ? num_threads : workload.size();

    if (workload.size() == 0) {
	fatal("Must specify at least one workload!");
    }
#endif

    FullCPU::Params *params = new FullCPU::Params();
    params->name = getInstanceName();
    params->numberOfThreads = actual_num_threads;
    params->max_insts_any_thread = max_insts_any_thread;
    params->max_insts_all_threads = max_insts_all_threads;
    params->max_loads_any_thread = max_loads_any_thread;
    params->max_loads_all_threads = max_loads_all_threads;
    params->deferRegistration = defer_registration;
    params->clock = clock;
    params->functionTrace = function_trace;
    params->functionTraceStart = function_trace_start;
    params->cpu_id = cpu_id;
    params->min_insts_all_cpus = min_insts_all_cpus;
#if FULL_SYSTEM
    params->system = system;
//     params->cpu_id = cpu_id;
#endif


    //
    //  Issue bandwidth caps
    //
    vector<unsigned> ibw(SMT_MAX_THREADS);
    unsigned issue_bandwidth_size = 0;
    if (issue_bandwidth.isValid())
	issue_bandwidth_size = issue_bandwidth.size();

    if (issue_bandwidth_size >= SMT_MAX_THREADS) {
	// copy what we need...
	for (int i = 0; i < SMT_MAX_THREADS; ++i)
	    ibw[i] = issue_bandwidth[i];
    }
    else {
	for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	    if (i < issue_bandwidth_size)
		ibw[i] = issue_bandwidth[i];
	    else
		ibw[i] = issue_width;
	}
    }


    //
    //  ROB caps
    //
    vector<unsigned> rc(SMT_MAX_THREADS);
    unsigned rob_caps_size = 0;
    if (rob_caps.isValid())
	rob_caps_size = rob_caps.size();

    if (rob_caps_size >= SMT_MAX_THREADS) {
	// copy what we need...
	for (int i = 0; i < SMT_MAX_THREADS; ++i)
	    rc[i] = (rob_caps[i]==0) ? rob_size : rob_caps[i];
    } else {
	for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	    if (i < rob_caps_size)
		rc[i] = (rob_caps[i]==0) ? rob_size : rob_caps[i];
	    else
		rc[i] = rob_size;
	}
    }


    //
    //  I-Count biasing
    //
    vector<unsigned> _ic_bias(SMT_MAX_THREADS);
    unsigned icount_bias_size = 0;
    if (icount_bias.isValid())
	icount_bias_size = icount_bias.size();

    if (icount_bias_size >= SMT_MAX_THREADS) {
	// copy what we need...
	for (int i = 0; i < SMT_MAX_THREADS; ++i)
	    _ic_bias[i] = icount_bias[i];
    } else {
	for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	    if (i < icount_bias_size)
		_ic_bias[i] = icount_bias[i];
	    else
		_ic_bias[i] = 0;
	}
    }


    //
    //  Issue BW weighting
    //
    vector<unsigned> tweights(SMT_MAX_THREADS);
    unsigned thread_weights_size = 0;
    if (thread_weights.isValid())
	thread_weights_size = thread_weights.size();

    if (thread_weights_size >= SMT_MAX_THREADS) {
	// copy what we need...
	for (int i = 0; i < SMT_MAX_THREADS; ++i)
	    tweights[i] = thread_weights[i];
    }
    else {
	for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	    if (i < thread_weights_size)
		tweights[i] = thread_weights[i];
	    else
		tweights[i] = 1;
	}
    }

    //  can't do prioritized issue if thread weights aren't specified
    bool pi = prioritized_issue;
    if (pi && !thread_weights.isValid()) {
	cerr << "Warning: prioritized_issue selected, but thread_wieghts not "
	    "specified\n         --> Turning prioritized_issue OFF\n\n";

	pi = false;
    }



    FullCPU *cpu = new FullCPU(params,
#if FULL_SYSTEM
		      itb, dtb, mem,
#else
		      workload,
#endif // FULL_SYSTEM

		      icache->getInterface(),
		      dcache->getInterface(),
		      sw_prefetch_policy,

		      iq,
		      rob_size,
		      lsq_size,
		      storebuffer_size,

		      fetch_width,
		      lines_to_fetch,
		      num_icache_ports,
		      fetch_branches,
		      ifq_size,
		      decode_to_dispatch,

		      branch_pred,

		      fetch_policy,
		      fetch_pri_enable,
		      _ic_bias,

		      mt_frontend,
		      decode_width,
		      dispatch_to_issue,
		      rc,
		      dispatch_policy,
		      loose_mod_n_policy,
		      use_hm_predictor,
		      use_lr_predictor,
		      use_lat_predictor,
		      max_chains,
		      max_wires,
		      chain_wire_policy,

		      issue_width,
		      ibw,
		      inorder_issue,
		      disambig_mode,
		      pi,
		      fupools,

		      tweights,

		      mispred_recover,
		      fault_handler_delay,
		      iq_comm_latency,

		      commit_width,
		      prioritized_commit,
		      commit_model,
		      pc_sample_interval,
		      (PipeTrace *)ptrace,
                      adaptiveMHA);

    return cpu;
}

REGISTER_SIM_OBJECT("FullCPU", FullCPU)
