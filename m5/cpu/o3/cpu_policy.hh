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

#ifndef __CPU_O3_CPU_CPU_POLICY_HH__
#define __CPU_O3_CPU_CPU_POLICY_HH__

#include "cpu/o3/bpred_unit.hh"
#include "cpu/o3/free_list.hh"
#include "cpu/o3/inst_queue.hh"
#include "cpu/o3/ldstq.hh"
#include "cpu/o3/mem_dep_unit.hh"
#include "cpu/o3/regfile.hh"
#include "cpu/o3/rename_map.hh"
#include "cpu/o3/rob.hh"
#include "cpu/o3/store_set.hh"

#include "cpu/o3/commit.hh"
#include "cpu/o3/decode.hh"
#include "cpu/o3/fetch.hh"
#include "cpu/o3/iew.hh"
#include "cpu/o3/rename.hh"

#include "cpu/o3/comm.hh"

template<class Impl>
struct SimpleCPUPolicy
{
    typedef TwobitBPredUnit<Impl> BPredUnit;
    typedef PhysRegFile<Impl> RegFile;
    typedef SimpleFreeList FreeList;
    typedef SimpleRenameMap RenameMap;
    typedef ROB<Impl> ROB_TYPE;
    typedef InstructionQueue<Impl> IQ;
    typedef MemDepUnit<StoreSet, Impl> MemDepUnit_TYPE;
    typedef LDSTQ<Impl> LDSTQ_TYPE;
    
    typedef SimpleFetch<Impl> Fetch;
    typedef SimpleDecode<Impl> Decode;
    typedef SimpleRename<Impl> Rename;
    typedef SimpleIEW<Impl> IEW;
    typedef SimpleCommit<Impl> Commit;

    /** The struct for communication between fetch and decode. */
    typedef SimpleFetchSimpleDecode<Impl> FetchStruct;

    /** The struct for communication between decode and rename. */
    typedef SimpleDecodeSimpleRename<Impl> DecodeStruct;

    /** The struct for communication between rename and IEW. */
    typedef SimpleRenameSimpleIEW<Impl> RenameStruct;

    /** The struct for communication between IEW and commit. */
    typedef SimpleIEWSimpleCommit<Impl> IEWStruct;

    /** The struct for communication within the IEW stage. */
    typedef IssueStruct<Impl> IssueStruct_TYPE;

    /** The struct for all backwards communication. */
    typedef TimeBufStruct TimeStruct;

};

#endif //__CPU_O3_CPU_CPU_POLICY_HH__
