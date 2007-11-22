/*
 *
 * bpred.hh - branch predictor interfaces
 *
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
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
 *
 * INTERNET: dburger@cs.wisc.edu
 * US Mail:  1210 W. Dayton Street, Madison, WI 53706
 *
 */

#ifndef __ENCUMBERED_CPU_FULL_BPRED_HH__
#define __ENCUMBERED_CPU_FULL_BPRED_HH__

#define dassert(a) assert(a)

#include "base/misc.hh"
#include "base/statistics.hh"
#include "cpu/smt.hh"
#include "cpu/static_inst.hh"
#include "encumbered/cpu/full/bpred_update.hh"
#include "sim/host.hh"
#include "sim/sim_object.hh"

// forward decls
class FullCPU;

// branch predictor types
enum BPredClass {
    BPredComb,			// combined predictor (McFarling/21264)
    BPredGlobal,		// 2-level w/global history
    BPredLocal,			// 2-level w/local history
    BPred_NUM
};

enum ConfCounterType {
    CNT_RESET,			//  Resetting counter
    CNT_SAT			//  Saturating counter
};

enum {
    PS_INCORRECT,
    PS_CORRECT,
    PS_HIGH_CONF,
    PS_LOW_CONF,
    PS_HC_COR,
    PS_HC_INCOR,
    PS_LC_COR,
    PS_LC_INCOR,
    NUM_PRED_STATE_ENTRIES
};



// branch predictor def
class BranchPred : public SimObject
{
  public:

    // an entry in a BTB
    struct BTBEntry
    {
	Addr addr;		// address of branch being tracked
	Addr target;		// last destination of branch when taken
	BTBEntry *prev;	// lru chaining pointers
	BTBEntry *next;
    };

    // return address stack (ras)
    struct ReturnAddrStack
    {
	int tos;	// top-of-stack
	Addr *stack;	// return-address stack
    };

    BranchPred(const std::string &name,
	       BPredClass bp_class,	// type of predictor
	       unsigned int global_hist_bits,
	       unsigned int global_pred_index_bits,
	       bool global_xor,
	       unsigned int num_local_hist_regs,
	       unsigned int local_hist_bits,
	       unsigned int local_pred_index_bits,
	       bool local_xor,
	       unsigned int meta_pred_index_bits,
	       bool meta_xor,
	       unsigned int btb_sets,  // number of sets in BTB
	       unsigned int btb_assoc, // BTB associativity
	       unsigned int ras_size,  // num entries in ret-addr stack
	       bool conf_pred_enable,
	       unsigned int conf_pred_index_bits,
	       unsigned int conf_pred_ctr_bits,
	       int conf_pred_ctr_thresh, // < 0 means static assignment
	       bool conf_pred_xor,
	       ConfCounterType conf_pred_ctr_type);

    FullCPU *cpu;

    BPredClass bp_class;	// type of predictor

    unsigned int global_hist_reg[SMT_MAX_THREADS];
    unsigned int global_hist_bits;
    uint8_t *global_pred_table;
    unsigned int global_pred_index_bits;
    bool global_xor;

    unsigned int *local_hist_regs;
    unsigned int num_local_hist_regs;
    unsigned int local_hist_bits;
    uint8_t *local_pred_table;
    unsigned int local_pred_index_bits;
    bool local_xor;

    uint8_t *meta_pred_table;
    unsigned int meta_pred_index_bits;
    bool meta_xor;

    int ras_size;		// return-address stack size
    ReturnAddrStack retAddrStack[SMT_MAX_THREADS]; // return-address stack

    struct
    {
	int sets;		// num BTB sets
	int assoc;		// BTB associativity
	BTBEntry *btb_data;	// BTB addr-prediction table
    } btb;

    bool conf_pred_enable;	// enable confidence predictor
    uint8_t *conf_pred_table;
    unsigned int conf_pred_index_bits;
    unsigned int conf_pred_ctr_bits;
    int conf_pred_ctr_thresh;	// < 0 means static assignment
    unsigned int conf_pred_xor;
    unsigned int conf_pred_ctr_type;

