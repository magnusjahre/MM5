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

#ifndef __CPU_O3_CPU_STORE_SET_HH__
#define __CPU_O3_CPU_STORE_SET_HH__

#include <vector>

#include "arch/alpha/isa_traits.hh"
#include "cpu/inst_seq.hh"

class StoreSet
{
  public:
    typedef unsigned SSID;

  public:
    StoreSet(int SSIT_size, int LFST_size);

    void violation(Addr store_PC, Addr load_PC);

    void insertLoad(Addr load_PC, InstSeqNum load_seq_num);

    void insertStore(Addr store_PC, InstSeqNum store_seq_num);

    InstSeqNum checkInst(Addr PC);

    void issued(Addr issued_PC, InstSeqNum issued_seq_num, bool is_store);

    void squash(InstSeqNum squashed_num);

    void clear();

  private:
    inline int calcIndex(Addr PC)
    { return (PC >> offset_bits) & index_mask; }

    inline SSID calcSSID(Addr PC)
    { return ((PC ^ (PC >> 10)) % LFST_size); }

    SSID *SSIT;

    std::vector<bool> validSSIT;

    InstSeqNum *LFST;

    std::vector<bool> validLFST;

    int *SSCounters;

    int SSIT_size;
    
    int LFST_size;

    int index_mask;

    // HACK: Hardcoded for now.
    int offset_bits;
};

#endif // __CPU_O3_CPU_STORE_SET_HH__
