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

#ifndef __ENCUMBERED_CPU_FULL_CPU_HH__
#define __ENCUMBERED_CPU_FULL_CPU_HH__

#include <sstream>

#include "base/hashmap.hh"
#include "base/statistics.hh"
#include "base/str.hh"
#include "config/full_system.hh"
#include "cpu/base.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/bpred.hh"
#include "encumbered/cpu/full/fetch.hh"
#include "encumbered/cpu/full/floss_reasons.hh"
#include "encumbered/cpu/full/fu_pool.hh"
#include "encumbered/cpu/full/iq/iq_station.hh"
#include "encumbered/cpu/full/iq/segmented/chain_wire.hh"
#include "encumbered/cpu/full/issue.hh"
#include "encumbered/cpu/full/machine_queue.hh"
#include "encumbered/cpu/full/pipetrace.hh"
#include "encumbered/cpu/full/spec_state.hh"
#include "encumbered/cpu/full/storebuffer.hh"
#include "encumbered/cpu/full/thread.hh"
#include "encumbered/cpu/full/writeback.hh"

#include "mem/cache/miss/adaptive_mha.hh"
#include "mem/accounting/interference_manager.hh"
#include "mem/requesttrace.hh"
#include "mem/accounting/memory_overlap_estimator.hh"


class BaseIQ;
class Process;

class DecodeDispatchQueue;
struct IcacheOutputBuffer;
class ROBStation;

struct ClusterSharedInfo;

class IntervalStats;
class MemInterface;

class AdaptiveMHA;
class InterferenceManager;
class MemoryOverlapEstimator;

class FullCPU : public BaseCPU
{
private:
	//Tick noCommitCycles;
	Tick tmpBlockedCycles;
	Tick l1MissStallCycles;
	std::string statsOrderFileName;

	InterferenceManager* interferenceManager;
	int issueStallMessageCounter;
	bool stallMessageIssued;

	bool hasDumpedStats;

	AdaptiveMHA* amha;

	int committedSinceLast;

	int committedTraceCounter;
	int stallCycleTraceCounter;
	Tick lastDumpTick;
	RequestTrace committedInstTrace;

	int quitOnCPUID;

	bool isStalled;
	Addr stalledOnAddr;
	InstSeqNum stalledOnInstSeqNum;
	MemoryOverlapEstimator* overlapEstimator;

public:
	////////////////////////////////////////////
	//
	//  Types...
	//
	////////////////////////////////////////////
	enum DispatchPolicyEnum {MODULO_N, THREAD_PER_QUEUE, DEPENDENCE};
	enum MemDisambiguationEnum {DISAMBIG_CONSERVATIVE,
		DISAMBIG_NORMAL,
		DISAMBIG_ORACLE};
	enum SoftwarePrefetchPolicy { SWP_ENABLE, SWP_DISABLE, SWP_SQUASH };
	enum CommitModelEnum {
		COMMIT_MODEL_SMT, COMMIT_MODEL_PERTHREAD, COMMIT_MODEL_SSCALAR,
		COMMIT_MODEL_RR
	};

	//typedef CPUTraits<ISATraitsType::ISAType, CPU> CPUTraitsType;

	struct FetchInfo
	{
		unsigned int instructionCount;
		// This is the last fetch we attempted.  We may want to keep a list
		// of addresses, but this is good enough for prefetching.  We
		// did a cache access to this address.
		struct FetchRecord
		{
			int thread;
			int asid;
			Addr addr;
			MemAccessResult result;

			FetchRecord(int t, int a, Addr ad, MemAccessResult r)
			: thread(t), asid(a), addr(ad), result(r) { }
		};
		typedef std::vector<FetchRecord> FetchList;
		FetchList fetches;

		FetchInfo() : instructionCount(0) { }

		typedef FetchList::iterator iterator;
		iterator begin() { return(fetches.begin()); }
		iterator end() { return(fetches.end()); }

		void addFetch(int thread, int asid, Addr fetchAddress,
				MemAccessResult result) {
			fetches.push_back(FetchRecord(thread, asid, fetchAddress, result));
		}
	};


