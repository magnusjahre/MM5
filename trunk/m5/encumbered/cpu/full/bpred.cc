/*
 * bpred.cc - branch predictor routines
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

#include <cassert>
#include <cmath>
#include <string>

#include "base/cprintf.hh"
#include "base/misc.hh"
#include "base/statistics.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/bpred.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "sim/builder.hh"
#include "sim/host.hh"
#include "sim/stats.hh"

using namespace std;

#define NBIT_MASK(n)		((1 << (n)) - 1)
#define IS_POWER_OF_TWO(n)	(((n) & ((n) - 1)) == 0)

#define BP_VERBOSE 0


/*
 * Is two-bit counter value strongly biased?
 */
#define TWOBIT_STRONG(x)	((x) == 0 || (x) == 3)

/*
 * Do  two-bit counter values agree?
 */
#define TWOBIT_AGREE(x,y)	(((x) >= 2) == ((y) >= 2))

//
// This should be stuck into the BranchPred structure, but I'm not
// quite sure what it's for... (other than something
// confidence-related)
static int conf_table[64];

// branch predictor constructor
BranchPred::BranchPred(const string &_name,
		       BPredClass _bp_class, // type of predictor
		       unsigned int _global_hist_bits,
		       unsigned int _global_pred_index_bits,
		       bool _global_xor,
		       unsigned int _num_local_hist_regs,
		       unsigned int _local_hist_bits,
		       unsigned int _local_pred_index_bits,
		       bool _local_xor,
		       unsigned int _meta_pred_index_bits,
		       bool _meta_xor,
		       unsigned int btb_sets,  // number of sets in BTB
		       unsigned int btb_assoc, // BTB associativity
		       unsigned int _ras_size,  // num entries in RAS
		       bool _conf_pred_enable,
		       unsigned int _conf_pred_index_bits,
		       unsigned int _conf_pred_ctr_bits,
		       int _conf_pred_ctr_thresh,
		       bool _conf_pred_xor,
		       ConfCounterType _conf_pred_ctr_type)
    : SimObject(_name),
      cpu(NULL),	// initialized later via setCPU()
      bp_class(_bp_class),
      global_hist_bits(_global_hist_bits),
      global_pred_table(NULL),
      global_pred_index_bits(_global_pred_index_bits),
      global_xor(_global_xor),
      num_local_hist_regs(_num_local_hist_regs),
      local_hist_bits(_local_hist_bits),
      local_pred_table(NULL),
      local_pred_index_bits(_local_pred_index_bits),
      local_xor(_local_xor),
      meta_pred_table(NULL),
      meta_pred_index_bits(_meta_pred_index_bits),
      meta_xor(_meta_xor),
      ras_size(_ras_size),
      conf_pred_enable(_conf_pred_enable),
      conf_pred_index_bits(_conf_pred_index_bits),
      conf_pred_ctr_bits(_conf_pred_ctr_bits),
      conf_pred_ctr_thresh(_conf_pred_ctr_thresh),
      conf_pred_xor(_conf_pred_xor),
      conf_pred_ctr_type(_conf_pred_ctr_type)
{
    int i;

    if (conf_pred_ctr_thresh == -1) {
	/* static confidence assignment: high confidence iff both
	 * predictors strong & agree.  conf_table is 1 for high confidence.
	 * index value is meta|local|global (2 bits each). */
	for (i = 0; i < 64; i++) {
	    int local = (i >> 2) & 3;
	    int global = i & 0x3;
	    conf_table[i] = (local == global) && TWOBIT_STRONG(local);
	}
    } else if (conf_pred_ctr_thresh == -2) {
	/* static confidence assignment: high confidence iff both
	 * predictors strong & agree OR meta strong, prediction indicated by
	 * meta is strong, other prediction agrees (maybe only weakly) */
	for (i = 0; i < 64; i++) {
	    int meta = (i >> 4) & 3;
	    int local = (i >> 2) & 3;
	    int global = i & 0x3;
	    conf_table[i] = ((local == global) &&TWOBIT_STRONG(local))
		|| (TWOBIT_STRONG(meta)
		    && ((meta >= 2) ? TWOBIT_STRONG(local) :
			TWOBIT_STRONG(global))
		    && TWOBIT_AGREE(local, global));

	}
    } else if (conf_pred_ctr_thresh >= 0) {
	/* dynamic counters determine confidence: initialize them */
	for (i = 0; i < 64; i++)
	    conf_table[i] = (1 << conf_pred_ctr_bits) - 1;
    } else
	panic("conf_pred_ctr_thresh is negative!\n");

    if (bp_class == BPredComb || bp_class == BPredGlobal) {
	/* allocate global predictor */
	unsigned pred_table_size = 1 << global_pred_index_bits;

	global_pred_table = new uint8_t[pred_table_size];

	/* initialize history regs (for repeatable results) */
	for (i = 0; i < SMT_MAX_THREADS; ++i)
	    global_hist_reg[i] = 0;

	/* initialize to weakly taken (not that it matters much) */
	for (i = 0; i < pred_table_size; ++i)
	    global_pred_table[i] = 2;

	// confidence predictor only works with COMB or GLOBAL predictor types
	if (conf_pred_enable) {
	    conf_pred_table = new uint8_t[1 << conf_pred_index_bits];

	    for (i = 0; i < (1 << conf_pred_index_bits); ++i)
		conf_pred_table[i] = 0;

	    if (conf_pred_ctr_bits > 8)
		fatal("bpred_create: confidence counter has max of 8 bits");

	    if (conf_pred_ctr_thresh >= (1 << conf_pred_ctr_bits))
		fatal("bpred_create: confidence threshold has max value of %d",
		      (1 << conf_pred_ctr_bits));
	} else
	  conf_pred_table = 0;
    } else {
	// no global predictor: config incompatible with confidence predictor
	if (conf_pred_enable)
	    fatal("confidence predictor only works with hybrid or global "
		  "branch predictor types");
    }

    if (bp_class == BPredComb || bp_class == BPredLocal) {
	/* allocate local predictor */
	unsigned pred_table_size = 1 << local_pred_index_bits;

	local_pred_table = new uint8_t[pred_table_size];

	if (!IS_POWER_OF_TWO(num_local_hist_regs))
	    fatal("number of local history regs must be a power of two");
	local_hist_regs = new unsigned int[num_local_hist_regs];

	/* initialize history registers */
	for (i = 0; i < num_local_hist_regs; ++i)
	    local_hist_regs[i] = 0;

	/* initialize to weakly taken (not that it matters much) */
	for (i = 0; i < pred_table_size; ++i)
	    local_pred_table[i] = 2;
    }
    if (bp_class == BPredComb) {
	/* allocate meta predictor */
	unsigned pred_table_size = 1 << meta_pred_index_bits;

	meta_pred_table = new uint8_t[pred_table_size];

	/* initialize to weakly favor global (not that it matters much) */
	for (i = 0; i < pred_table_size; ++i)
	    meta_pred_table[i] = 1;
    }

    /* allocate BTB */
    if (!btb_sets || !IS_POWER_OF_TWO(btb_sets))
	fatal("number of BTB sets must be non-zero and a power of two");
    if (!btb_assoc || !IS_POWER_OF_TWO(btb_assoc))
	fatal("BTB associativity must be non-zero and a power of two");

    btb.btb_data = new BTBEntry[btb_sets * btb_assoc];
    btb.sets = btb_sets;
    btb.assoc = btb_assoc;

    /* initialize BTB entries (for repeatable results) */
    for (i = 0; i < (btb.assoc * btb.sets); i++) {
	btb.btb_data[i].addr = btb.btb_data[i].target = 0;
	btb.btb_data[i].next = btb.btb_data[i].prev = 0;
    }

    /* if BTB is set-associative, initialize per-set LRU chains */
    if (btb.assoc > 1) {
	for (i = 0; i < (btb.assoc * btb.sets); i++) {
	    if (i % btb.assoc != btb.assoc - 1)
		btb.btb_data[i].next = &btb.btb_data[i + 1];
	    else
		btb.btb_data[i].next = NULL;

	    if (i % btb.assoc != btb.assoc - 1)
		btb.btb_data[i + 1].prev = &btb.btb_data[i];
	}
    }

    /* allocate return-address stack */
    if (!IS_POWER_OF_TWO(ras_size))
	fatal("Return-address-stack size must be zero or a power of two");

    if (ras_size) {
	for (i = 0; i < SMT_MAX_THREADS; i++) {
	    retAddrStack[i].stack = new Addr[ras_size];
	    // clear stack entries (for repeatable results)
	    for (int j = 0; j < ras_size; ++j)
		retAddrStack[i].stack[j] = 0;
	    retAddrStack[i].tos = ras_size - 1;
	}
    }
}

