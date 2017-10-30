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
 * Simobject instatiation of main memorys.
 */
#include "config/full_system.hh"
#include "mem/config/compression.hh"

#include "mem/functional/functional.hh"
#include "mem/timing/base_memory.hh"
#include "mem/timing/simple_mem_bank.hh"
#include "mem/bus/bus.hh"
#include "sim/builder.hh"

// Compression Templates
#include "base/compression/null_compression.hh"
#include "base/compression/lzss_compression.hh"

// Interfaces
#include "mem/memory_interface.hh"
#include "mem/bus/slave_interface.hh"

#include "mem/trace/mem_trace_writer.hh"

using namespace std;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(BaseMemory)

    SimObjectParam<Bus *> in_bus;
#if FULL_SYSTEM
    SimObjectParam<FunctionalMemory *> func_mem;
#endif
    Param<int> latency;
    Param<int> uncacheable_latency;
    Param<bool> snarf_updates;
    Param<bool> do_writes;
    VectorParam<Range<Addr> > addr_range;
    SimObjectParam<HierParams *> hier;
    Param<bool> compressed;
    SimObjectParam<MemTraceWriter *> mem_trace;
    /* Lagt til av oss */
    Param<int> bus_frequency;
    Param<int> num_banks;
    Param<int> RAS_latency;
    Param<int> CAS_latency;
    Param<int> precharge_latency;
    Param<int> min_activate_to_precharge_latency;
    Param<bool> static_memory_latency;

END_DECLARE_SIM_OBJECT_PARAMS(BaseMemory)

BEGIN_INIT_SIM_OBJECT_PARAMS(BaseMemory)

    INIT_PARAM(in_bus, "incoming bus object"),
#if FULL_SYSTEM
    INIT_PARAM(func_mem, "corresponding functional memory object"),
#endif
    INIT_PARAM(latency, "access latency"),
    INIT_PARAM_DFLT(uncacheable_latency, "uncacheable latency", 1000),
    INIT_PARAM_DFLT(snarf_updates, "update memory on cache-to-cache transfers",
		    true),
    INIT_PARAM_DFLT(do_writes, "update memory",false),
    INIT_PARAM_DFLT(addr_range, "Address range",
		    vector<Range<Addr> >(1, RangeIn((Addr)0, MaxAddr))),
    INIT_PARAM_DFLT(hier, "Hierarchy global variables", &defaultHierParams),
    INIT_PARAM_DFLT(compressed, "This memory stores compressed data.", false),
    INIT_PARAM_DFLT(mem_trace, "Memory trace to write accesses to", NULL),
	INIT_PARAM_DFLT(bus_frequency, "Bus frequency", 400),
	INIT_PARAM_DFLT(num_banks, "Number of banks", 8),
    INIT_PARAM_DFLT(RAS_latency, "RAS-to-CAS latency (bus cycles)", 4),
    INIT_PARAM_DFLT(CAS_latency, "CAS latency (bus cycles)", 4),
    INIT_PARAM_DFLT(precharge_latency, "precharge latency (bus cycles)", 4),
    INIT_PARAM_DFLT(min_activate_to_precharge_latency, "Minimum activate to precharge time (bus cycles)", 12),
    INIT_PARAM_DFLT(static_memory_latency, "Return the same latency for all data transfers", false)

END_INIT_SIM_OBJECT_PARAMS(BaseMemory)

CREATE_SIM_OBJECT(BaseMemory)
{
    BaseMemory::Params params;
    params.in = in_bus;
#if FULL_SYSTEM
    params.funcMem = func_mem;
#endif
    params.access_lat = latency;
    params.uncache_lat = uncacheable_latency;
    params.snarf_updates = snarf_updates;
    params.do_writes = do_writes;
    params.addrRange = addr_range;

    params.bus_frequency = bus_frequency;
    params.num_banks = num_banks;

    params.RAS_latency = RAS_latency;
    params.CAS_latency = CAS_latency;
    params.precharge_latency = precharge_latency;
    params.min_activate_to_precharge_latency = min_activate_to_precharge_latency;

    params.static_memory_latency = static_memory_latency;

    if (compressed) {
#if defined(USE_LZSS_COMPRESSION)
	SimpleMemBank<LZSSCompression> *retval =
	    new SimpleMemBank<LZSSCompression>(getInstanceName(), hier,
					       params);
	if (in_bus == NULL) {
	    retval->setSlaveInterface(new MemoryInterface<SimpleMemBank<LZSSCompression> >(getInstanceName(), hier, retval, mem_trace));
	} else {
	    retval->setSlaveInterface(new SlaveInterface<SimpleMemBank<LZSSCompression>, Bus>(getInstanceName(), hier, retval, in_bus, mem_trace));
	}
	return retval;
#else
	panic("compressed memory not compiled into M5");
#endif
    } else {
	SimpleMemBank<NullCompression> *retval =
	    new SimpleMemBank<NullCompression>(getInstanceName(), hier,
					       params);
	if (in_bus == NULL) {
	    retval->setSlaveInterface(new MemoryInterface<SimpleMemBank<NullCompression> >(getInstanceName(), hier, retval, mem_trace));
	} else {
	    retval->setSlaveInterface(new SlaveInterface<SimpleMemBank<NullCompression>, Bus>(getInstanceName(), hier, retval, in_bus, mem_trace));
	}
	return retval;
    }
}

REGISTER_SIM_OBJECT("BaseMemory", BaseMemory)

#endif // DOXYGEN_SHOULD_SKIP_THIS
