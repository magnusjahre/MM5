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

/*
 * Figuring out why we didn't fetch the full width of the machine.
 */

#ifndef __ENCUMBERED_CPU_FULL_FLOSS_REASONS_HH__
#define __ENCUMBERED_CPU_FULL_FLOSS_REASONS_HH__

#include <stdio.h>

#include "cpu/smt.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/op_class.hh"
#include "mem/mem_cmd.hh"

#define DEBUG_CAUSES 0

//
//  Macro to simplify setting Fetch-Loss causes...
//  (assumes that the NOT_SET cause is enum'd to -1)
//
#define SET_FIRST_FLOSS_CAUSE(flag,reason)    \
{                                             \
    if ((flag) == -1)                         \
	(flag) = (reason);                    \
}


//
//  Reasons why commit may terminate
//
enum CommitEndCause {
    COMMIT_NO_INSN,	        /* nothing to commit */
    COMMIT_BW,			/* commit bandwidth limit */
    COMMIT_STOREBUF,		/* store buffer full */
    COMMIT_MEMBAR,		/* memory barrier */

    //  meta-cause
    COMMIT_FU,			/* FU unavailable or not finished */
    COMMIT_DMISS,		/* data cache miss */

    NUM_COMMIT_END_CAUSES,

    COMMIT_CAUSE_NOT_SET = -1
};


//
//  Reasons why issue may terminate
//
//  NOTE:  For issue, we look at the reason that the FIRST instrction that
//         was examined did not issue. We ignore later instrctions.
//
enum IssueEndCause {
    ISSUE_NO_INSN,
    ISSUE_IQ,                   // Unable to issue due to IQ (IQ not empty) 
    ISSUE_BW,			// issue bandwidth limit      
    ISSUE_AGE,		        // instruction ready buy not old enough 
    ISSUE_INORDER,              // In-order issue - next inst not in seq. Like ISSUE_DEPS I suppose

    // meta-causes
    ISSUE_DEPS,                 // instruction has dependencies 
    ISSUE_FU,			// inadequate FU's available  
    ISSUE_MEM_BLOCKED,	        // Memory system busy (generic)

    NUM_ISSUE_END_CAUSES,

    ISSUE_CAUSE_NOT_SET = -1
};


//
//  Reasons why dispatch may terminate
//
//  NOTE:  For dispatch we look at the reason that the LAST instruction that
//         was examined did not dispatch. We ignore all others.
//
enum DispatchEndCause {
    FLOSS_DIS_IREG_FULL,
    FLOSS_DIS_FPREG_FULL,
    FLOSS_DIS_NO_INSN,
    FLOSS_DIS_ROB_CAP,
    FLOSS_DIS_IQ_CAP,
    FLOSS_DIS_BW,
    FLOSS_DIS_POLICY,
    FLOSS_DIS_SERIALIZING,
    FLOSS_DIS_BROKEN,

    // meta-causes
    FLOSS_DIS_IQ_FULL,
    FLOSS_DIS_LSQ_FULL,
    FLOSS_DIS_ROB_FULL,

    NUM_DIS_END_CAUSES,

    FLOSS_DIS_CAUSE_NOT_SET = -1
};


enum FetchEndCause {
    FLOSS_FETCH_NONE,
    FLOSS_FETCH_BANDWIDTH,
    FLOSS_FETCH_BRANCH_LIMIT,
    FLOSS_FETCH_INVALID_PC,
    FLOSS_FETCH_BTB_MISS,
    FLOSS_FETCH_BRANCH_RECOVERY,
    FLOSS_FETCH_FAULT_FLUSH,
    FLOSS_FETCH_SYNC,
    FLOSS_FETCH_LOW_CONFIDENCE,
    FLOSS_FETCH_POLICY,
    FLOSS_FETCH_UNKNOWN,
    FLOSS_FETCH_ZERO_PRIORITY,

    // meta-causes
    FLOSS_FETCH_IMISS,
    FLOSS_FETCH_QFULL,

    NUM_FETCH_END_CAUSES,

    FLOSS_FETCH_CAUSE_NOT_SET = -1
};


//==========================================================================
//
//   These are the variables that feed the "end-cause" information to
//   this module from each of the pipe stages.
//
//==========================================================================

struct FlossState
{
    //
    //  Commit Stage
    //
    enum CommitEndCause commit_end_cause[SMT_MAX_THREADS];
    OpClass             commit_fu[SMT_MAX_THREADS][TheISA::MaxInstSrcRegs];
    MemAccessResult     commit_mem_result[SMT_MAX_THREADS];
    int                 commit_end_thread;

    //
    //  Issue Stage
    //
    enum IssueEndCause issue_end_cause[SMT_MAX_THREADS];
    OpClass            issue_fu[SMT_MAX_THREADS][TheISA::MaxInstSrcRegs];
    MemAccessResult    issue_mem_result[SMT_MAX_THREADS];
    int                issue_end_thread;

    //
    //  Dispatch Stage
    //
    enum DispatchEndCause dispatch_end_cause;

    //
    //  Fetch Stage
    //
    enum FetchEndCause  fetch_end_cause[SMT_MAX_THREADS];
    MemAccessResult     fetch_mem_result[SMT_MAX_THREADS];

    void clear();
    void dump(unsigned threads);
};

enum FlossType {
    FLOSS_IQ_FU,
    FLOSS_IQ_DEPS,
    FLOSS_LSQ_FU,
    FLOSS_LSQ_DEPS,
    FLOSS_ROB
};

//==========================================================================

void record_floss_reasons(FlossState *state, int thread_fetched[]);

void floss_init();
void dump_floss_reasons(FILE * stream);

extern void floss_clear(FlossState *);

#endif // __FLOSS_REASONS_HH__