	enum ChainCreationReason {
		CHAIN_CR_NO_IDEPS=0,       // Inst has no outstanding ideps
		CHAIN_CR_DEPTH,            // Chain has reached max depth
		CHAIN_CR_LOAD,             // Inst is a load
		CHAIN_CR_MULT_IDEPS,       // Chain has multiple ideps
		NUM_CHAIN_CR_CLASSES
	};


	enum HitMissPredFunction {
		HMP_LATENCY = 0, // add latency to load's WB time if predicted miss
		HMP_HEAD_SEL,    // don't mark load as a head if it's predicted to hit
		//    and we have less than 25% of chains free
		HMP_BOTH,        // do both of the above...
		NUM_HMP_FUNCS
	};


	////////////////////////////////////////////
	//
	//  Friends...
	//
	////////////////////////////////////////////

	Fault execute_instruction(DynInst *fetched_inst, int thread_number);

	////////////////////////////////////////////
	//
	// Constructor
	//
	////////////////////////////////////////////

	struct Params : public BaseCPU::Params
	{
	};

	void registerProcessHalt();
	void restartProcess();

	Addr getCacheAddr(Addr memaddr);

	FullCPU(FullCPU::Params *params,
#if FULL_SYSTEM
			AlphaITB *itb, AlphaDTB *dtb, FunctionalMemory *mem,
#else
			std::vector<Process *> workload,
#endif // FULL_SYSTEM

			//
			// Caches
			//
			MemInterface *_icache_interface,
			MemInterface *_dcache_interface,
			SoftwarePrefetchPolicy _softwarePrefetchPolicy,

			//
			//  Internal Structures
			//
			std::vector<BaseIQ *> IQ,
			unsigned ROB_size,
			unsigned LSQ_size,
			unsigned storebuffer_size,


			//
			//  Fetch
			//
			int fetch_width,
			int lines_to_fetch,
			int num_icache_ports,
			int fetch_branches,
			int ifq_size,
			int decode_dispatch_latency,

			BranchPred *bpred,

			int fetch_policy,
			bool fetch_priority_enable,
			std::vector<unsigned> icount_bias,

			//
			//  Decode/Dispatch
			//
			bool mt_frontend,
			int decode_width,
			int dispatch_to_issue_latency,
			std::vector<unsigned> _rob_caps,
			DispatchPolicyEnum disp_policy,
			bool loose_mod_n_policy,
			bool use_hm_predictor,
			bool use_lr_predictor,
			bool use_lat_predictor,
			unsigned max_chains,
			unsigned max_wires,
			ChainWireInfo::MappingPolicy chainWirePolicy,
			// hmp_func is hard-coded inside constructor for now...

			//
			//  Issue
			//
			int issue_width,
			std::vector<unsigned> _issue_bandwidth,
			bool inorder_issue,
			MemDisambiguationEnum disambig_mode,
			bool prioritized_issue,
			std::vector<FuncUnitPool *> fupools,

			std::vector<unsigned> _thread_weights,

			//
			// Writeback
			//
			int mispred_fixup_penalty,
			int fault_handler_delay,
			unsigned iq_comm_latency,

			//
			// Commit
			//
			int commit_width,
			bool prioritized_commit,
			CommitModelEnum commit_model,

			//
			//  Other
			//
			int _pc_sample_interval,
			PipeTrace *pt,

			AdaptiveMHA* _amha,
			InterferenceManager* _intMan,
			int _quitOnCPUID,
			MemoryOverlapEstimator* _overlapEst,
			int _commitTraceFrequency
	);


	////////////////////////////////////////////
	//
	// Destructor
	//
	////////////////////////////////////////////
	~FullCPU();

	void takeOverFrom(BaseCPU *oldCPU);

	// startup callback: initialization after unserialization
	void startup();

	////////////////////////////////////////////////////////////////////////
	//
	// CONFIGURATION PARAMETERS
	//
	////////////////////////////////////////////////////////////////////////

	//
	//  Internal Structures
	//
	unsigned ROB_size;
	unsigned LSQ_size;
	unsigned storebuffer_size;

	//
	// Caches
	//
	MemInterface *icacheInterface;
	MemInterface *dcacheInterface;
	SoftwarePrefetchPolicy softwarePrefetchPolicy;