void
BranchPred::regStats( )
{
    using namespace Stats;

    lookups
	.init(cpu->number_of_threads)
	.name(name() + ".lookups")
	.desc("num BP lookups")
	.flags(total)
	;

    cond_predicted
	.init(cpu->number_of_threads)
	.name(name() + ".cond_predicted")
	.desc("num committed conditional branches")
	.flags(total)
	;

    cond_correct
	.init(cpu->number_of_threads)
	.name(name() + ".cond_correct")
	.desc("num correct dir predictions")
	.flags(total)
	;

    btb_lookups
	.init(cpu->number_of_threads)
	.name(name() + ".btb_lookups")
	.desc("Number of BTB lookups")
	.flags(total)
	;

    btb_hits
	.init(cpu->number_of_threads)
	.name(name() + ".btb_hits")
	.desc("Number of BTB hits")
	.flags(total)
	;

    used_btb
	.init(cpu->number_of_threads)
	.name(name() + ".used_btb")
	.desc("num committed branches using target from BTB")
	.flags(total)
	;

    btb_correct
	.init(cpu->number_of_threads)
	.name(name() + ".btb_correct")
	.desc("num correct BTB predictions")
	.flags(total)
	;

    used_ras
	.init(cpu->number_of_threads)
	.name(name() + ".used_ras")
	.desc("num returns predicted using RAS")
	.flags(total)
	;

    ras_correct
	.init(cpu->number_of_threads)
	.name(name() + ".ras_correct")
	.desc("num correct RAS predictions")
	.flags(total)
	;

    switch (bp_class) {
    case BPredComb:
	pred_state_table_size = 64;	/* bit patterns */
	break;

    case BPredGlobal:
    case BPredLocal:
	pred_state_table_size = 16;
	break;

    default:
	fatal("bad bpred class");
    }


    for (int i = 0; i < NUM_PRED_STATE_ENTRIES; i++)
	pred_state[i].init(pred_state_table_size);


    conf_chc.init(cpu->number_of_threads);
    conf_clc.init(cpu->number_of_threads);
    conf_ihc.init(cpu->number_of_threads);
    conf_ilc.init(cpu->number_of_threads);

    corr_conf_dist.init(/* base value */ 0,
			/* array size - 1*/
			(1 << conf_pred_ctr_bits) - 1,
			/* bucket size */ 1);

    incorr_conf_dist.init(/* base value */ 0,
			  /* array size - 1*/
			  (1 << conf_pred_ctr_bits) - 1,
			  /* bucket size */ 1);


    if (conf_pred_enable) {

	conf_chc
	    .name(name() + ".conf.cor_high")
	    .desc("num correct preds with high confidence")
	    .flags(total)
	    ;

	conf_clc
	    .name(name() + ".conf.cor_low")
	    .desc("num correct preds with low confidence")
	    .flags(total)
	    ;

	conf_ihc
	    .name(name() + ".conf.incor_high")
	    .desc("num incorrect preds with high confidence")
	    .flags(total)
	    ;

	conf_ilc
	    .name(name() + ".conf.incor_low")
	    .desc("num incorrect preds with low confidence")
	    .flags(total)
	    ;

	corr_conf_dist
	    .name(name() + ".conf.cor.dist")
	    .desc("Number of correct predictions for each confidence value")
	    .flags(pdf)
	    ;

	incorr_conf_dist
	    .name(name() + ".conf.incor.dist")
	    .desc("Number of incorrect predictions for each confidence value")
	    .flags(pdf)
	    ;
    }
}

