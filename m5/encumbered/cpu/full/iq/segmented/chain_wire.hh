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

#ifndef __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_CHAIN_WIRE_HH__
#define __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_CHAIN_WIRE_HH__

#include <iostream>

/*======================================================================

Explanation of chain-mapping policies:

   OneToOne:  This is the original segmented IQ policy. Each chain is
              staticaly allocated a wire in each cluster.

   Dynamic:   Each cluster holds an equal fraction of all available
              wires (specified with the 'max_wires' CPU parameter).

              Any chain ID can be assigned to any cluster with a free
	      chain-wire. If the instruction cannot be assigned to the
	      preferred "producing" cluster, it can be assigned to a
	      "second-chance" cluster (the cluster with the lowest
	      occupancy). If the second chance cluster is full or is
	      the same cluster as the "producing" cluster, dispatch
	      is stalled.

   Static:    Each cluster holds an equal fraction of all available
              wires (specified with the 'max_wires' CPU parameter).
	      These chain-wires are statically assigned a chain-ID.

              A chain can only be assigned to the cluster where its
	      chain-wire is assigned. If the instruction cannot be
	      assigned to the preferred "producing" cluster, it can
	      be assigned to a "second-chance" cluster (the cluster
	      with the lowest occupancy). If the second chance cluster
	      is full or is the same cluster as the "producing" cluster,
	      dispatch is stalled.

   Static-Stall:  Each cluster holds an equal fraction of all available
              wires (specified with the 'max_wires' CPU parameter).
	      These chain-wires are statically assigned a chain-ID.

              A chain can only be assigned to the cluster where its
	      chain-wire is assigned. If the instruction cannot be
	      assigned to the preferred "producing" cluster, dispatch
	      is stalled.

======================================================================*/



class ChainWireInfo
{
  public:
    enum MappingPolicy {OneToOne, Static, StaticStall, Dynamic};

  private:
    //
    //  Configuration args
    //
    MappingPolicy policy;
    unsigned num_clusters;
    unsigned num_wires;       //  total wires (all clusters)
    unsigned num_chains;      //  total chains

    struct WireMap {
	bool  mapped;   // this chain is mapped to this cluster
	bool  free;     // this wire is free to be used

	WireMap() {mapped = false; free = false;}

	bool allocatable() { return mapped && free; }

	void dump() {
	    std::cout << "  mapped: " << mapped << ", free: " << free
		      << std::endl;
	}
    };

    WireMap **wireMap;

    unsigned *wires_in_use;      // wires in use (each cluster)
    unsigned *num_clust_wires;   // total wires (each cluster)

  public:
    ChainWireInfo(unsigned _chains, unsigned _wires, unsigned _clust,
		  MappingPolicy _pol);

    bool wireAllocatable(unsigned clust, unsigned chain);
    bool chainMapped(unsigned clust, unsigned chain);
    unsigned freeWires(unsigned clust);

    int findFreeWire(unsigned clust);

    void allocateWire(unsigned clust, unsigned chain);
    void releaseWire(unsigned clust, unsigned chain);

    void dump();
    bool sanityCheckOK();
};

#endif  // __ENCUMBERED_CPU_FULL_IQ_SEGMENTED_CHAIN_WIRE_HH__