	//
	//  Fetch
	//
	int fetch_width;
	int lines_to_fetch;
	int icache_block_size;
	int insts_per_block;
	int num_icache_ports;
	int fetch_branches;
	int ifq_size;
	int decode_dispatch_latency;

	BranchPred *branch_pred;

	int fetch_policy;
	bool fetch_priority_enable;
	std::vector<unsigned> static_icount_bias;

	//
	//  Decode/Dispatch
	//
	bool mt_frontend;
	int decode_width;
	int dispatch_to_issue_latency;
	DispatchPolicyEnum dispatch_policy;
	std::vector<unsigned> rob_cap;
	bool loose_mod_n_policy;

	// chaining stuff...
	bool use_hm_predictor;
	bool use_lr_predictor;
	bool use_lat_predictor;
	unsigned max_chains;
	unsigned max_wires;
	ChainWireInfo::MappingPolicy chainWirePolicy;
	HitMissPredFunction hmp_func;

	//
	//  Issue
	//
	int issue_width;
	std::vector<unsigned> issue_bandwidth;
	bool inorder_issue;
	MemDisambiguationEnum disambig_mode;
	bool prioritize_issue;
	std::vector<FuncUnitPool *> FUPools;
	std::vector<unsigned> issue_thread_weights;

	//
	//  Writeback
	//
	int mispred_fixup_penalty;
	int fault_handler_delay;
	unsigned iq_comm_latency;

	//
	//  Commit
	//
	int commit_width;
	CommitModelEnum commit_model;
	bool prioritized_commit;
	int commitTraceFrequency;

	//
	//  Other
	//
	PipeTrace *ptrace;

	////////////////////////////////////////////////////////////////////////
	//
	//  Non-parameter Variables (not statistics)
	//
	////////////////////////////////////////////////////////////////////////

	// Note that this has exactly the same contents as BaseCPU's
	// contexts vector.  However, in this case we know they are
	// SpecExecContext ptrs, so keeping this array around avoids
	// having to cast the members of the contexts vector (which are
	// just plain ExecContext ptrs) when we really need a
	// SpecExecContext ptr.
	SpecExecContext *thread[SMT_MAX_THREADS];

	//----------------------------------------------------------------------
	//
	//  Caches
	//
	MemInterface *itlb;

	MemInterface *dtlb;

	//----------------------------------------------------------------------
	//
	//  Internal Structures
	//
	BaseIQ **IQ;
	BaseIQ *LSQ;
	class MachineQueue<ROBStation> ROB;
	StoreBuffer *storebuffer;

	unsigned numIQueues;  //  ?????
	unsigned IQNumSlots;  //  ?????

	//----------------------------------------------------------------------
	//
	//  Fetch
	//
	unsigned int icache_ports_used_last_fetch;
	InstSeqNum next_fetch_seq;
	int fetch_stall[SMT_MAX_THREADS];
	int fetch_fault_count[SMT_MAX_THREADS];
	enum FetchEndCause fid_cause[SMT_MAX_THREADS];

	// The ifetch queue (ifq) is a queue that buffers fetched
	// instructions until they can enter the decode pipeline.
	FetchQueue ifq[SMT_MAX_THREADS];

	IcacheOutputBuffer *icache_output_buffer[SMT_MAX_THREADS];

	ThreadListElement fetch_list[SMT_MAX_THREADS];
	ThreadInfo thread_info[SMT_MAX_THREADS];

	//----------------------------------------------------------------------
	//
	//  Decode/Dispatch
	//
	unsigned dispatch_width;
	InstSeqNum dispatch_seq;

	DecodeDispatchQueue *decodeQueue;
	unsigned first_decode_thread;
	unsigned dispatch_starting_iqueue[SMT_MAX_THREADS];
	Tick lastDispatchTime[SMT_MAX_THREADS];

	int free_int_physical_regs;
	int free_fp_physical_regs;

	CreateVector create_vector[SMT_MAX_THREADS];
	SpecStateList state_list;

	bool rob_cap_active[SMT_MAX_THREADS];
	bool iq_cap_active[SMT_MAX_THREADS];

