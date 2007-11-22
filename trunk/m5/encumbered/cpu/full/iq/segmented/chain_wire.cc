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

#include <iostream>

#include "base/misc.hh"
#include "encumbered/cpu/full/iq/segmented/chain_wire.hh"
#include "sim/root.hh"

using namespace std;

//==========================================================================
//
//  Policies:   OneToOne = Each cluster has one wire/chain
//              Static   = Each cluster statically allocates chains to wires
//                         (wires are evenly distributed among clusters)
//              Dynamic  = Each cluster allocates chains to available wires
//                         (wires are evenly distributed among clusters)
//
//
//
ChainWireInfo::ChainWireInfo(unsigned _chains, unsigned _wires,
			     unsigned _clusts, MappingPolicy _policy)
{
    policy = _policy;
    num_chains = _chains;
    num_wires = _wires;
    num_clusters = _clusts;

    if (_clusts > 1 && (num_wires % _clusts))
	cerr << "*\n*\n*  WARNING: number of wires not evenly divisible "
	    "among clusters\n*\n";

    //  allocate wire counters
    wires_in_use = new unsigned[_clusts];
    num_clust_wires = new unsigned[_clusts];

    // allocate array of pointers
    wireMap = new WireMap *[_clusts];

    for (int i = 0; i < _clusts; ++i) {
	wires_in_use[i] = 0;

	switch(policy) {
	  case OneToOne:
	    num_clust_wires[i] = num_chains;
	    break;
	  case Static:
	  case StaticStall:
	  case Dynamic:
	    num_clust_wires[i] = num_wires / _clusts;
	    break;
	  default:
	    panic("Illegal Chain Wire Policy");
	    break;
	}

	// allocate array of int for each cluster
	wireMap[i] = new WireMap[num_chains];

	for (int c = 0; c < num_chains; ++c) {
	    switch(policy) {
	      case OneToOne:
		// each chain gets a wire
		wireMap[i][c].mapped = true;
		wireMap[i][c].free = true;
		break;
	      case Static:
	      case StaticStall:
		// chains evenly distributed among clusters
		if ((c % _clusts) == i) {
		    wireMap[i][c].mapped = true;
		    wireMap[i][c].free = true;
		}
		break;
	      case Dynamic:
		// initiall, no chains allocated
		break;
	    }
	}
    }
}


int
ChainWireInfo::findFreeWire(unsigned clust)
{
    if (freeWires(clust) == 0)
	return -1;

    switch (policy) {
      case OneToOne:
      case Static:
      case StaticStall:
	for (int w = 0; w < num_chains; ++w)
	    if (wireAllocatable(clust, w))
		return w;
	break;

      case Dynamic:
	for (int w = 0; w < num_chains; ++w)
	    if (!wireMap[clust][w].mapped)
		return w;
	break;
    }

    return -1;
}


unsigned
ChainWireInfo::freeWires(unsigned clust)
{
    return num_clust_wires[clust] - wires_in_use[clust];
}


bool
ChainWireInfo::chainMapped(unsigned clust, unsigned chain)
{
    return wireMap[clust][chain].mapped;
}


bool
ChainWireInfo::wireAllocatable(unsigned clust, unsigned chain)
{
    if (policy == Dynamic) {
	bool rv = wires_in_use[clust] < num_clust_wires[clust];
	rv = rv & ! wireMap[clust][chain].mapped;
	return rv;
    } else {
	//  WireMapped && Free
	return wireMap[clust][chain].allocatable();
    }
}


void
ChainWireInfo::allocateWire(unsigned clust, unsigned chain)
{
    m5_assert(wireAllocatable(clust,chain));

    switch (policy) {
      case OneToOne:
      case Static:
      case StaticStall:
	wireMap[clust][chain].free = false;
	break;

      case Dynamic:
	wireMap[clust][chain].mapped = true;
	break;
    }

    ++wires_in_use[clust];
}

//--------------------------------------------------------------------
//
//   For the dynamic allocation policy
//
//
//

void
ChainWireInfo::releaseWire(unsigned clust, unsigned chain)
{
    //
    //  not all chains have wires mapped in all clusters...
    //
    switch(policy) {
      case OneToOne:
	//
	//  all chains are mapped in all clusters, but only some clusters
	//  will have wire marked as being in use...
	//
	if (!wireMap[clust][chain].free) {
	    wireMap[clust][chain].free = true;
	    assert(wires_in_use[clust]);
	    --wires_in_use[clust];
	}
	break;

      case Static:
      case StaticStall:
	if (wireMap[clust][chain].mapped) {
	    m5_assert(!wireMap[clust][chain].free);

	    wireMap[clust][chain].free = true;

	    --wires_in_use[clust];
	}
	break;

      case Dynamic:
	if (wireMap[clust][chain].mapped) {

	    wireMap[clust][chain].mapped = false;
	    // dynamic doesn't really use the "free" member

	    --wires_in_use[clust];
	}
	break;
    }
}


void
ChainWireInfo::dump()
{
    cout << "===============================================\n";
    cout << " Chain Wire Dump\n";
    cout << "-----------------------------------------------\n";

    for (int clust = 0; clust < num_clusters; ++clust)
	cout << "  cluster " << clust << " has " << num_clust_wires[clust]
	     << " wires (" << wires_in_use[clust] << " in use)\n";

    for (int clust = 0; clust < num_clusters; ++clust) {
	cout << "-----------------------------------------------\n";
	cout << " Cluster " << clust << " Wire State\n";
	for (int w = 0; w < num_chains; ++w)
	    wireMap[clust][w].dump();
    }
}


bool
ChainWireInfo::sanityCheckOK()
{
    bool rv = true;

    for (int clust = 0; clust < num_clusters; ++clust) {
	unsigned map_count = 0;
	unsigned used_count = 0;

	for (int w = 0; w < num_chains; ++w) {
	    if (wireMap[clust][w].mapped) {
		++map_count;

		if (!wireMap[clust][w].free)
		    ++used_count;
	    }
	}

	if (policy == Dynamic) {
	    if (map_count != wires_in_use[clust]) {
		cerr << "ChainWireInfo: Cluster " << clust
		     << " Insane! (map_count["<< clust << "] is "
		     << map_count << ", should be "
		     << wires_in_use[clust] << ")\n";
		rv = false;
	    }
	} else {
	    if (map_count != num_clust_wires[clust]) {
		cerr << "ChainWireInfo: Cluster " << clust
		     << " Insane! (map_count["<< clust << "] is "
		     << map_count << ", should be "
		     << num_clust_wires[clust] << ")\n";
		rv = false;
	    }
	}

	if (used_count != wires_in_use[clust]) {
	    cerr << "ChainWireInfo: Cluster " << clust
		 << " Insane! (wires-in-use["<< clust << "] is "
		 << wires_in_use[clust] << ", should be "
		 << used_count << ")\n";
	    rv = false;
	}
    }

    return rv;
}
