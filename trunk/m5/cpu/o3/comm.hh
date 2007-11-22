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

#ifndef __CPU_O3_CPU_COMM_HH__
#define __CPU_O3_CPU_COMM_HH__

#include <vector>

#include "arch/alpha/isa_traits.hh"
#include "cpu/inst_seq.hh"
#include "sim/host.hh"

// Find better place to put this typedef.
// The impl might be the best place for this.
typedef short int PhysRegIndex;

template<class Impl>
struct SimpleFetchSimpleDecode {
    typedef typename Impl::DynInstPtr DynInstPtr;

    int size;

    DynInstPtr insts[Impl::MaxWidth];
};

template<class Impl>
struct SimpleDecodeSimpleRename {
    typedef typename Impl::DynInstPtr DynInstPtr;

    int size;

    DynInstPtr insts[Impl::MaxWidth];
};

template<class Impl>
struct SimpleRenameSimpleIEW {
    typedef typename Impl::DynInstPtr DynInstPtr;

    int size;

    DynInstPtr insts[Impl::MaxWidth];
};

template<class Impl>
struct SimpleIEWSimpleCommit {
    typedef typename Impl::DynInstPtr DynInstPtr;

    int size;

    DynInstPtr insts[Impl::MaxWidth];

    bool squash;
    bool branchMispredict;
    bool branchTaken;
    uint64_t mispredPC;
    uint64_t nextPC;
    InstSeqNum squashedSeqNum;
};

template<class Impl>
struct IssueStruct {
    typedef typename Impl::DynInstPtr DynInstPtr;

    int size;

    DynInstPtr insts[Impl::MaxWidth];
};

struct TimeBufStruct {
    struct decodeComm {
        bool squash;
        bool stall;
        bool predIncorrect;
        uint64_t branchAddr;

        InstSeqNum doneSeqNum;

        // Might want to package this kind of branch stuff into a single
        // struct as it is used pretty frequently.
        bool branchMispredict;
        bool branchTaken;
        uint64_t mispredPC;
        uint64_t nextPC;
    };

    decodeComm decodeInfo;

    // Rename can't actually tell anything to squash or send a new PC back
    // because it doesn't do anything along those lines.  But maybe leave
    // these fields in here to keep the stages mostly orthagonal.
    struct renameComm {
        bool squash;
        bool stall;
        
        uint64_t nextPC;
    };

    renameComm renameInfo;

    struct iewComm { 
        bool stall;

        // Also eventually include skid buffer space.
        unsigned freeIQEntries;
    };

    iewComm iewInfo;

    struct commitComm {
        bool squash;
        bool stall;
        unsigned freeROBEntries;
        
        bool branchMispredict;
        bool branchTaken;
        uint64_t mispredPC;
        uint64_t nextPC;

        bool robSquashing;

        // Represents the instruction that has either been retired or
        // squashed.  Similar to having a single bus that broadcasts the
        // retired or squashed sequence number.
        InstSeqNum doneSeqNum;

        // Extra bit of information so that the LDSTQ only updates when it
        // needs to.
        bool commitIsLoad;

        // Communication specifically to the IQ to tell the IQ that it can
        // schedule a non-speculative instruction.
        InstSeqNum nonSpecSeqNum;
    };

    commitComm commitInfo;
};

#endif //__CPU_O3_CPU_COMM_HH__