	unsigned lastChainUsed;
	class ChainWireInfo *chainWires;

	ClusterSharedInfo *clusterSharedInfo;

	//  Clustering:  which IQ we should use for mod-n instruction placement
	unsigned mod_n_queue_idx;
	bool dispatchStalled;
	bool regRenameStalled;

	//----------------------------------------------------------------------
	//
	//  Issue
	//
	unsigned numFUPools;
	unsigned issue_starting_iqueue;
	unsigned issue_starting_fu_pool;
	unsigned issue_current_fupool_for_sb;

	bool mem_set_this_inst;

	InstSeqNum expected_inorder_seq_num;

	unsigned hp_thread;       // current HP thread
	Tick hp_thread_change;  // when to change HP threads

	unsigned n_issued[SMT_MAX_THREADS];  // currently unused part of fetch-loss
	unsigned n_issued_total;

	struct lsq_store_info_t *store_info[SMT_MAX_THREADS];

	//----------------------------------------------------------------------
	//
	//  Writeback
	//
	EventQueue writebackEventQueue;

	//----------------------------------------------------------------------
	//
	//  Commit
	//

	unsigned n_committed[SMT_MAX_THREADS];  // unused part of fetch-loss
	unsigned n_committed_total;

	Addr commitPC[SMT_MAX_THREADS];	/* Last PC committed */

	unsigned rr_commit_last_thread;
	bool itcaCommitStalled;

	//----------------------------------------------------------------------
	//
	//  Other
	//

	//  ptrace
	InstSeqNum correctPathSeq[SMT_MAX_THREADS];

	//  fetch-loss
	FlossState floss_state;
	int floss_this_cycle;

	// maximum depth of dependence graph: set after call to
	// update_dependence_depths()
	int max_dependence_depth;

	unsigned chain_heads_in_rob;


	////////////////////////////////////////////////////////////////////////
	//
	//  Statistics
	//
	////////////////////////////////////////////////////////////////////////

	//----------------------------------------------------------------------
	//
	//  Internal Structures
	//
	// occupancy counters
	Stats::Scalar<> ROB_fcount;
	Stats::Formula ROB_full_rate;
	Counter lsq_fcount;
	Stats::Scalar<> IFQ_count;	// cumulative IFQ occupancy
	Stats::Formula IFQ_occupancy;
	Stats::Formula IFQ_latency;
	Stats::Vector<> IFQ_fcount; // cumulative IFQ full count
	Stats::Formula IFQ_full_rate;
	Stats::Vector<>  ROB_count;	 // cumulative ROB occupancy
	Stats::Formula ROB_occ_rate;
	Stats::VectorDistribution<> ROB_occ_dist;


	//----------------------------------------------------------------------
	//
	//  Fetch
	//

	Stats::Scalar<> fetch_decisions;
	Stats::Scalar<> fetch_idle_cycles;
	Stats::Scalar<> fetch_idle_cycles_cache_miss;
	Stats::Scalar<> fetch_idle_icache_blocked_cycles;

	Stats::Vector<> qfull_iq_occupancy;
	Stats::VectorDistribution<> qfull_iq_occ_dist_;
	Stats::Vector<> qfull_rob_occupancy;
	Stats::VectorDistribution<> qfull_rob_occ_dist_;

	// only really useful with ptrace...
	Stats::Scalar<> currentROBCount;

	Stats::Scalar<> conf_predictions;
	Stats::Scalar<> conf_updates;

	Stats::Vector<> priority_changes;

	Stats::Vector<> fetch_chances;
	Stats::Vector<> fetched_inst;
	Stats::Vector<> fetched_branch;
	Stats::Vector<> fetch_choice_dist;
	Stats::Distribution<> fetch_nisn_dist;
	Stats::Distribution<> *fetch_nisn_dist_;

	Stats::Formula idle_rate;
	Stats::Formula branch_rate;
	Stats::Formula fetch_rate;
	Stats::Formula fetch_chance_pct;

