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

#ifndef __ENCUMBERED_CPU_FULL_REG_INFO_HH__
#define __ENCUMBERED_CPU_FULL_REG_INFO_HH__

#include <cassert>
#include <iostream>
#include <vector>

#include "sim/host.hh"

class ROBStation;

class RegInfoElement
{
  private:
    //
    //  Common data elements
    //
    ROBStation *rob_producer;

    //
    //  Optional data elements: not all configurations will update
    //  these data values
    //
    Tick pred_ready_time;
    unsigned issue_latency;  // used by the segmented IQ

    //  Chains
    bool chained;
    unsigned chain_id;
    unsigned chain_depth;

    //  Clusters
    unsigned producing_cluster;


  public:
    RegInfoElement() {
	clear();
    }

    void clear() {
	rob_producer = 0;
	pred_ready_time = 0;
	issue_latency = 0;

	chained = false;

	rob_producer = 0;
	chain_id = 0;
	chain_depth = 0;
	producing_cluster = 0;
    }

    void setProducer(ROBStation *rob) {
	rob_producer = rob;
    }
    ROBStation *producer() {return rob_producer;}

    void setCluster(unsigned &clust) {producing_cluster = clust;}
    unsigned cluster() {return producing_cluster;}

    void setLatency(unsigned latency) {issue_latency = latency;}
    unsigned latency() {return issue_latency;}

    void setPredReady(Tick pred) {pred_ready_time = pred;}
    Tick predReady() {return pred_ready_time;}


    //
    //  Chaining stuff
    //
    void setChain(unsigned chain, unsigned depth=0) {
	chained     = true;
	chain_id    = chain;
	chain_depth = depth;   // zero indicates no info
    }

    void unChain() {
	chained = false;
    }

    bool isChained() {
	return chained;
    }

    unsigned chainNum() {
	assert(chained);
	return chain_id;
    }

    unsigned chainDepth() {
	assert(chained);
	return chain_depth;
    }

    void tickLatency() {
	if (issue_latency) {
	    --issue_latency;
	}
    }


    //
    //  Debugging support
    //
    void dump();
};


struct RegInfoTable
{
    typedef std::vector<RegInfoElement> RegInfoVector;

    std::vector<RegInfoVector> table;

    void init(unsigned threads, unsigned registers) {
	table.resize(threads);
	for (int t=0; t<threads; ++t) {
	    table[t].resize(registers);
//	    for (int r=0; r<registers; ++r) {
//		table[t][r].clear();
//	    }
	}
    }

    RegInfoTable() {}

    RegInfoVector & operator[](unsigned t) {return table[t];}

    //
    //  debugging support
    //
    void reg_dump(unsigned t, unsigned r) {
	table[t][r].dump();
    }
};

#endif // __ENCUMBERED_CPU_FULL_REG_INFO_HH__
