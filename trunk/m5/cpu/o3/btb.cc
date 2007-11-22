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

#include "base/intmath.hh"
#include "base/trace.hh"
#include "cpu/o3/btb.hh"

DefaultBTB::DefaultBTB(unsigned _numEntries,
                       unsigned _tagBits,
                       unsigned _instShiftAmt)
    : numEntries(_numEntries),
      tagBits(_tagBits),
      instShiftAmt(_instShiftAmt)
{
    // @todo Check to make sure num_entries is valid (a power of 2)

    DPRINTF(Fetch, "BTB: Creating BTB object.\n");

    btb = new BTBEntry[numEntries];
    
    for (int i = 0; i < numEntries; ++i)
    {
        btb[i].valid = false;
    }

    idxMask = numEntries - 1;

    tagMask = (1 << tagBits) - 1;

    tagShiftAmt = instShiftAmt + FloorLog2(numEntries);
}

inline
unsigned
DefaultBTB::getIndex(const Addr &inst_PC)
{
    // Need to shift PC over by the word offset.
    return (inst_PC >> instShiftAmt) & idxMask;
}

inline
Addr
DefaultBTB::getTag(const Addr &inst_PC)
{
    return (inst_PC >> tagShiftAmt) & tagMask;
}

bool
DefaultBTB::valid(const Addr &inst_PC)
{
    unsigned btb_idx = getIndex(inst_PC);
    
    Addr inst_tag = getTag(inst_PC);

    assert(btb_idx < numEntries);

    if (btb[btb_idx].valid && inst_tag == btb[btb_idx].tag) {
        return true;
    } else {
        return false;
    }
}

// @todo Create some sort of return struct that has both whether or not the
// address is valid, and also the address.  For now will just use addr = 0 to
// represent invalid entry.
Addr
DefaultBTB::lookup(const Addr &inst_PC)
{
    unsigned btb_idx = getIndex(inst_PC);

    Addr inst_tag = getTag(inst_PC);

    assert(btb_idx < numEntries);
 
    if (btb[btb_idx].valid && inst_tag == btb[btb_idx].tag) {
        return btb[btb_idx].target;
    } else {
        return 0;
    }
}

void
DefaultBTB::update(const Addr &inst_PC, const Addr &target)
{
    unsigned btb_idx = getIndex(inst_PC);

    assert(btb_idx < numEntries);
    
    btb[btb_idx].valid = true;
    btb[btb_idx].target = target;
    btb[btb_idx].tag = getTag(inst_PC);
}