	//
	//  Fetch loss counters
	//
	Stats::Vector2d<> stat_floss_icache;
	Stats::Vector2d<> stat_floss_iqfull_deps;
	Stats::Vector2d<> stat_floss_iqfull_fu;
	Stats::Vector2d<> stat_floss_iqfull_dcache;
	Stats::Vector2d<> stat_floss_iqfull_other;
	Stats::Vector2d<> stat_floss_lsqfull_deps;
	Stats::Vector2d<> stat_floss_lsqfull_fu;
	Stats::Vector2d<> stat_floss_lsqfull_other;
	Stats::Vector2d<> stat_floss_lsqfull_dcache;
	Stats::Vector2d<> stat_floss_robfull_dcache;
	Stats::Vector2d<> stat_floss_robfull_fu;
	Stats::Vector2d<> stat_floss_robfull_other;
	Stats::Vector2d<> stat_floss_qfull_other;
	Stats::Vector2d<> stat_floss_other;

	std::vector<std::vector<double> > floss_icache;
	std::vector<std::vector<double> > floss_iqfull_deps;
	std::vector<std::vector<double> > floss_iqfull_fu;
	std::vector<std::vector<double> > floss_iqfull_dcache;
	std::vector<std::vector<double> > floss_iqfull_other;
	std::vector<std::vector<double> > floss_lsqfull_deps;
	std::vector<std::vector<double> > floss_lsqfull_fu;
	std::vector<std::vector<double> > floss_lsqfull_other;
	std::vector<std::vector<double> > floss_lsqfull_dcache;
	std::vector<std::vector<double> > floss_robfull_dcache;
	std::vector<std::vector<double> > floss_robfull_fu;
	std::vector<std::vector<double> > floss_robfull_other;
	std::vector<std::vector<double> > floss_qfull_other;
	std::vector<std::vector<double> > floss_other;

	//----------------------------------------------------------------------
	//
	//  Decode/Dispatch
	//

	Stats::Scalar<> secondChoiceCluster;
	Stats::Scalar<> secondChoiceStall;

	Counter used_int_physical_regs[SMT_MAX_THREADS];
	Counter used_fp_physical_regs[SMT_MAX_THREADS];

	Stats::Vector<> reg_int_thrd_occ;
	Stats::Vector<> reg_fp_thrd_occ;
#if 0
	stat_stat_t *chain_create_dist;

	stat_stat_t *inst_class_dist;
#endif
	Stats::Vector<> chain_create_dist;
	Stats::Vector<> inst_class_dist;

	std::vector<uint64_t> dispatch_count;
	Stats::Vector<> dispatch_count_stat;
	Stats::Vector<> dispatched_serializing;
	Stats::Vector<> dispatch_serialize_stall_cycles;
	Stats::Vector<> chain_heads;
	Stats::Formula chain_head_frac;
	Stats::Vector<> chains_insuf;
	Stats::Formula chains_insuf_rate;

	Stats::Vector<> dispatched_ops;
	Stats::Formula dispatched_op_rate;

	Stats::Vector<> rob_cap_events;
	Stats::Vector<> rob_cap_inst_count;
	Stats::Vector<> iq_cap_events;
	Stats::Vector<> iq_cap_inst_count;

	Stats::Formula dispatch_rate;

	Stats::Vector<> mod_n_disp_stalls;
	Stats::Vector<> mod_n_disp_stall_free;
	Stats::Formula mod_n_stall_avg_free;
	Stats::Formula mod_n_stall_frac;

	Stats::Scalar<> reg_int_full;
	Stats::Scalar<> reg_fp_full;

	Stats::Formula reg_int_occ_rate;
	Stats::Formula reg_fp_occ_rate;

	Stats::Scalar<> insufficient_chains;

	Stats::Vector<> two_op_inst_count;
	Stats::Formula two_input_ratio;
	Stats::Vector<> one_rdy_inst_count;
	Stats::Formula one_rdy_ratio;

	//----------------------------------------------------------------------
	//
	//  Issue
	//
	// total number of instructions executed
	Stats::Vector<> exe_inst;
	Stats::Vector<> exe_swp;
	Stats::Vector<> exe_nop;
	Stats::Vector<> exe_refs;
	Stats::Vector<> exe_loads;
	Stats::Vector<> exe_branches;

	Stats::Vector<> issued_ops;

