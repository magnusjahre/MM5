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

#ifndef __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_SEG_CHAIN_HH__
#define __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_SEG_CHAIN_HH__

#include "cpu/smt.hh"
#include "encumbered/cpu/full/iq/segmented/chain_info.hh"

class SegChainInfoEntry : public ChainInfoEntry
{
  public:
    unsigned head_level;
    bool self_timed;

    unsigned chain_depth;

    bool head_promoted;
    unsigned long long head_prom_sr;

    void dump(unsigned last_seg_idx);
    void dump();
    void tick(bool pipelined_mode);
};


class SegChainInfoTable : public ChainInfoTable<SegChainInfoEntry>
{
    unsigned last_seg_index;

    bool pipelined_promotion;

  public:
    SegChainInfoTable(unsigned n_chains, unsigned n_clust,
		      unsigned n_segments, bool p_mode);

    void init(unsigned index);
    void tick();
    void dump();
    bool head_was_promoted(unsigned chain, unsigned seg_num);
};


#endif // __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_SEG_CHAIN_HH__