void
BranchPred::regFormulas()
{
    using namespace Stats;

    lookup_rate
	.name(name() + ".lookup_rate")
	.desc("Rate of bpred lookups")
	.flags(total)
	;
    lookup_rate = lookups / cpu->numCycles;

    dir_accuracy
	.name(name() + ".dir_accuracy")
	.desc("fraction of predictions correct")
	.flags(total)
	;
    dir_accuracy = cond_correct / cond_predicted;

    btb_hit_rate
	.name(name() + ".btb_hit_rate")
	.desc("BTB hit ratio")
	.flags(total)
	;
    btb_hit_rate = btb_hits / btb_lookups;

    btb_accuracy
	.name(name() + ".btb_accuracy")
	.desc("fraction of BTB targets correct")
	.flags(total)
	;
    btb_accuracy = btb_correct / used_btb;

    ras_accuracy
	.name(name() + ".ras_accuracy")
	.desc("fraction of RAS targets correct")
	.flags(total)
	;
    ras_accuracy = ras_correct / used_ras;

    if (conf_pred_enable) {
	conf_sens
	    .name(name() + ".conf.sens")
	    .desc("Sens: \% correct preds that were HC")
	    .precision(4)
	    ;
	conf_sens = conf_chc / (conf_chc + conf_clc);

	conf_pvp
	    .name(name() + ".conf.pvp")
	    .desc("PVP: Prob of correct pred given HC")
	    .precision(4)
	    ;
	conf_pvp = conf_chc / (conf_chc + conf_ihc);

	conf_spec
	    .name(name() + ".conf.spec")
	    .desc("Spec: \% incorr preds that were LC")
	    .precision(4)
	    ;
	conf_spec = conf_ilc / (conf_ihc + conf_ilc);

	conf_pvn
	    .name(name() + ".conf.pvn")
	    .desc("PVN: Prob of incorrect pred given LC")
	    .precision(4)
	    ;
	conf_pvn = conf_ilc / (conf_clc + conf_ilc);
    }
}

void
decode_state(unsigned input, char *output)
{
    int i;
    unsigned work;

    output[5] = '\0';
    output[1] = output[3] = ' ';

    for (i = 0; i < 3; i++) {
	work = (input >> (2 * i)) & 0x03;

	output[2 * (2 - i)] = '0' + work;
    }
}


#if 0

void
bpred_print_pred_state(struct bpred_t *p)
{
    int i;
    Counter total_brs = 0;
    int pred_state;
    char s[10];

    if (!p)
	return;

    for (i = 0; i < SMT_MAX_THREADS; ++i) {
	total_brs += p->cond_predicted[i];
    }

    if (conf_pred_enable) {
	ccprintf(cerr, "%s.pred_state.begin\n", nameStr);
	ccprintf(cerr,"   state(M,L,G)      correct    incorrect        "
		 "total      acc    frac   %% H-C    acc   %% L-C    acc\n");

	for (pred_state = 0; pred_state < p->pred_state_table_size;
	     pred_state++) {
	    Counter low_conf = p->pred_state[PS_LOW_CONF][pred_state];
	    Counter high_conf = p->pred_state[PS_HIGH_CONF][pred_state];
	    Counter correct = p->pred_state[PS_CORRECT][pred_state];
	    Counter incorrect = p->pred_state[PS_INCORRECT][pred_state];
	    Counter hc_correct = p->pred_state[PS_HC_COR][pred_state];
	    Counter lc_incorrect = p->pred_state[PS_LC_INCOR][pred_state];
	    Counter total = correct + incorrect;

	    decode_state(pred_state, s);

	    ccprintf(cerr,
		     "    %02x  (%s) %12.0f %12.0f %12.0f   %6.2f  "
		     "%6.2f  %6.2f %6.2f  %6.2f %6.2f\n",
		    pred_state, s, (double) correct, (double) incorrect,
		    (double) total, 100.0 * (double) correct / (double) total,
		    100.0 * (double) total / (double) total_brs,
		    100.0 * (double) high_conf / (double) total,
		    100.0 * (double) hc_correct / (double) correct,
		    100.0 * (double) low_conf / (double) total,
		    100.0 * (double) lc_incorrect / (double) incorrect);
	}

	ccprintf(cerr, "%s.pred_state.end\n", nameStr);
    }
}


#endif

#if 0				/*  doesn't work any more  */
void
conf_pred_print_state(struct bpred_t *p)
{
    if (!p->conf_pred_index_bits)
	return;

    ccprintf(cerr, "conf.pred_state.begin\n");

    for (int i = 0; i < (1 << p->conf_pred_index_bits) / 16; i += 16) {
	ccprintf(cerr, "    %04X : ", i);
	for (int j = 0; j < 16; j++)
	    ccprintf(cerr, "%02X  ", p->conf_pred_table[i + j]);
	ccprintf(cerr, "\n");
    }

    ccprintf(cerr, "conf.pred_state.end\n");
}
#endif