	// total number of loads forwaded from LSQ stores
	Stats::Vector<> lsq_forw_loads;

	// total number of loads ignored due to invalid addresses
	Stats::Vector<> inv_addr_loads;

	// total number of software prefetches ignored due to invalid addresses
	Stats::Vector<> inv_addr_swpfs;

	// total non-speculative bogus addresses seen (debug var)
	Counter sim_invalid_addrs;
	Stats::Vector<> fu_busy;  //cumulative fu busy

	// ready loads blocked due to memory disambiguation
	Stats::Vector<> lsq_blocked_loads;

	Stats::Scalar<> lsqInversion;

	Stats::Vector<> n_issued_dist;
	Stats::VectorDistribution<> issue_delay_dist;

	Stats::VectorDistribution<> queue_res_dist;

	Stats::Vector<> stat_fu_busy;
	Stats::Vector2d<> stat_fuBusy;
	Stats::Vector<> dist_unissued;
	Stats::Vector2d<> stat_issued_inst_type;

	Stats::Formula misspec_cnt;
	Stats::Formula misspec_ipc;
	Stats::Formula issue_rate;
	Stats::Formula issue_stores;
	Stats::Formula issue_op_rate;
	Stats::Formula fu_busy_rate;
	Stats::Formula commit_stores;
	Stats::Formula commit_ipc;
	Stats::Formula commit_ipb;
	Stats::Formula lsq_inv_rate;

	/**
	 *These are never registered anywhere, even in old stats package...???
	 */
	struct smt_dist_stat_t *dependence_depth_dist;
	struct smt_dist_stat_t *misspec_dependence_depth_dist;

	//----------------------------------------------------------------------
	//
	//  Writeback
	//

	Stats::Vector<> writeback_count;
	Stats::Vector<> producer_inst;
	Stats::Vector<> consumer_inst;
	Stats::Vector<> wb_penalized;

	Stats::Formula wb_rate;
	Stats::Formula wb_fanout;
	Stats::Formula wb_penalized_rate;

	Stats::Distribution<> pred_wb_error_dist;

	//----------------------------------------------------------------------
	//
	//  Commit
	//

	// total number of instructions committed
	Stats::Vector<> stat_com_inst;
	Stats::Vector<> stat_com_swp;
	Stats::Vector<> stat_com_refs;
	Stats::Vector<> stat_com_loads;
	Stats::Vector<> stat_com_membars;
	Stats::Vector<> stat_com_branches;

	std::vector<uint64_t> com_inst;
	std::vector<uint64_t> com_loads;

	virtual Counter totalInstructions() const
	{
		Counter total = 0;
		int size = com_inst.size();
		for (int i = 0; i < size; ++i)
			total += com_inst[i];
		return total;
	}

	Stats::Distribution<> n_committed_dist;

	Stats::Scalar<> commit_eligible_samples;
	Stats::Vector<> commit_eligible;
	Stats::Formula bw_lim_avg;
	Stats::Formula bw_lim_rate;

	Stats::VectorStandardDeviation<> commit_bwlimit_stat;

	Stats::Scalar<> commit_total_mem_stall_time;
	Stats::Scalar<> commit_cycles_empty_ROB;

	//----------------------------------------------------------------------
	//
	//  Other
	//
	IntervalStats *istat;
	Counter dependence_depth_count;



	////////////////////////////////////////////////////////////////////////
	//
	//  Methods
	//
	////////////////////////////////////////////////////////////////////////

	public:
		// main simulation loop (one cycle)
		void tick();

	private:
		class TickEvent : public Event
		{
		private:
			FullCPU *cpu;

		public:
			TickEvent(FullCPU *c)
			: Event(&mainEventQueue, CPU_Tick_Pri), cpu(c) { }

			void process() { cpu->tick(); }

			virtual const char *description() { return "tick"; }
		};

		TickEvent tickEvent;

		class ProcessRestartEvent : public Event
		{
		private:
			FullCPU *cpu;

		public:
			ProcessRestartEvent(FullCPU *c)
			: Event(&mainEventQueue, Sim_Exit_Pri), cpu(c) { }

			void process() {
				cpu->restartProcess();
				delete this;
			}