    // stats
    //Counter lookups[SMT_MAX_THREADS];	// Counts all lookups
    Stats::Vector<> lookups;
    Stats::Vector<> conf_chc;
    Stats::Vector<> conf_clc;
    Stats::Vector<> conf_ihc;
    Stats::Vector<> conf_ilc;
    Stats::Vector<> cond_predicted;
    Stats::Vector<> cond_correct;
    // # of BTB lookups (spec & misspec)
    Stats::Vector<> btb_lookups;
    // # of BTB hits    ( "       "    )
    Stats::Vector<> btb_hits;
    // # of targets fetched from BTB (committed)
    Stats::Vector<> used_btb;
    // # of correct targets from BTB
    Stats::Vector<> btb_correct;
    // number of committed instrs using RAS
    Stats::Vector<> used_ras;
    // num correct return-address predictions
    Stats::Vector<> ras_correct;
    unsigned pred_state_table_size;
    Stats::Vector<> pred_state[NUM_PRED_STATE_ENTRIES];

    Stats::Formula lookup_rate;
    Stats::Formula dir_accuracy;
    Stats::Formula btb_hit_rate;
    Stats::Formula btb_accuracy;
    Stats::Formula ras_accuracy;
    Stats::Formula conf_sens;
    Stats::Formula conf_pvp;
    Stats::Formula conf_spec;
    Stats::Formula conf_pvn;

    Stats::Distribution<> corr_conf_dist;
    Stats::Distribution<> incorr_conf_dist;


    // initialize CPU pointer
    void setCPU(FullCPU *_cpu) { cpu = _cpu; }

    void regStats( );
    void regFormulas();
    // query predictor for predicted target address
    enum LookupResult {
	Predict_Not_Taken,
	Predict_Taken_With_Target,	// taken & BTB or RAS hit
	Predict_Taken_No_Target		// taken but BTB miss
    };

    LookupResult
    lookup(int thread_number,
	   Addr baddr,	// branch address
	   const StaticInstBasePtr &brInst,
	   // the following are all output parameters
	   Addr *pred_target_ptr, // not touched if no target provided
	   BPredUpdateRec *dir_update_ptr, // pred state pointer
	   enum conf_pred *confidence = NULL);

    // Speculative execution can corrupt the ret-addr stack.  So for
    // each lookup we return the top-of-stack (TOS) at that point; a
    // mispredicted branch, as part of its recovery, restores the TOS
    // using this value -- hopefully this uncorrupts the stack.
    void
    recover(int thread_number,
	    Addr baddr,	// branch address
	    BPredUpdateRec *dir_update_ptr);

    // update the branch predictor, only useful for stateful predictors;
    // updates entry for instruction type OP at address BADDR.  BTB only
    // gets updated for branches which are taken.  Inst was determined to
    // jump to address BTARGET and was taken if TAKEN is non-zero.
    // Predictor statistics are updated with result of prediction,
    // indicated by CORRECT and PRED_TAKEN, predictor state to be updated
    // is indicated by *DIR_UPDATE_PTR (may be NULL for jumps, which
    // shouldn't modify state bits).  Note if bpred_update is done
    // speculatively, branch-prediction may get polluted.
    void
    update(int thread_number,
	   Addr baddr,	// branch address
	   Addr btarget,	// resolved branch target
	   int taken,	// non-zero if branch was taken
	   int pred_taken,	// non-zero if branch was pred taken
	   int correct,	// was earlier prediction correct?
	   const StaticInstBasePtr &brInst,	// static instruction
	   BPredUpdateRec *dir_update_ptr); // pred state pointer

    /// Pop top element off of return address stack.
    void popRAS(int thread_number);
};

#endif // __ENCUMBERED_CPU_FULL_BPRED_HH__
