/*
 * Copyright (c) 2004, 2005
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

#ifndef __CPU_O3_CPU_ALPHA_SIMPLE_PARAMS_HH__
#define __CPU_O3_CPU_ALPHA_SIMPLE_PARAMS_HH__

#include "cpu/o3/cpu.hh"

//Forward declarations
class System;
class AlphaITB;
class AlphaDTB;
class FunctionalMemory;
class Process;
class MemInterface;

/**
 * This file defines the parameters that will be used for the AlphaFullCPU.
 * This must be defined externally so that the Impl can have a params class
 * defined that it can pass to all of the individual stages.
 */

class AlphaSimpleParams : public BaseFullCPU::Params
{
  public:

#if FULL_SYSTEM
    AlphaITB *itb; AlphaDTB *dtb;
#else
    std::vector<Process *> workload;
    Process *process;
#endif // FULL_SYSTEM

    FunctionalMemory *mem;
    
    //
    // Caches
    //
    MemInterface *icacheInterface;
    MemInterface *dcacheInterface;
    
    //
    // Fetch
    //
    unsigned decodeToFetchDelay;
    unsigned renameToFetchDelay;
    unsigned iewToFetchDelay;
    unsigned commitToFetchDelay;
    unsigned fetchWidth;
    
    //
    // Decode
    //
    unsigned renameToDecodeDelay;
    unsigned iewToDecodeDelay;
    unsigned commitToDecodeDelay;
    unsigned fetchToDecodeDelay;
    unsigned decodeWidth;
    
    //
    // Rename
    //
    unsigned iewToRenameDelay;
    unsigned commitToRenameDelay;
    unsigned decodeToRenameDelay;
    unsigned renameWidth;
    
    //
    // IEW
    //
    unsigned commitToIEWDelay;
    unsigned renameToIEWDelay;
    unsigned issueToExecuteDelay;
    unsigned issueWidth;
    unsigned executeWidth;
    unsigned executeIntWidth;
    unsigned executeFloatWidth;
    unsigned executeBranchWidth;
    unsigned executeMemoryWidth;
    
    //
    // Commit
    //
    unsigned iewToCommitDelay;
    unsigned renameToROBDelay;
    unsigned commitWidth;
    unsigned squashWidth;
      
    //
    // Branch predictor (BP & BTB)
    //
/*
    unsigned localPredictorSize;
    unsigned localPredictorCtrBits;
*/

    unsigned local_predictor_size;
    unsigned local_ctr_bits;
    unsigned local_history_table_size;
    unsigned local_history_bits;
    unsigned global_predictor_size;
    unsigned global_ctr_bits;
    unsigned global_history_bits;
    unsigned choice_predictor_size;
    unsigned choice_ctr_bits;

    unsigned BTBEntries;
    unsigned BTBTagSize;

    unsigned RASSize;

    //
    // Load store queue
    //
    unsigned LQEntries;
    unsigned SQEntries;

    //
    // Memory dependence
    //
    unsigned SSITSize;
    unsigned LFSTSize;

    //
    // Miscellaneous
    //
    unsigned numPhysIntRegs;
    unsigned numPhysFloatRegs;
    unsigned numIQEntries;
    unsigned numROBEntries;

    // Probably can get this from somewhere.
    unsigned instShiftAmt;

    bool defReg;
};

#endif // __CPU_O3_CPU_ALPHA_PARAMS_HH__
