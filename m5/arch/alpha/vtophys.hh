/*
 * Copyright (c) 2002, 2003, 2004, 2005
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

#ifndef __ARCH_ALPHA_VTOPHYS_H__
#define __ARCH_ALPHA_VTOPHYS_H__

#include "arch/alpha/isa_traits.hh"

class ExecContext;
class PhysicalMemory;

AlphaISA::PageTableEntry
kernel_pte_lookup(PhysicalMemory *pmem, Addr ptbr, AlphaISA::VAddr vaddr);

Addr vtophys(PhysicalMemory *xc, Addr vaddr);
Addr vtophys(ExecContext *xc, Addr vaddr);
uint8_t *vtomem(ExecContext *xc, Addr vaddr, size_t len);
uint8_t *ptomem(ExecContext *xc, Addr paddr, size_t len);

void CopyOut(ExecContext *xc, void *dst, Addr src, size_t len);
void CopyIn(ExecContext *xc, Addr dst, void *src, size_t len);
void CopyString(ExecContext *xc, char *dst, Addr vaddr, size_t maxlen);

#endif // __ARCH_ALPHA_VTOPHYS_H__