/*
 * Calculate a 'pred_index_bits'-long predictor table index given branch
 * index bits (bindex) and 'hist_bits' bits of history (hist).  'xor'
 * specifies whether the bindex and hist bits should be xored or
 * concatenated.
 */

unsigned int
pred_index(unsigned int bindex, unsigned int hist, unsigned int hist_bits,
	   unsigned int pred_index_bits, bool _xor)
{
    /* bindex bits needed to fill out pred_index */
    unsigned needed_bindex_bits = pred_index_bits - hist_bits;

    if (needed_bindex_bits > 0) {
	/* move hist_bits up to make room for bindex bits; this also
	 * guarantees that any non-xored bindex bits come from the
	 * low-order part of bindex */
	hist <<= needed_bindex_bits;

	/* if we're concatenating (not xoring) bindex bits, clear the
	 * unused bits */
	if (!_xor)
	    bindex &= NBIT_MASK(needed_bindex_bits);
    } else {
	if (!_xor) {
	    /* don't need any bindex bits... */
	    bindex = 0;
	}
    }

    /* if !xor, the bindex & hist bits are disjoint, so XOR == OR */
    return ((hist ^ bindex) & NBIT_MASK(pred_index_bits));
}


/* probe a predictor for a next fetch address, the predictor is probed
   with branch address BADDR, the branch target is BTARGET (used for
   static predictors), and OP is the instruction opcode (used to simulate
   predecode bits; a pointer to the predictor state entry (or null for jumps)
   is returned in *DIR_UPDATE_PTR (used for updating predictor state),
   and the non-speculative top-of-stack is returned in stack_recover_idx
   (used for recovering ret-addr stack after mis-predict).  */
BranchPred::LookupResult
BranchPred::lookup(int thread,
		   Addr baddr,	/* branch address */
		   const StaticInstBasePtr &brInst, /* static instruction */
		   Addr *pred_target_ptr,
		   BPredUpdateRec * brstate, /* state pointer for update/recovery */
		   enum conf_pred * confidence)
{
    int i, index;
    bool pred_taken = false;
    unsigned int local_pred_ctr = 0, global_pred_ctr = 0, meta_dir_ctr = 0;

    /* if this is not a branch, return not-taken */
    if (!brInst->isControl())
	return Predict_Not_Taken;

    lookups[thread]++;

    /* if we got this far, we going to make a prediction... */
    brstate->used_predictor = true;

    /* we'll set these later if necessary */
    brstate->used_btb = 0;
    brstate->used_ras = 0;

#if BP_VERBOSE
    ccprintf(cerr, "BR: %#08X (cycle %n) ", baddr, curTick);
#endif

    /* if unconditional, predict taken, else do a direction prediction */
    if (brInst->isUncondCtrl()) {
	pred_taken = 1;

	/* Branch could be misspeculated if it's an indirect jump and we
	 * get the wrong target out of the BTB.  Need to snapshot state
	 * so we can undo potential (mis-)updates in bpred_recover() */
	brstate->global_hist = global_hist_reg[thread];
	brstate->ras_tos = retAddrStack[thread].tos;
	brstate->ras_value = retAddrStack[thread].stack[brstate->ras_tos];

#if BP_VERBOSE
	ccprintf(cerr, "UNCOND ");
#endif
    } else {
	/* branch bits used as index */
	unsigned int bindex = baddr >> BranchPredAddrShiftAmt;
	unsigned global_hist = global_hist_reg[thread];

	brstate->pred_state = 0;

#if BP_VERBOSE
	ccprintf(cerr, "COND   ");
#endif

	/*************************************/
	/* generate local prediction, if any */
	/*************************************/
	if (local_pred_table) {
	    /* local predictor */
	    unsigned local_bindex;
	    unsigned local_hist;
	    unsigned pidx;

	    local_bindex = bindex;

	    local_bindex &= (num_local_hist_regs - 1);

	    local_hist = local_hist_regs[local_bindex];
	    pidx = pred_index(bindex, local_hist, local_hist_bits,
			      local_pred_index_bits, local_xor);
	    local_pred_ctr = local_pred_table[pidx];

	    /*  Local predictor result...  */
	    pred_taken = (local_pred_ctr >= 2);

	    /* save index for state update at commit */
	    brstate->local_pidx = pidx;
	    /* for statistics */
	    brstate->pred_state = local_pred_ctr;

#if BP_VERBOSE
	    ccprintf(cerr, "LH=%#08X  IX=%#08X  CT=%1d ", local_hist, pidx,
		     local_pred_ctr);
#endif
	}

	/**************************************/
	/* generate global prediction, if any */
	/**************************************/
	if (global_pred_table) {
	    unsigned pidx =
		pred_index(bindex, global_hist, global_hist_bits,
			   global_pred_index_bits, global_xor);
	    global_pred_ctr = global_pred_table[pidx];

	    /*  Global prediction is...  */
	    pred_taken = (global_pred_ctr >= 2);

	    /* save index for state update at commit */
	    brstate->global_pidx = pidx;
	    /* for statistics */
	    brstate->pred_state <<= 2;	/* move local state over (if any) */
	    brstate->pred_state |= global_pred_ctr;

#if BP_VERBOSE
	    ccprintf(cerr, "GH=%#08X  IX=%#08X  CT=%1d ", global_hist,
		     pidx, global_pred_ctr);
#endif
	}

	/*************************************************************/
	/* if we're using a hybrid predictor, use it to choose local */
	/* vs. global prediction.  if not, we will use the value of  */
	/* pred_taken set by either the local or global predictor    */
	/* above (not both, since if we're not doing hybird only one */
	/* will exist)                                               */
	/*************************************************************/
	if (meta_pred_table) {
	    unsigned pidx =
		pred_index(bindex, global_hist, global_hist_bits,
			   meta_pred_index_bits, meta_xor);
	    meta_dir_ctr = meta_pred_table[pidx];

	    /* meta > 2 --> use local */
	    pred_taken =
		(((meta_dir_ctr >= 2) ? local_pred_ctr : global_pred_ctr) >=
		 2);

	    /* for statistics */
	    brstate->meta_pidx = pidx;
	    brstate->pred_state |= (meta_dir_ctr << 4);
#if BP_VERBOSE
	    ccprintf(cerr, "META=%1d ", meta_dir_ctr);
#endif
	}

	/*******************************************/
	/* speculatively update global history reg */
	/*******************************************/
	global_hist =
	    ((global_hist << 1) | pred_taken) &
	    NBIT_MASK(global_hist_bits);
	global_hist_reg[thread] = global_hist;

	/*  Just in case we got it wrong... generate the opposite update;
	 * this value will be placed in the history reg if we misspeculate */
	brstate->global_hist = global_hist ^ 0x01;

	/* save RAS TOS index and value for speculation recovery also */
	brstate->ras_tos = retAddrStack[thread].tos;
	brstate->ras_value = retAddrStack[thread].stack[brstate->ras_tos];


	/******************************/
	/*  Look up confidence value  */
	/******************************/
	brstate->conf_result = CONF_NULL;
	if (confidence) {
	    if (conf_pred_table) {
		unsigned pidx =
		    pred_index(bindex, global_hist, global_hist_bits,
			       conf_pred_index_bits, conf_pred_xor);

		brstate->conf_pidx = pidx;
		brstate->conf_value = conf_pred_table[pidx];

		if (conf_pred_table[pidx] >= conf_pred_ctr_thresh)
		    *confidence = CONF_HIGH;
		else
		    *confidence = CONF_LOW;
	    } else if (conf_pred_ctr_thresh < 0) {
		// static ctr confidence
		*confidence =
		    conf_table[brstate->pred_state] ? CONF_HIGH : CONF_LOW;

	    } else {
		// dynamic ctr confidence, small table
		if (conf_table[brstate->pred_state] > conf_pred_ctr_thresh)
		    *confidence = CONF_HIGH;
		else
		    *confidence = CONF_LOW;
	    }

	    brstate->conf_result = *confidence;
	}
    }