			virtual const char *description() { return "process restart event"; }
		};

		bool restartHaltedProcesses;
		ProcessRestartEvent* restartEvent;


		// PC Sampling Profile
		class PCSampleEvent : public Event
		{
		private:

			FullCPU *cpu;
			int	 interval;

		public:

			PCSampleEvent(int _interval, FullCPU *_cpu);

			~PCSampleEvent() { }

			void process();
		};

		PCSampleEvent *pcSampleEvent;

		m5::hash_map<Addr, Counter> pcSampleHist;

		void dumpPCSampleProfile();

		public:
			void change_thread_state(int thread_number, int activate, int priority);

			virtual void activateContext(int thread_num, int delay);

			//----------------------------------------------------------------------
			//
			//  Caches
			//


			//----------------------------------------------------------------------
			//
			//  Internal Structures
			//
			unsigned IQNumInstructions();
			unsigned IQNumInstructions(unsigned thread);
			unsigned IQNumReadyInstructions();
			unsigned IQNumReadyInstructions(unsigned thread);
			unsigned IQFreeSlots();
			unsigned IQFreeSlotsX(unsigned idx);
			unsigned IQFreeSlots(unsigned thread);
			BaseIQ::iterator IQOldestInstruction();
			BaseIQ::iterator IQOldestInstruction(unsigned thread);
			bool IQCapMet(unsigned thread);

			unsigned IQLeastFull();
			unsigned IQMostFull();

			unsigned IQSize() { return IQNumSlots; }
			BaseIQ::rq_iterator IQIssuableList();

			//  Debugging
			void fuDump();
			void fuDump(int pool);

			void dumpIQ();
			void ROBDump();


			//----------------------------------------------------------------------
			//
			//  Fetch
			//
			void fetch_init();
			void initialize_fetch_list(int);
			void fetch_squash(int thread_number);

			void clear_fetch_stall(Tick when, int thread_number, int stall_type);

			void round_robin_policy(ThreadListElement *thread_list);
			void icount_policy(ThreadListElement *thread_list);


			void fetch();
			void update_icounts();
			void choose_next_thread(ThreadListElement *thread_list);

			// Align an address (typically a PC) to the start of an I-cache block.
			// We fold in the PISA 64- to 32-bit conversion here as well.
			Addr icacheBlockAlignPC(Addr addr)
			{
				addr = TheISA::realPCToFetchPC(addr);
				return (addr & ~((Addr)icache_block_size - 1));
			}

			std::pair<DynInst *, Fault> fetchOneInst(int thread_number);
			std::pair <int, bool> fetchOneLine(int thread_number, int max_to_fetch,
					int &branch_cnt,
					bool entering_interrupt);
			int fetchOneThread(int thread_number, int max_to_fetch);

			void instructionFetchComplete(DynInst *inst);

			void fetchRegStats();
			void fetch_dump();



			//----------------------------------------------------------------------
			//
			//  Decode/Dispatch
			//
			void cv_init();
			void cv_init_spec_thread(unsigned thread);

			void dispatch_init();

			void start_decode();
			int choose_decode_thread();
			unsigned choose_dependence_cluster(DynInst *);
			void updateDispatchStalled(bool stalled);
			void updateRegRenameStalled(bool stalled);
			void dispatch();
			enum DispatchEndCause checkThreadForDispatch(unsigned t,
					unsigned idx, unsigned insts);
			enum DispatchEndCause checkGlobalResourcesForDispatch(unsigned insts);

			int choose_iqueue(unsigned thread);
			unsigned dispatch_thread(unsigned thread, unsigned iq_idx, unsigned max,
					enum DispatchEndCause &cause);
			ROBStation *dispatch_one_inst(DynInst *inst, unsigned iq_idx);


			void fixup_btb_miss(DynInst *inst);

			NewChainInfo choose_chain(DynInst *inst, unsigned clust);
			bool checkClusterForDispatch(unsigned clust, bool chainHead);


			//----------------------------------------------------------------------
			//
			//  Issue
			//
			void lsq_refresh();

