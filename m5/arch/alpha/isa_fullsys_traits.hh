/*
 * Copyright (c) 2003, 2004, 2005
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

#ifndef __ARCH_ALPHA_ISA_FULLSYS_TRAITS_HH__
#define __ARCH_ALPHA_ISA_FULLSYS_TRAITS_HH__

//
// This file contains declarations that go *inside* the AlphaISA
// traits class.  It should *not* be included directly from anywhere
// but alpha/isa_traits.hh.
//

// Alpha IPR register accessors
static inline bool PcPAL(Addr addr) { return addr & 0x1; }

////////////////////////////////////////////////////////////////////////
//
//  Translation stuff
//

static const Addr PteShift = 3;
static const Addr NPtePageShift = PageShift - PteShift;
static const Addr NPtePage = ULL(1) << NPtePageShift;
static const Addr PteMask = NPtePage - 1;
static inline Addr PteAddr(Addr a) { return (a & PteMask) << PteShift; }

// User Virtual
static const Addr USegBase = ULL(0x0);
static const Addr USegEnd = ULL(0x000003ffffffffff);
static inline bool IsUSeg(Addr a) { return USegBase <= a && a <= USegEnd; }

// Kernel Direct Mapped
static const Addr K0SegBase = ULL(0xfffffc0000000000);
static const Addr K0SegEnd = ULL(0xfffffdffffffffff);
static inline bool IsK0Seg(Addr a) { return K0SegBase <= a && a <= K0SegEnd; }
static inline Addr K0Seg2Phys(Addr addr) { return addr & ~K0SegBase; }

// Kernel Virtual
static const Addr K1SegBase = ULL(0xfffffe0000000000);
static const Addr K1SegEnd = ULL(0xffffffffffffffff);
static inline bool IsK1Seg(Addr a) { return K1SegBase <= a && a <= K1SegEnd; }

static inline Addr
TruncPage(Addr addr)
{ return addr & ~(PageBytes - 1); }

static inline Addr
RoundPage(Addr addr)
{ return (addr + PageBytes - 1) & ~(PageBytes - 1); }

struct VAddr
{
    static const int ImplBits = 43;
    static const Addr ImplMask = (ULL(1) << ImplBits) - 1;
    static const Addr UnImplMask = ~ImplMask;

    VAddr(Addr a) : addr(a) {}
    Addr addr;
    operator Addr() const { return addr; }
    const VAddr &operator=(Addr a) { addr = a; return *this; }

    Addr vpn() const { return (addr & ImplMask) >> PageShift; }
    Addr page() const { return addr & PageMask; }
    Addr offset() const { return addr & PageOffset; }

    Addr level3() const
    { return PteAddr(addr >> PageShift); }
    Addr level2() const
    { return PteAddr(addr >> NPtePageShift + PageShift); }
    Addr level1() const
    { return PteAddr(addr >> 2 * NPtePageShift + PageShift); }
};

struct PageTableEntry
{
    PageTableEntry(uint64_t e) : entry(e) {}
    uint64_t entry;
    operator uint64_t() const { return entry; }
    const PageTableEntry &operator=(uint64_t e) { entry = e; return *this; }
    const PageTableEntry &operator=(const PageTableEntry &e)
    { entry = e.entry; return *this; }

    Addr _pfn()  const { return (entry >> 32) & 0xffffffff; }
    Addr _sw()   const { return (entry >> 16) & 0xffff; }
    int  _rsv0() const { return (entry >> 14) & 0x3; }
    bool _uwe()  const { return (entry >> 13) & 0x1; }
    bool _kwe()  const { return (entry >> 12) & 0x1; }
    int  _rsv1() const { return (entry >> 10) & 0x3; }
    bool _ure()  const { return (entry >>  9) & 0x1; }
    bool _kre()  const { return (entry >>  8) & 0x1; }
    bool _nomb() const { return (entry >>  7) & 0x1; }
    int  _gh()   const { return (entry >>  5) & 0x3; }
    bool _asm()  const { return (entry >>  4) & 0x1; }
    bool _foe()  const { return (entry >>  3) & 0x1; }
    bool _fow()  const { return (entry >>  2) & 0x1; }
    bool _for()  const { return (entry >>  1) & 0x1; }
    bool valid() const { return (entry >>  0) & 0x1; }

    Addr paddr() const { return _pfn() << PageShift; }
};

// ITB/DTB page table entry
struct PTE
{
    Addr tag;			// virtual page number tag
    Addr ppn;			// physical page number
    uint8_t xre;		// read permissions - VMEM_PERM_* mask
    uint8_t xwe;		// write permissions - VMEM_PERM_* mask
    uint8_t asn;		// address space number
    bool asma;			// address space match
    bool fonr;			// fault on read
    bool fonw;			// fault on write
    bool valid;			// valid page table entry

    void serialize(std::ostream &os);
    void unserialize(Checkpoint *cp, const std::string &section);
};

////////////////////////////////////////////////////////////////////////
//
//  Internal Processor Reigsters
//
enum md_ipr_names
{
    IPR_ISR = 0x100,		// interrupt summary register
    IPR_ITB_TAG = 0x101,	// ITLB tag register
    IPR_ITB_PTE = 0x102,	// ITLB page table entry register
    IPR_ITB_ASN = 0x103,	// ITLB address space register
    IPR_ITB_PTE_TEMP = 0x104,	// ITLB page table entry temp register
    IPR_ITB_IA = 0x105,		// ITLB invalidate all register
    IPR_ITB_IAP = 0x106,	// ITLB invalidate all process register
    IPR_ITB_IS = 0x107,		// ITLB invalidate select register
    IPR_SIRR = 0x108,		// software interrupt request register
    IPR_ASTRR = 0x109,		// asynchronous system trap request register
    IPR_ASTER = 0x10a,		// asynchronous system trap enable register
    IPR_EXC_ADDR = 0x10b,	// exception address register
    IPR_EXC_SUM = 0x10c,	// exception summary register
    IPR_EXC_MASK = 0x10d,	// exception mask register
    IPR_PAL_BASE = 0x10e,	// PAL base address register
    IPR_ICM = 0x10f,		// instruction current mode
    IPR_IPLR = 0x110,		// interrupt priority level register
    IPR_INTID = 0x111,		// interrupt ID register
    IPR_IFAULT_VA_FORM = 0x112,	// formatted faulting virtual addr register
    IPR_IVPTBR = 0x113,		// virtual page table base register
    IPR_HWINT_CLR = 0x115,	// H/W interrupt clear register
    IPR_SL_XMIT = 0x116,	// serial line transmit register
    IPR_SL_RCV = 0x117,		// serial line receive register
    IPR_ICSR = 0x118,		// instruction control and status register
    IPR_IC_FLUSH = 0x119,	// instruction cache flush control
    IPR_IC_PERR_STAT = 0x11a,	// inst cache parity error status register
    IPR_PMCTR = 0x11c,		// performance counter register

    // PAL temporary registers...
    // register meanings gleaned from osfpal.s source code
    IPR_PALtemp0 = 0x140,	// local scratch
    IPR_PALtemp1 = 0x141,	// local scratch
    IPR_PALtemp2 = 0x142,	// entUna
    IPR_PALtemp3 = 0x143,	// CPU specific impure area pointer
    IPR_PALtemp4 = 0x144,	// memory management temp
    IPR_PALtemp5 = 0x145,	// memory management temp
    IPR_PALtemp6 = 0x146,	// memory management temp
    IPR_PALtemp7 = 0x147,	// entIF
    IPR_PALtemp8 = 0x148,	// intmask
    IPR_PALtemp9 = 0x149,	// entSys
    IPR_PALtemp10 = 0x14a,	// ??
    IPR_PALtemp11 = 0x14b,	// entInt
    IPR_PALtemp12 = 0x14c,	// entArith
    IPR_PALtemp13 = 0x14d,	// reserved for platform specific PAL
    IPR_PALtemp14 = 0x14e,	// reserved for platform specific PAL
    IPR_PALtemp15 = 0x14f,	// reserved for platform specific PAL
    IPR_PALtemp16 = 0x150,	// scratch / whami<7:0> / mces<4:0>
    IPR_PALtemp17 = 0x151,	// sysval
    IPR_PALtemp18 = 0x152,	// usp
    IPR_PALtemp19 = 0x153,	// ksp
    IPR_PALtemp20 = 0x154,	// PTBR
    IPR_PALtemp21 = 0x155,	// entMM
    IPR_PALtemp22 = 0x156,	// kgp
    IPR_PALtemp23 = 0x157,	// PCBB

    IPR_DTB_ASN = 0x200,	// DTLB address space number register
    IPR_DTB_CM = 0x201,		// DTLB current mode register
    IPR_DTB_TAG = 0x202,	// DTLB tag register
    IPR_DTB_PTE = 0x203,	// DTLB page table entry register
    IPR_DTB_PTE_TEMP = 0x204,	// DTLB page table entry temporary register

    IPR_MM_STAT = 0x205,	// data MMU fault status register
    IPR_VA = 0x206,		// fault virtual address register
    IPR_VA_FORM = 0x207,	// formatted virtual address register
    IPR_MVPTBR = 0x208,		// MTU virtual page table base register
    IPR_DTB_IAP = 0x209,	// DTLB invalidate all process register
    IPR_DTB_IA = 0x20a,		// DTLB invalidate all register
    IPR_DTB_IS = 0x20b,		// DTLB invalidate single register
    IPR_ALT_MODE = 0x20c,	// alternate mode register
    IPR_CC = 0x20d,		// cycle counter register
    IPR_CC_CTL = 0x20e,		// cycle counter control register
    IPR_MCSR = 0x20f,		// MTU control register

    IPR_DC_FLUSH = 0x210,
    IPR_DC_PERR_STAT = 0x212,	// Dcache parity error status register
    IPR_DC_TEST_CTL = 0x213,	// Dcache test tag control register
    IPR_DC_TEST_TAG = 0x214,	// Dcache test tag register
    IPR_DC_TEST_TAG_TEMP = 0x215, // Dcache test tag temporary register
    IPR_DC_MODE = 0x216,	// Dcache mode register
    IPR_MAF_MODE = 0x217,	// miss address file mode register

    NumInternalProcRegs		// number of IPR registers
};

////////////////////////////////////////////////////////////////////////
//
// Alpha Exceptions
//
static Addr fault_addr[Num_Faults];

////////////////////////////////////////////////////////////////////////
//
//  Interrupt levels
//
enum InterruptLevels
{
    INTLEVEL_SOFTWARE_MIN = 4,
    INTLEVEL_SOFTWARE_MAX = 19,

    INTLEVEL_EXTERNAL_MIN = 20,
    INTLEVEL_EXTERNAL_MAX = 34,

    INTLEVEL_IRQ0 = 20,
    INTLEVEL_IRQ1 = 21,
    INTINDEX_ETHERNET = 0,
    INTINDEX_SCSI = 1,
    INTLEVEL_IRQ2 = 22,
    INTLEVEL_IRQ3 = 23,

    INTLEVEL_SERIAL = 33,

    NumInterruptLevels = INTLEVEL_EXTERNAL_MAX
};


// EV5 modes
enum mode_type
{
    mode_kernel = 0,		// kernel
    mode_executive = 1,		// executive (unused by unix)
    mode_supervisor = 2,	// supervisor (unused by unix)
    mode_user = 3,		// user mode
    mode_number			// number of modes
};

struct RegFile;

static void intr_post(RegFile *regs, Fault fault, Addr pc);
static void swap_palshadow(RegFile *regs, bool use_shadow);
static void initCPU(RegFile *regs);
static void initIPRs(RegFile *regs);

/** 
 * Function to check for and process any interrupts.
 * @param xc The execution context.
 */
template <class XC>
static void processInterrupts(XC *xc);

// redirected register map
static const int reg_redir[NumIntRegs];

#endif // __ARCH_ALPHA_ISA_FULLSYS_TRAITS_HH__
