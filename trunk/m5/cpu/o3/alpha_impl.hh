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

#ifndef __CPU_O3_CPU_ALPHA_IMPL_HH__
#define __CPU_O3_CPU_ALPHA_IMPL_HH__

#include "arch/alpha/isa_traits.hh"

#include "cpu/o3/alpha_params.hh"
#include "cpu/o3/cpu_policy.hh"

// Forward declarations.
template <class Impl>
class AlphaDynInst;

template <class Impl>
class AlphaFullCPU;

/** Implementation specific struct that defines several key things to the
 *  CPU, the stages within the CPU, the time buffers, and the DynInst.
 *  The struct defines the ISA, the CPU policy, the specific DynInst, the
 *  specific FullCPU, and all of the structs from the time buffers to do
 *  communication.
 *  This is one of the key things that must be defined for each hardware
 *  specific CPU implementation.
 */
struct AlphaSimpleImpl
{
    /** The ISA to be used. */
    typedef AlphaISA ISA;

    /** The type of MachInst. */
    typedef ISA::MachInst MachInst;

    /** The CPU policy to be used (ie fetch, decode, etc.). */
    typedef SimpleCPUPolicy<AlphaSimpleImpl> CPUPol;

    /** The DynInst to be used. */
    typedef AlphaDynInst<AlphaSimpleImpl> DynInst;

    /** The refcounted DynInst pointer to be used.  In most cases this is
     *  what should be used, and not DynInst *.
     */
    typedef RefCountingPtr<DynInst> DynInstPtr;

    /** The FullCPU to be used. */
    typedef AlphaFullCPU<AlphaSimpleImpl> FullCPU;

    /** The Params to be passed to each stage. */
    typedef AlphaSimpleParams Params;

    enum {
        MaxWidth = 8
    };
};

#endif // __CPU_O3_CPU_ALPHA_IMPL_HH__
