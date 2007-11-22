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

#ifndef __ARCH_ALPHA_EV5_HH__
#define __ARCH_ALPHA_EV5_HH__

#include "config/alpha_tlaser.hh"

namespace EV5 {

#if ALPHA_TLASER
const uint64_t AsnMask = ULL(0x7f);
#else
const uint64_t AsnMask = ULL(0xff);
#endif

const int VAddrImplBits = 43;
const Addr VAddrImplMask = (ULL(1) << VAddrImplBits) - 1;
const Addr VAddrUnImplMask = ~VAddrImplMask;
inline Addr VAddrImpl(Addr a) { return a & VAddrImplMask; }
inline Addr VAddrVPN(Addr a) { return a >> AlphaISA::PageShift; }
inline Addr VAddrOffset(Addr a) { return a & AlphaISA::PageOffset; }
inline Addr VAddrSpaceEV5(Addr a) { return a >> 41 & 0x3; }
inline Addr VAddrSpaceEV6(Addr a) { return a >> 41 & 0x7f; }

#if ALPHA_TLASER
inline bool PAddrIprSpace(Addr a) { return a >= ULL(0xFFFFF00000); }
const int PAddrImplBits = 40;
#else
inline bool PAddrIprSpace(Addr a) { return a >= ULL(0xFFFFFF00000); }
const int PAddrImplBits = 44; // for Tsunami
#endif
const Addr PAddrImplMask = (ULL(1) << PAddrImplBits) - 1;
const Addr PAddrUncachedBit39 = ULL(0x8000000000);
const Addr PAddrUncachedBit40 = ULL(0x10000000000);
const Addr PAddrUncachedBit43 = ULL(0x80000000000);
const Addr PAddrUncachedMask = ULL(0x807ffffffff); // Clear PA<42:35>
inline Addr Phys2K0Seg(Addr addr)
{
#if !ALPHA_TLASER
    if (addr & PAddrUncachedBit43) {
        addr &= PAddrUncachedMask;
        addr |= PAddrUncachedBit40;
    }
#endif
    return addr | AlphaISA::K0SegBase;
}

inline int DTB_ASN_ASN(uint64_t reg) { return reg >> 57 & AsnMask; }
inline Addr DTB_PTE_PPN(uint64_t reg)
{ return reg >> 32 & (ULL(1) << PAddrImplBits - AlphaISA::PageShift) - 1; }
inline int DTB_PTE_XRE(uint64_t reg) { return reg >> 8 & 0xf; }
inline int DTB_PTE_XWE(uint64_t reg) { return reg >> 12 & 0xf; }
inline int DTB_PTE_FONR(uint64_t reg) { return reg >> 1 & 0x1; }
inline int DTB_PTE_FONW(uint64_t reg) { return reg >> 2 & 0x1; }
inline int DTB_PTE_GH(uint64_t reg) { return reg >> 5 & 0x3; }
inline int DTB_PTE_ASMA(uint64_t reg) { return reg >> 4 & 0x1; }

inline int ITB_ASN_ASN(uint64_t reg) { return reg >> 4 & AsnMask; }
inline Addr ITB_PTE_PPN(uint64_t reg)
{ return reg >> 32 & (ULL(1) << PAddrImplBits - AlphaISA::PageShift) - 1; }
inline int ITB_PTE_XRE(uint64_t reg) { return reg >> 8 & 0xf; }
inline bool ITB_PTE_FONR(uint64_t reg) { return reg >> 1 & 0x1; }
inline bool ITB_PTE_FONW(uint64_t reg) { return reg >> 2 & 0x1; }
inline int ITB_PTE_GH(uint64_t reg) { return reg >> 5 & 0x3; }
inline bool ITB_PTE_ASMA(uint64_t reg) { return reg >> 4 & 0x1; }

inline uint64_t MCSR_SP(uint64_t reg) { return reg >> 1 & 0x3; }

inline bool ICSR_SDE(uint64_t reg) { return reg >> 30 & 0x1; }
inline int ICSR_SPE(uint64_t reg) { return reg >> 28 & 0x3; }
inline bool ICSR_FPE(uint64_t reg) { return reg >> 26 & 0x1; }

inline uint64_t ALT_MODE_AM(uint64_t reg) { return reg >> 3 & 0x3; }
inline uint64_t DTB_CM_CM(uint64_t reg) { return reg >> 3 & 0x3; }
inline uint64_t ICM_CM(uint64_t reg) { return reg >> 3 & 0x3; }

const uint64_t MM_STAT_BAD_VA_MASK = ULL(0x0020);
const uint64_t MM_STAT_DTB_MISS_MASK = ULL(0x0010);
const uint64_t MM_STAT_FONW_MASK = ULL(0x0008);
const uint64_t MM_STAT_FONR_MASK = ULL(0x0004);
const uint64_t MM_STAT_ACV_MASK = ULL(0x0002);
const uint64_t MM_STAT_WR_MASK = ULL(0x0001);
inline int Opcode(AlphaISA::MachInst inst) { return inst >> 26 & 0x3f; }
inline int Ra(AlphaISA::MachInst inst) { return inst >> 21 & 0x1f; }

const Addr PalBase = 0x4000;
const Addr PalMax = 0x10000;

/* namespace EV5 */ }

#endif // __ARCH_ALPHA_EV5_HH__
