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

#include "base/cprintf.hh"
#include "encumbered/cpu/full/iq/segmented/seg_chain.hh"

using namespace std;

void
SegChainInfoEntry::tick(bool pipelined_mode) {
    head_prom_sr = head_prom_sr << 1;

    if (head_promoted) {
	if (pipelined_mode) {
	    // since the head has moved down one
	    head_prom_sr |= 1 << (head_level + 1);
	} else
	    head_prom_sr = 1;

	head_promoted = false;
    }
}

void
SegChainInfoEntry::dump(unsigned last_seg_num)
{
    cout << "  State: ";
    if (free) {
	cout << "free\n";
    } else {
	if (self_timed)
	    cout << "ST  ";
	else
	    cout << "ACT ";
	cout << "  hpv: ";
	for (int i = last_seg_num; i >= 0; --i) {
	    if (head_prom_sr & 1 << i)
		cout << '1';
	    else
		cout << '0';
	}
	cprintf("[0]\n"
		"  Head_Level: %u,  Chain_depth: %u\n"
		"  Creator: %d,  Created @%d\n",
		head_level, chain_depth, creator, created_ts);
    }
}


void
SegChainInfoEntry::dump()
{
    cout << "  State: ";
    if (free) {
	cout << "free\n";
    }
    else {
	if (self_timed)
	    cout << "ST  ";
	else
	    cout << "ACT ";

	cprintf("\n"
		"  Head_Level: %u,  Chain_depth: %u\n"
		"  Creator: %d,  Created @%d\n",
		head_level, chain_depth, creator, created_ts);
    }
}


//==========================================================================

SegChainInfoTable::SegChainInfoTable(unsigned num_chains, unsigned num_clust,
				     unsigned num_segments, bool p_mode)
    : ChainInfoTable<SegChainInfoEntry>(num_chains, num_clust)
{
    last_seg_index = num_segments - 1;
    pipelined_promotion = p_mode;
}


void
SegChainInfoTable::init(unsigned index)
{
    // initialize the base class members first
    ChainInfoTable<SegChainInfoEntry>::init(index);

    // initialize derived class members first
    table[index].head_level        = last_seg_index;
    table[index].head_promoted     = false;
    table[index].chain_depth       = 0;
    table[index].self_timed        = false;
}


//  This should happen at the END of the IQ::tick()
void
SegChainInfoTable::tick()
{
    for (unsigned i = 0; i < num_chains; ++i)
	table[i].tick(pipelined_promotion);
}


bool
SegChainInfoTable::head_was_promoted(unsigned chain, unsigned seg_num)
{
    if (pipelined_promotion)
	return (table[chain].head_prom_sr & (1 << seg_num));

    //  treat everyone just like segment zero
    return (table[chain].head_prom_sr & 1);
}


void
SegChainInfoTable::dump()
{
    cout << "===============================\n";
    cout << "Chain Table Dump @" << curTick << endl;
    cout << "-------------------------------\n";

    for (unsigned i = 0; i < num_chains; ++i) {
	cout << "Chain " << i;
	table[i].dump(last_seg_index);
    }
    cout << "===============================\n";
}
