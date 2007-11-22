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

/**
 * @file
 * Definition of a memory trace CPU object for optimal caches. Uses a memory 
 * trace to access a fully associative cache with optimal replacement.
 */

#include <algorithm> // For heap functions.

#include "cpu/trace/opt_cpu.hh"
#include "cpu/trace/reader/mem_trace_reader.hh"

#include "sim/builder.hh"
#include "sim/sim_events.hh"

using namespace std;

OptCPU::OptCPU(const string &name,
	       MemTraceReader *_trace,
	       int block_size,
	       int cache_size,
	       int _assoc)
    : SimObject(name), tickEvent(this), trace(_trace),
      numBlks(cache_size/block_size), assoc(_assoc), numSets(numBlks/assoc),
      setMask(numSets - 1)
{
    int log_block_size = 0;
    int tmp_block_size = block_size;
    while (tmp_block_size > 1) {
	++log_block_size;
	tmp_block_size = tmp_block_size >> 1;
    }
    assert(1<<log_block_size == block_size);
    MemReqPtr req;
    trace->getNextReq(req);
    refInfo.resize(numSets);
    while (req) {
	RefInfo temp;
	temp.addr = req->paddr >> log_block_size;
	int set = temp.addr & setMask;
	refInfo[set].push_back(temp);
	trace->getNextReq(req);
    }

    // Initialize top level of lookup table.
    lookupTable.resize(16);
    
    // Annotate references with next ref time.
    for (int k = 0; k < numSets; ++k) {
	for (RefIndex i = refInfo[k].size() - 1; i >= 0; --i) {
	    Addr addr = refInfo[k][i].addr;
	    initTable(addr, InfiniteRef);
	    refInfo[k][i].nextRefTime = lookupValue(addr);
	    setValue(addr, i);
	}
    }
    
    // Reset the lookup table
    for (int j = 0; j < 16; ++j) {
	if (lookupTable[j].size() == (1<<16)) {
	    for (int k = 0; k < (1<<16); ++k) {
		if (lookupTable[j][k].size() == (1<<16)) {
		    for (int l = 0; l < (1<<16); ++l) {
			lookupTable[j][k][l] = -1;
		    }
		}
	    }
	}
    }
    
    tickEvent.schedule(0);

    hits = 0;
    misses = 0;
}

void
OptCPU::processSet(int set)
{
    // Initialize cache
    int blks_in_cache = 0;
    RefIndex i = 0;
    cacheHeap.clear();
    cacheHeap.resize(assoc);
    
    while (blks_in_cache < assoc) {
	RefIndex cache_index = lookupValue(refInfo[set][i].addr);
	if (cache_index == -1) {
	    // First reference to this block
	    misses++;
	    cache_index = blks_in_cache++;
	    setValue(refInfo[set][i].addr, cache_index);
	} else {
	    hits++;
	}
	// update cache heap to most recent reference
	cacheHeap[cache_index] = i;
	if (++i >= refInfo[set].size()) {
	    return;
	}
    }
    for (int start = assoc/2; start >= 0; --start) {
	heapify(set,start);
    }
    //verifyHeap(set,0);
    
    for (; i < refInfo[set].size(); ++i) {
	RefIndex cache_index = lookupValue(refInfo[set][i].addr);
	if (cache_index == -1) {
	    // miss
	    misses++;
	    // replace from cacheHeap[0]
	    // mark replaced block as absent
	    setValue(refInfo[set][cacheHeap[0]].addr, -1);
	    setValue(refInfo[set][i].addr, 0);
	    cacheHeap[0] = i;
	    heapify(set, 0);
	    // Make sure its in the cache
	    assert(lookupValue(refInfo[set][i].addr) != -1);
	} else {
	    // hit
	    hits++;
	    assert(refInfo[set][cacheHeap[cache_index]].addr == 
		   refInfo[set][i].addr);
	    assert(refInfo[set][cacheHeap[cache_index]].nextRefTime == i);
	    assert(heapLeft(cache_index) >= assoc);
	    
	    cacheHeap[cache_index] = i;
	    processRankIncrease(set, cache_index);
	    assert(lookupValue(refInfo[set][i].addr) != -1);
	}
    }  
}
void
OptCPU::tick()
{
    // Do opt simulation
    
    int references = 0;
    for (int set = 0; set < numSets; ++set) {
	if (!refInfo[set].empty()) {
	    processSet(set);
	}
	references += refInfo[set].size();
    }
    // exit;
    fprintf(stderr,"sys.cpu.misses %d #opt cache misses\n",misses);
    fprintf(stderr,"sys.cpu.hits %d #opt cache hits\n", hits);
    fprintf(stderr,"sys.cpu.accesses %d #opt cache acceses\n", references);
    new SimExitEvent("Finshed Memory Trace");
}

void
OptCPU::initTable(Addr addr, RefIndex index)
{
    int l1_index = (addr >> 32) & 0x0f;
    int l2_index = (addr >> 16) & 0xffff;
    assert(l1_index == addr >> 32);
    if (lookupTable[l1_index].size() != (1<<16)) {
	lookupTable[l1_index].resize(1<<16);
    }
    if (lookupTable[l1_index][l2_index].size() != (1<<16)) {
	lookupTable[l1_index][l2_index].resize(1<<16, index);
    }
}

OptCPU::TickEvent::TickEvent(OptCPU *c)
    : Event(&mainEventQueue, CPU_Tick_Pri), cpu(c)
{
}

void
OptCPU::TickEvent::process()
{
    cpu->tick();
}

const char *
OptCPU::TickEvent::description()
{
    return "OptCPU tick event";
}


BEGIN_DECLARE_SIM_OBJECT_PARAMS(OptCPU)
  
    SimObjectParam<MemTraceReader *> data_trace; 
    Param<int> size;
    Param<int> block_size;
Param<int> assoc;

END_DECLARE_SIM_OBJECT_PARAMS(OptCPU)

BEGIN_INIT_SIM_OBJECT_PARAMS(OptCPU)

    INIT_PARAM_DFLT(data_trace, "memory trace", NULL),
    INIT_PARAM(size, "cache size"),
    INIT_PARAM(block_size, "block size"),
    INIT_PARAM(assoc,"associativity")
    
END_INIT_SIM_OBJECT_PARAMS(OptCPU)

CREATE_SIM_OBJECT(OptCPU)
{
    return new OptCPU(getInstanceName(),
		      data_trace,
		      block_size,
		      size,
		      assoc);
}

REGISTER_SIM_OBJECT("OptCPU", OptCPU)
