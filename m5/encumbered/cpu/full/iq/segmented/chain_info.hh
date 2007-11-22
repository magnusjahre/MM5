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

#ifndef __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_CHAIN_INFO_HH__
#define __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_CHAIN_INFO_HH__

#include <cassert>
#include <iostream>

#include "base/trace.hh"
#include "cpu/inst_seq.hh"
#include "cpu/smt.hh"
#include "sim/root.hh"


///////////////////////////////////////////////////////////////////////////////
//
//  This information is needed by both the queue itself and the
//  individual segments
//
class ChainInfoEntry
{
  public:
    unsigned released_clusters;

    InstSeqNum creator;
    Tick created_ts;

    bool free;

    //
    //  Methods
    //
    void dump();
};

class ChainInfoTableBase
{
  public:
    virtual int find_free() = 0;
    virtual void claim(unsigned i, unsigned t, InstSeqNum s) = 0;
    virtual void init(unsigned i) = 0;
    virtual bool release(unsigned i, unsigned t) = 0;
    virtual bool sanityCheckOK() = 0;
    virtual void dump() {};
    virtual unsigned chainsFree() = 0;
    virtual ~ChainInfoTableBase() {};
};

template <class T = ChainInfoEntry>
class ChainInfoTable : public ChainInfoTableBase
{
  private:
    unsigned num_clusters;
    unsigned chains_in_use;
    unsigned thread_chains[SMT_MAX_THREADS];
    unsigned last_chain_found;

  protected:
    T *table;
    unsigned num_chains;

  public:
    ChainInfoTable(unsigned n_chains, unsigned n_clust) {
	num_chains = n_chains;
	num_clusters = n_clust;

	table = new T [num_chains];

	for (unsigned i = 0; i < num_chains; ++i)
	    init(i);

	for (int t = 0; t < SMT_MAX_THREADS; ++t)
	    thread_chains[t] = 0;

	last_chain_found = 0;
	chains_in_use = 0;
    }

    //  allow access to the chain data elements
    T & operator[](unsigned index) {
	return table[index];
    }

    unsigned chainsInUse() { return chains_in_use; }
    unsigned chainsInUseThread(int t) { return thread_chains[t]; }
    unsigned chainsFree() { return num_chains - chains_in_use; }

    virtual void claim(unsigned chain, unsigned thread, InstSeqNum seq) {

	table[chain].free = false;
	table[chain].creator = seq;
	table[chain].created_ts = curTick;
	table[chain].released_clusters = 0;

	++chains_in_use;
	++thread_chains[thread];

	DPRINTF(Chains, "Chains: thread %d claims chain %d (%d free)\n",
		thread, chain, chainsFree());
	assert(chains_in_use <= num_chains);
    }

    virtual void init(unsigned index) {
	table[index].released_clusters = 0;
	table[index].creator           = 0;
	table[index].created_ts        = 0;
	table[index].free              = true;
    }

    virtual bool release(unsigned chain, unsigned thread) {
	DPRINTF(Chains, "Chains: thread %d releases chain %d\n", thread,
		chain);

	// releasing a free chain is a bad thing
	assert(!table[chain].free);

	++table[chain].released_clusters;

	//  if all clusters have released this entry, then clear it
	if (table[chain].released_clusters == num_clusters) {
	    init(chain);
	    --chains_in_use;
	    --thread_chains[thread];
	    DPRINTF(Chains,
		    "Chains: chain %d release is complete (%d free)\n",
		    chain, chainsFree());
	    return true;
	}

	return false;
    }


    virtual int find_free() {
	if (chains_in_use == num_chains)
	    return -1;

	// this is designed to force the use of chain zero first
	// (mostly for comparison purposes w/ earlier code)
	unsigned chain = 0;
	if (chains_in_use != 0)
	    chain = (last_chain_found + 1) % num_chains;

	do {
	    if (table[chain].free) {
		last_chain_found = chain;
		return (int)chain;
	    }

	    //  increment & handle wrapping...
	    if (++chain >= num_chains)
		chain = 0;
	} while (chain != last_chain_found);

	return -1;
    }

    bool sanityCheckOK() {
	bool rv = true;

	unsigned free = 0;
	for (int i = 0; i < num_chains; ++i)
	    if (!table[i].free)
		++free;

	if (free != chains_in_use) {
	    std::cerr << curTick
		 << ": ChainInfoTable: Sanity Check Failed! chains_in_use("
		 << chains_in_use << ") != # free (" << free << ")\n";
	    rv = false;
	}

	unsigned ct = 0;
	for (int t = 0; t < SMT_MAX_THREADS; ++t)
	    ct += thread_chains[t];

	if (ct != chains_in_use) {
	    std::cerr << curTick
		 << ": ChainInfoTable: Sanity Check Failed! chains_in_use("
		 << chains_in_use << ") != thread_chains(" << ct << ")\n";
	    rv = false;
	}

	if (ct != free) {
	    std::cerr << curTick
		 << ": ChainInfoTable: Sanity Check Failed! thread_chains("
		 << ct << ") != # free (" << free << ")\n";
	    rv = false;
	}

	return rv;
    }

    void dump() {
	std::cout << "===============================\n";
	std::cout << "Chain Table Dump @" << curTick << std::endl;
	std::cout << "-------------------------------\n";

	for (unsigned i = 0; i < num_chains; ++i) {
	    std::cout << "Chain " << i;
	    table[i].dump();
	}
	std::cout << "===============================\n";
    }
};

#endif // __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_CHAIN_INFO_HH__