    /*
     * If branch is predicted not taken, there's no need to check the
     * BTB for a target.  Note that this assumes that a BTB lookup does
     * not affect the state of the BTB (e.g. the replacement policy).
     */
    if (!pred_taken) {
#if BP_VERBOSE
	ccprintf(cerr, "<NT>\n");
#endif
	return Predict_Not_Taken;
    }
#if BP_VERBOSE
    else {
	ccprintf(cerr, "<T> ");
    }
#endif


    /*
     * If we get here, branch is predicted taken (incl. unconditionals).
     * Try to get a target address from the RAS or the BTB.
     */

    if (ras_size) {
	ReturnAddrStack *ras = &retAddrStack[thread];

	if (brInst->isReturn()) {
	    /* if this is a return, pop return-address stack and go */
	    Addr target = ras->stack[ras->tos];
	    DPRINTF(BPredRAS, "RAS ret %#x idx %d tgt %#x\n",
		    baddr, ras->tos, target);
	    ras->tos--;
	    if (ras->tos < 0)
		ras->tos = ras_size - 1;
	    brstate->used_ras = 1;
#if BP_VERBOSE
	    ccprintf(cerr, "RAS= %#08X\n", target);
#endif
	    brstate->ras_tos = ras->tos;
	    brstate->ras_value = ras->stack[ras->tos];
	    *pred_target_ptr = target;
	    return Predict_Taken_With_Target;
	} else if (brInst->isCall()) {
	    /* if function call, push return address onto stack */
	    ras->tos++;
	    if (ras->tos == ras_size)
		ras->tos = 0;
	    ras->stack[ras->tos] = baddr + sizeof(MachInst);
	    brstate->ras_tos = ras->tos;
	    brstate->ras_value = ras->stack[ras->tos];
	    DPRINTF(BPredRAS, "RAS call %#x idx %d tgt %#x\n", baddr, ras->tos,
		    baddr + sizeof(MachInst));
#if BP_VERBOSE
	    ccprintf(cerr, "PUSH %#08X ", ras->stack[ras->tos]);
#endif
	}
    }
    /* predicted taken, not a return: do BTB lookup */
    index = ((baddr >> BranchPredAddrShiftAmt) & (btb.sets - 1)) * btb.assoc;

    /* Now we know the set; look for a PC match */

    btb_lookups[thread]++;

    for (i = index; i < (index + btb.assoc); i++) {
	if (btb.btb_data[i].addr == baddr) {
	    /* match (BTB hit): return target */
	    brstate->used_btb = 1;
	    btb_hits[thread]++;
#if BP_VERBOSE
	    ccprintf(cerr, "BTB=%#08X\n", btb.btb_data[i].target);
#endif
	    *pred_target_ptr = btb.btb_data[i].target;
	    return Predict_Taken_With_Target;
	}
    }

#if BP_VERBOSE
    ccprintf(cerr, "BTB missed\n");
#endif

    /* BTB miss: just return predicted direction (taken) */
    return Predict_Taken_No_Target;
}



/* Speculative execution can corrupt the ret-addr stack.  So for each
 * lookup we return the top-of-stack (TOS) at that point; a mispredicted
 * branch, as part of its recovery, restores the TOS using this value --
 * hopefully this uncorrupts the stack. */
