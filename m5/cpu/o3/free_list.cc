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

#include "base/trace.hh"

#include "cpu/o3/free_list.hh"

SimpleFreeList::SimpleFreeList(unsigned _numLogicalIntRegs,
                               unsigned _numPhysicalIntRegs, 
                               unsigned _numLogicalFloatRegs,
                               unsigned _numPhysicalFloatRegs)
    : numLogicalIntRegs(_numLogicalIntRegs),
      numPhysicalIntRegs(_numPhysicalIntRegs),
      numLogicalFloatRegs(_numLogicalFloatRegs),
      numPhysicalFloatRegs(_numPhysicalFloatRegs),
      numPhysicalRegs(numPhysicalIntRegs + numPhysicalFloatRegs)
{
    DPRINTF(FreeList, "FreeList: Creating new free list object.\n");

    // DEBUG stuff.
    freeIntRegsScoreboard.resize(numPhysicalIntRegs);

    freeFloatRegsScoreboard.resize(numPhysicalRegs);

    for (PhysRegIndex i = 0; i < numLogicalIntRegs; ++i) {
        freeIntRegsScoreboard[i] = 0;
    }

    // Put all of the extra physical registers onto the free list.  This 
    // means excluding all of the base logical registers.
    for (PhysRegIndex i = numLogicalIntRegs; 
         i < numPhysicalIntRegs; ++i)
    {
        freeIntRegs.push(i);

        freeIntRegsScoreboard[i] = 1;
    }

    for (PhysRegIndex i = 0; i < numPhysicalIntRegs + numLogicalFloatRegs;
         ++i)
    {
        freeFloatRegsScoreboard[i] = 0;
    }

    // Put all of the extra physical registers onto the free list.  This 
    // means excluding all of the base logical registers.  Because the
    // float registers' indices start where the physical registers end,
    // some math must be done to determine where the free registers start.
    for (PhysRegIndex i = numPhysicalIntRegs + numLogicalFloatRegs;
         i < numPhysicalRegs; ++i)
    {
        freeFloatRegs.push(i);

        freeFloatRegsScoreboard[i] = 1;
    }
}