			void issue_init();
			void issue();
			bool sb_issue(StoreBuffer::iterator i, unsigned pool_num);
			bool iq_issue(BaseIQ::iterator i, unsigned pool_num);
			bool lsq_issue(BaseIQ::iterator i, unsigned pool_num);
			bool issue_load(BaseIQ::iterator lsq, int *latency);
			bool issue_prefetch(BaseIQ::iterator rs, int *latency);

			void release_fu();

			void update_exe_inst_stats(DynInst *inst);
			bool find_idep_to_blame(BaseIQ::iterator inst, int thread);


			//----------------------------------------------------------------------
			//
			//  Writeback
			//
			void writeback();
			void recover(ROBStation *ROB_branch_entry, int branch_thread);

			void remove_LSQ_element(BaseIQ::iterator);
			void remove_ROB_element(ROBStation *);


			//----------------------------------------------------------------------
			//
			//  Commit
			//
			void commit();
			bool eligible_to_commit(ROBStation *rs,
					enum CommitEndCause *reason);
			void commit_one_inst(ROBStation *rs);

			unsigned oldest_inst(ROBStation ***clist, unsigned *cnum, unsigned *cx);

			int getStalledL1MissCycles(){
				int tmp = l1MissStallCycles;
				l1MissStallCycles = 0;
				return tmp;
			}

			bool requestInROB(MemReqPtr& req, int blockSize);
			int getCommittedInstructions();
			void updateITCACommitStalled(bool stalled);


			//----------------------------------------------------------------------
			//
			//  Other
			//

			//  Fetch-Loss stuff
			void flossRegStats();
			void flossReset();

			void flossRecord(FlossState *, int num_fetched[]);

			friend struct FlossState;

#if 0
			void blame_fu(int thread, int base_idx, double total_loss,
					OpClass *fu_classes);
			void blame_commit_stage(FlossState *state,
					int thread, double total_loss, int base_idx);
			void blame_issue_stage(FlossState *state, int thread,
					double total_loss, int base_idx);
			void blame_dispatch_stage(FlossState *state, int thread,
					double loss, int idx);

			void check_counters(FlossState *state);
			Counter total_floss();
#endif

			//  Register stats...
			void dispatchRegStats();
			void issueRegStats();
			void writebackRegStats();
			void commitRegStats();
			void pred_queueRegStats();

			void update_com_inst_stats(DynInst *inst);

			// Register FOrmulas
			void dispatchRegFormulas();
			void fetchRegFormulas();
			void writebackRegFormulas();
			void commitRegFormulas();
			void pred_queueRegFormulas();
			void issueRegFormulas();

			// override of SimObject method: register statistics
			virtual void regStats();
			virtual void regFormulas();

			virtual BranchPred *getBranchPred();
};

/////////////////////////////////////////////////////
//
//   Results from call to choose_chain() use this
//   enum and structure
//
struct NewChainInfo
{
	IQStation::IDEP_info idep_info[TheISA::MaxInstSrcRegs];

	unsigned head_chain;

	int  hm_prediction;
	int  pred_last_op_index;
	int  lr_prediction;

	int  suggested_cluster;

	bool head_of_chain;
	bool out_of_chains;

	//  constructor
	NewChainInfo() {
		out_of_chains = false;
		head_of_chain = false;

		pred_last_op_index = -1;
	}

	std::string str_dump() {
		std::ostringstream s;

		s << "F:";
		for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
			if (idep_info[i].chained) {
				s << idep_info[i].follows_chain
				<< ", lat=" << idep_info[i].delay;
			}
		}
		s << " H:";
		if (head_of_chain)
			s << head_chain;
		s << std::endl;

		return s.str();
	}

	void dump() {
		std::cout << "New Chain Info:\n";
		std::cout << "   Follows chain(s):\n";

		for (int i = 0; i < TheISA::MaxInstSrcRegs; ++i) {
			if (idep_info[i].chained) {
				std::cout << "     chain=" << idep_info[i].follows_chain
				<< ", lat=" << idep_info[i].delay
				<< ", depth=" << idep_info[i].chain_depth << std::endl;
			}
		}

		if (head_of_chain)
			std::cout << "   Head of chain: " << head_chain << std::endl;
	}
};
#endif // __ENCUMBERED_CPU_FULL_CPU_HH__