void
BranchPred::recover(int thread,
		    Addr baddr,	/* branch address */
		    BPredUpdateRec *brstate)
{				/* pred state pointer */
#if BP_VERBOSE
    ccprintf(cerr, "RE: %#08X (cycle %n) ", baddr, curTick);
#endif

#if BP_VERBOSE
    ccprintf(cerr, "RAS_TOS=%02d GH=%#08X\n", brstate->ras_tos,
	     brstate->global_hist);
#endif

    if (DTRACE(BPredRAS) &&
	(retAddrStack[thread].tos != brstate->ras_tos ||
	 retAddrStack[thread].stack[brstate->ras_tos] != brstate->ras_value))
	DPRINTFN("RAS recover %#x %d %#x\n", baddr, brstate->ras_tos,
		 brstate->ras_value);

    /* if we didn't use the predictor on this branch (for
     * leading-thread predictions) don't update */
    if (!brstate->used_predictor)
	return;

    retAddrStack[thread].tos = brstate->ras_tos;
    retAddrStack[thread].stack[brstate->ras_tos] = brstate->ras_value;
    global_hist_reg[thread] = brstate->global_hist;
}

/*
 * Update 2-bit counter
 */
static void
update_ctr(uint8_t * ctrp, bool incr)
{
    int ctr = *ctrp;

    if (incr && ctr < 3) {
	++(*ctrp);
	return;
    }

    if (!incr && ctr > 0) {
	--(*ctrp);
	return;
    }
}


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
BranchPred::update( int thread,	/* thread ID */
		    Addr baddr,	/* branch address */
		    Addr btarget,	/* resolved branch target */
		    int taken,		/* non-zero if branch was taken */
		    int pred_taken,	/* non-zero if branch was pred taken */
		    int correct,	/* was earlier addr prediction ok? */
		    const StaticInstBasePtr &brInst, /* static instruction */
		    BPredUpdateRec *brstate)
{				/* pred state pointer */
    BTBEntry *pbtb = NULL;
    BTBEntry *lruhead = NULL, *lruitem = NULL;
    int index, i;
    unsigned pred_state_idx;
    /* For redundant lagging threads, we can optionally turn off
     * some or all of the updates.  The following flags control
     * this.  Assume we'll update everything unless we decide otherwise. */
    bool update_local_hist = true;
    bool update_counters = true;
    bool update_btb = true;

    /* if we didn't use the predictor on this branch (for
     * leading-thread predictions) don't update */
    if (!brstate->used_predictor)
	return;

    /* don't change bpred state for non-branch instructions */
    if (!brInst->isControl())
	return;

#if BP_VERBOSE
    ccprintf(cerr, "UP: %#08X (cycle %n) ", baddr, curTick);
#endif

    if (brInst->isCondCtrl()) {
	/* conditional branch: update predictor & record statistics */
	unsigned int bindex = baddr >> BranchPredAddrShiftAmt;

#if BP_VERBOSE
	ccprintf(cerr, "COND   ");
#endif

	cond_predicted[thread]++;
	if (taken == pred_taken) {
	    cond_correct[thread]++;
#if BP_VERBOSE
	    ccprintf(cerr, "C ");
	} else {
	    ccprintf(cerr, "I ");
#endif
	}

	/*
	 *  UPDATE LOCAL PREDICTOR
	 *
	 */
	if (local_pred_table) {
	    unsigned local_bindex = bindex;

	    local_bindex &= (num_local_hist_regs - 1);

	    /* update counter used for this branch */
	    if (update_counters)
		update_ctr(&local_pred_table[brstate->local_pidx], taken);

	    /* update history shift register */
	    if (update_local_hist)
		local_hist_regs[local_bindex] =
		    (((local_hist_regs[local_bindex] << 1) | taken)
		     & NBIT_MASK(local_hist_bits));
#if BP_VERBOSE
	    ccprintf(cerr, "LH=%#08X IDX=%#08X CT=%1d ",
		     local_hist_regs[local_bindex], brstate->local_pidx,
		     local_pred_table[brstate->local_pidx]);
#endif
	}
	/*
	 *  UPDATE GLOBAL PREDICTOR
	 *
	 */
	if (global_pred_table) {
	    if (update_counters)
		update_ctr(&global_pred_table[brstate->global_pidx],
			   taken);

#if BP_VERBOSE
	    ccprintf(cerr, "GIDX=%#08X CT=%1d ", brstate->global_pidx,
		    global_pred_table[brstate->global_pidx]);
#endif
	    /* shift reg was already updated speculatively in bpred_lookup() */
	}

	/*
	 *  UPDATE META PREDICTOR
	 *
	 */
	if (meta_pred_table) {
	    bool local_pred = ((brstate->pred_state & 0x8) == 0x8);
	    bool global_pred = ((brstate->pred_state & 0x2) == 0x2);

	    /* update meta-predictor only if global & local disagreed */
	    if (local_pred != global_pred && update_counters) {
		/* increment if local predictor was correct, decrement if
		 * global was correct */
		update_ctr(&meta_pred_table[brstate->meta_pidx],
			   local_pred == taken);
#if BP_VERBOSE
		ccprintf(cerr, "MIDX=%#08X CT=%1d ", brstate->meta_pidx,
			 meta_pred_table[brstate->meta_pidx]);
	    } else {
		ccprintf(cerr, "No_Meta_Update         ");
#endif
	    }
	}

	/*
	 *
	 *  Update Confidence stuff
	 *
	 */
	if (conf_pred_enable) {
	    pred_state_idx = brstate->pred_state;

	    if (conf_pred_table) {
		uint8_t *conf_pred_ctr =
		    &(conf_pred_table[brstate->conf_pidx]);

		//  BPred was correct: Increment the counter if we
		//                     haven't maxed out
		//  BPred was wrong:   Reset the counter
		if (taken == pred_taken) {
		    corr_conf_dist.sample(brstate->conf_value);

		    // correct prediction
		    if (brstate->conf_result == CONF_HIGH)
			conf_chc[thread]++;	// high confidence
		    else
			conf_clc[thread]++;	// low confidence

		    if (*conf_pred_ctr < ((1 << conf_pred_ctr_bits) - 1))
			(*conf_pred_ctr)++;
		} else {
		    incorr_conf_dist.sample(brstate->conf_value);

		    // incorrect prediction
		    if (brstate->conf_result == CONF_HIGH)
			conf_ihc[thread]++;	// high confidence
		    else
			conf_ilc[thread]++;	// low confidence

		    switch (conf_pred_ctr_type) {
		    case CNT_RESET:
			*conf_pred_ctr = 0;
			break;
		    case CNT_SAT:
			if (*conf_pred_ctr > 0)
			    (*conf_pred_ctr)--;
			break;
		    }
		}
	    } else {
		//  Must be using small predictor...
		if (taken == pred_taken) {
		    // correct prediction
		    if (brstate->conf_result == CONF_HIGH)
			conf_chc[thread]++;	// high confidence
		    else
			conf_clc[thread]++;	// low confidence
		} else {
		    // incorrect prediction
		    if (brstate->conf_result == CONF_HIGH)
			conf_ihc[thread]++;	// high confidence
		    else
			conf_ilc[thread]++;	// low confidence
		}

		// first six bits are prediction state, last bit is
		// correct/incorrect
		if (conf_pred_ctr_thresh >= 0) {
		    // using dynamic assignment
		    if (taken == pred_taken) {
			// correct prediction
			if (conf_table[pred_state_idx] <
			    ((1 << conf_pred_ctr_bits) - 1))
			    conf_table[pred_state_idx]++;
		    } else {
			// incorrect prediction
			if (conf_pred_ctr_type == CNT_SAT)
			    if (conf_table[pred_state_idx] > 0)
				conf_table[pred_state_idx]--;
			else
			    conf_table[pred_state_idx] = 0;
		    }
		}
	    }

	    /*
	     *  Update the big predictor table for all cases
	     */
	    if (taken == pred_taken) {
		//  correct prediction
		pred_state[PS_CORRECT][pred_state_idx]++;
		if (brstate->conf_result == CONF_HIGH)
		    pred_state[PS_HC_COR][pred_state_idx]++;
		if (brstate->conf_result == CONF_LOW)
		    pred_state[PS_LC_COR][pred_state_idx]++;
	    } else {
		//  incorrect prediction
		pred_state[PS_INCORRECT][pred_state_idx]++;
		if (brstate->conf_result == CONF_HIGH)
		    pred_state[PS_HC_INCOR][pred_state_idx]++;
		if (brstate->conf_result == CONF_LOW)
		    pred_state[PS_LC_INCOR][pred_state_idx]++;
	    }

	    if (brstate->conf_result == CONF_HIGH)
		pred_state[PS_HIGH_CONF][pred_state_idx]++;
	    if (brstate->conf_result == CONF_LOW)
		pred_state[PS_LOW_CONF][pred_state_idx]++;
	}


#if BP_VERBOSE
	ccprintf(cerr, "\n");
    } else {
	ccprintf(cerr, "UNCOND \n");
#endif
    }

    if (taken) {
	if (brstate->used_ras) {
	    /* used RAS... */
	    used_ras[thread]++;

	    DPRINTF(BPredRAS, "RAS update br %#x tgt %#x %s\n", baddr, btarget,
		     correct ? "correct" : "incorrect");

	    if (correct)
		ras_correct[thread]++;

	    /* no need to update BTB */
	    return;
	}

	if (brstate->used_btb) {
	    used_btb[thread]++;
	    if (correct)
		btb_correct[thread]++;
	}

	/* update BTB */
	if (update_btb) {
	    index = (baddr >> BranchPredAddrShiftAmt) & (btb.sets - 1);

	    if (btb.assoc > 1) {
		index *= btb.assoc;

		/* Now we know the set; look for a PC match; also identify
		 * MRU and LRU items */
		for (i = index; i < (index + btb.assoc); i++) {
		    if (btb.btb_data[i].addr == baddr) {
			/* match */
			assert(!pbtb);
			pbtb = &btb.btb_data[i];
		    }
		    dassert(btb.btb_data[i].prev
			    != btb.btb_data[i].next);
		    if (btb.btb_data[i].prev == NULL) {
			/* this is the head of the lru list, ie
			 * current MRU item */
			dassert(lruhead == NULL);
			lruhead = &btb.btb_data[i];
		    }
		    if (btb.btb_data[i].next == NULL) {
			/* this is the tail of the lru list, ie the LRU item */
			dassert(lruitem == NULL);
			lruitem = &btb.btb_data[i];
		    }
		}
		dassert(lruhead && lruitem);

		if (!pbtb)
		    /* missed in BTB; choose the LRU item in this set
		     * as the victim */
		    pbtb = lruitem;
		/* else hit, and pbtb points to matching BTB entry */

		/* Update LRU state: selected item, whether selected
		 * because it matched or because it was LRU and
		 * selected as a victim, becomes MRU */
		if (pbtb != lruhead) {
		    /* this splices out the matched entry... */
		    if (pbtb->prev)
			pbtb->prev->next = pbtb->next;
		    if (pbtb->next)
			pbtb->next->prev = pbtb->prev;
		    /* ...and this puts the matched entry at the head */
		    pbtb->next = lruhead;
		    pbtb->prev = NULL;
		    lruhead->prev = pbtb;
		    dassert(pbtb->prev || pbtb->next);
		    dassert(pbtb->prev != pbtb->next);
		}
		/* else pbtb is already MRU item; do nothing */
	    } else {
		/* direct-mapped BTB */
		pbtb = &btb.btb_data[index];
	    }

	    if (pbtb) {
		/* update current information */
		if (pbtb->addr == baddr) {
		    if (!correct)
			pbtb->target = btarget;
		} else {
		    /* enter a new branch in the table */
		    pbtb->addr = baddr;
		    pbtb->target = btarget;
		}
	    }
	}
    }
}


