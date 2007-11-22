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

#ifndef __CPU_O3_CPU_BTB_HH__
#define __CPU_O3_CPU_BTB_HH__

// For Addr type.
#include "arch/alpha/isa_traits.hh"

class DefaultBTB
{
  private:
    struct BTBEntry
    {
        BTBEntry()
            : tag(0), target(0), valid(false)
        {
        }

        Addr tag;
        Addr target;
        bool valid;
    };
    
  public:
    DefaultBTB(unsigned numEntries, unsigned tagBits, 
               unsigned instShiftAmt);

    Addr lookup(const Addr &inst_PC);

    bool valid(const Addr &inst_PC);

    void update(const Addr &inst_PC, const Addr &target_PC);

  private:
    inline unsigned getIndex(const Addr &inst_PC);

    inline Addr getTag(const Addr &inst_PC);

    BTBEntry *btb;
    
    unsigned numEntries;
    
    unsigned idxMask;
    
    unsigned tagBits;
    
    unsigned tagMask;
    
    unsigned instShiftAmt;

    unsigned tagShiftAmt;
};

#endif // __CPU_O3_CPU_BTB_HH__