/**
 * Pop top element off of return address stack.  Used to fix up RAS
 * after a SkipFuncEvent (see pc_event.cc).
 */
void
BranchPred::popRAS(int thread)
{
    ReturnAddrStack *ras = &retAddrStack[thread];
#if TRACING_ON
    int old_tos = ras->tos;
#endif

    ras->tos--;
    if (ras->tos < 0)
	ras->tos = ras_size - 1;

    DPRINTF(BPredRAS, "RAS pop %d -> %d\n", old_tos, ras->tos);
}


BEGIN_DECLARE_SIM_OBJECT_PARAMS(BranchPred)

    SimpleEnumParam<BPredClass> pred_class;
    Param<unsigned> global_hist_bits;
    Param<unsigned> global_index_bits;
    Param<bool> global_xor;
    Param<unsigned> local_hist_regs;
    Param<unsigned> local_hist_bits;
    Param<unsigned> local_index_bits;
    Param<bool> local_xor;
    Param<unsigned> choice_index_bits;
    Param<bool> choice_xor;
    Param<unsigned> btb_size;
    Param<unsigned> btb_assoc;
    Param<unsigned> ras_size;
    Param<bool> conf_pred_enable;
    Param<unsigned> conf_pred_index_bits;
    Param<unsigned> conf_pred_ctr_bits;
    Param<int> conf_pred_ctr_thresh;
    Param<bool> conf_pred_xor;
    SimpleEnumParam<ConfCounterType> conf_pred_ctr_type;

END_DECLARE_SIM_OBJECT_PARAMS(BranchPred)

// parameter strings for enum BPredClass
const char *bpred_class_strings[] =
{
    "hybrid", "global", "local"
};

// parameter strings for enum ConfCounterType
const char *conf_counter_type_strings[] =
{
    "resetting", "saturating"
};

BEGIN_INIT_SIM_OBJECT_PARAMS(BranchPred)

    INIT_ENUM_PARAM(pred_class, "predictor class",
		    bpred_class_strings),
    INIT_PARAM(global_hist_bits, "global predictor history reg bits"),
    INIT_PARAM(global_index_bits, "global predictor index bits"),
    INIT_PARAM(global_xor, "XOR global hist w/PC (false: concatenate)"),
    INIT_PARAM(local_hist_regs, "num. local predictor history regs"),
    INIT_PARAM(local_hist_bits, "local predictor history reg bits"),
    INIT_PARAM(local_index_bits, "local predictor index bits"),
    INIT_PARAM(local_xor, "XOR local hist w/PC (false: concatenate)"),
    INIT_PARAM(choice_index_bits, "choice predictor index bits"),
    INIT_PARAM(choice_xor, "XOR choice hist w/PC (false: concatenate)"),
    INIT_PARAM(btb_size, "number of entries in BTB"),
    INIT_PARAM(btb_assoc, "BTB associativity"),
    INIT_PARAM(ras_size, "return address stack size"),
    INIT_PARAM_DFLT(conf_pred_enable, "enable confidence predictor", false),
    INIT_PARAM(conf_pred_index_bits, "confidence predictor index bits"),
    INIT_PARAM(conf_pred_ctr_bits, "confidence predictor counter bits"),
    INIT_PARAM(conf_pred_ctr_thresh, "confidence predictor threshold"),
    INIT_PARAM(conf_pred_xor, "XOR confidence predictor bits"),
    INIT_ENUM_PARAM(conf_pred_ctr_type, "confidence predictor type",
		    conf_counter_type_strings)

END_INIT_SIM_OBJECT_PARAMS(BranchPred)


CREATE_SIM_OBJECT(BranchPred)
{
    // These flags are used to avoid evaluating parameters that have
    // no meaning in a given context (like global predictor parameters
    // when the selected predictor type is local).  It would be neater
    // to have different subclasses for the different predictor types,
    // but (1) it would be too big a rewrite of this old code, (2) we
    // never really use anything but the hybrid predictor anyway, and
    // (3) the performance hit from virtualizing all the BPred calls
    // might be noticeable.
    bool need_local  = pred_class == BPredComb || pred_class == BPredLocal;
    bool need_global = pred_class == BPredComb || pred_class == BPredGlobal;
    bool need_meta = pred_class == BPredComb;
    bool need_conf = conf_pred_enable;

    return new BranchPred(getInstanceName(),
			  pred_class,
			  need_global ? global_hist_bits : 0,
			  need_global ? global_index_bits : 0,
			  need_global ? global_xor : false,
			  need_local ? local_hist_regs : 0,
			  need_local ? local_hist_bits : 0,
			  need_local ? local_index_bits : 0,
			  need_local ? local_xor : false,
			  need_meta ? choice_index_bits : 0,
			  need_meta ? choice_xor : false,
			  btb_size / btb_assoc, btb_assoc,
			  ras_size,
			  conf_pred_enable,
			  need_conf ? conf_pred_index_bits : 0,
			  need_conf ? conf_pred_ctr_bits : 0,
			  need_conf ? conf_pred_ctr_thresh : 0,
			  need_conf ? conf_pred_xor : false,
			  need_conf ? conf_pred_ctr_type : CNT_RESET);
}

REGISTER_SIM_OBJECT("BranchPred", BranchPred)
