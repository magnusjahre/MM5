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

/**
 * @file
 * Simobject instatiation of caches.
 */
#include <vector>

// Must be included first to determine which caches we want
#include "mem/config/cache.hh"
#include "mem/config/compression.hh"

#include "mem/cache/base_cache.hh"
#include "mem/cache/cache.hh"
#include "mem/bus/bus.hh"
// #include "mem/crossbar/crossbar.hh" // Magnus
#include "mem/cache/coherence/directory.hh" // Magnus
#include "mem/cache/coherence/stenstrom.hh" // Magnus
#include "mem/cache/coherence/coherence_protocol.hh"
#include "sim/builder.hh"

// Tag Templates
#if defined(USE_CACHE_LRU)
#include "mem/cache/tags/lru.hh"
#endif

#if defined(USE_CACHE_FALRU)
#include "mem/cache/tags/fa_lru.hh"
#endif

#if defined(USE_CACHE_IIC)
#include "mem/cache/tags/iic.hh"
#endif

#if defined(USE_CACHE_SPLIT)
#include "mem/cache/tags/split.hh"
#endif

#if defined(USE_CACHE_SPLIT_LIFO)
#include "mem/cache/tags/split_lifo.hh"
#endif

// Compression Templates
#include "base/compression/null_compression.hh"
#if defined(USE_LZSS_COMPRESSION)
#include "base/compression/lzss_compression.hh"
#endif

// CacheTags Templates
#include "mem/cache/tags/cache_tags.hh"

// MissQueue Templates
#include "mem/cache/miss/miss_queue.hh"
#include "mem/cache/miss/blocking_buffer.hh"

// Coherence Templates
#include "mem/cache/coherence/uni_coherence.hh"
#include "mem/cache/coherence/simple_coherence.hh"

// Bus Interfaces
#include "mem/bus/slave_interface.hh"
#include "mem/bus/master_interface.hh"
#include "mem/memory_interface.hh"
        
// Crossbar interfaces
//#include "mem/crossbar/crossbar_master.hh"
//#include "mem/crossbar/crossbar_slave.hh"
        
// Interconnect includes
#include "mem/interconnect/interconnect.hh"
#include "mem/interconnect/interconnect_master.hh"
#include "mem/interconnect/interconnect_slave.hh"

#include "mem/trace/mem_trace_writer.hh"

//#include "mem/cache/prefetch/ghb_prefetcher.hh"
#include "mem/cache/prefetch/cdc_prefetcher.hh"
#include "mem/cache/prefetch/tagged_prefetcher.hh"
#include "mem/cache/prefetch/rpt_prefetcher.hh"
//#include "mem/cache/prefetch/stride_prefetcher.hh"


using namespace std;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(BaseCache)

    Param<int> size;
    Param<int> assoc;
    Param<int> block_size;
    Param<int> latency;
    Param<int> mshrs;
    Param<int> tgts_per_mshr;
    Param<int> write_buffers;
    Param<bool> prioritizeRequests;
    SimObjectParam<Bus *> in_bus;
    SimObjectParam<Bus *> out_bus;
    
    //SimObjectParam<Crossbar *> in_crossbar;  //Magnus
    //SimObjectParam<Crossbar *> out_crossbar; //Magnus
    
    SimObjectParam<Interconnect *> in_interconnect;
    SimObjectParam<Interconnect *> out_interconnect;
    Param<int> cpu_count;
    Param<int> memory_address_offset;
    Param<int> memory_address_parts;
    Param<int> cpu_id;
    Param<bool> multiprog_workload;
    
    // Directory coherence params
    Param<string> dirProtocolName;
    Param<bool> dirProtocolDoTrace;
    Param<Tick> dirProtocolTraceStart;
    Param<int> dirProtocolDumpInterval;
    
    Param<bool> is_shared;
    Param<bool> is_read_only;
    
    Param<bool> use_static_partitioning;
    Param<bool> use_mtp_partitioning;
    Param<Tick> mtp_epoch_size;
    Param<Tick> static_part_start_tick;
    Param<Tick> detailed_sim_start_tick;
    Param<Tick> detailed_sim_end_tick;
    Param<bool> use_static_partitioning_for_warmup;
    
    Param<bool> do_modulo_addr;
    Param<int> bank_id;
    Param<int> bank_count;
    Param<bool> simulate_contention;
    
    Param<bool> do_copy;
    SimObjectParam<CoherenceProtocol *> protocol;
    Param<Addr> trace_addr;
    Param<int> hash_delay;
#if defined(USE_CACHE_IIC)
    SimObjectParam<Repl *> repl;
#endif
    Param<bool> compressed_bus;
    Param<bool> store_compressed;
    Param<bool> adaptive_compression;
    Param<int> compression_latency;
    Param<int> subblock_size;
    Param<Counter> max_miss_count;
    SimObjectParam<HierParams *> hier;
    VectorParam<Range<Addr> > addr_range;
    SimObjectParam<MemTraceWriter *> mem_trace;
    Param<bool> split;
    Param<int> split_size;
    Param<bool> lifo;
    Param<bool> two_queue;
    Param<bool> prefetch_miss;
    Param<bool> prefetch_access;
    Param<int> prefetcher_size;
    Param<bool> prefetch_past_page;
    Param<bool> prefetch_serial_squash;
    Param<Tick> prefetch_latency;
    Param<int> prefetch_degree;
    Param<string> prefetch_policy;
    Param<bool> prefetch_cache_check_push;
    Param<bool> prefetch_use_cpu_id;
    Param<bool> prefetch_data_accesses_only;
    
    SimObjectParam<AdaptiveMHA *> adaptive_mha;

END_DECLARE_SIM_OBJECT_PARAMS(BaseCache)


BEGIN_INIT_SIM_OBJECT_PARAMS(BaseCache)

    INIT_PARAM(size, "capacity in bytes"),
    INIT_PARAM(assoc, "associativity"),
    INIT_PARAM(block_size, "block size in bytes"),
    INIT_PARAM(latency, "hit latency in CPU cycles"),
    INIT_PARAM(mshrs, "number of MSHRs (max outstanding requests)"),
    INIT_PARAM(tgts_per_mshr, "max number of accesses per MSHR"),
    INIT_PARAM_DFLT(write_buffers, "number of write buffers", 8),
    INIT_PARAM_DFLT(prioritizeRequests, "always service demand misses first",
		    false),
    
    INIT_PARAM_DFLT(in_bus, "incoming bus object", NULL),
    INIT_PARAM_DFLT(out_bus, "outgoing bus object", NULL), /* Magnus added DFLT and NULL */
    
    //INIT_PARAM_DFLT(in_crossbar, "incoming crossbar object", NULL),
    //INIT_PARAM_DFLT(out_crossbar, "outgoing crossbar object", NULL),
    
    INIT_PARAM_DFLT(in_interconnect, "incoming interconnect object", NULL),
    INIT_PARAM_DFLT(out_interconnect, "outgoing interconnect object", NULL),
    INIT_PARAM_DFLT(cpu_count, "the number of CPUs in the system", -1),
    INIT_PARAM_DFLT(memory_address_offset, "the index of this processors memory space" , -1),
    INIT_PARAM_DFLT(memory_address_parts, "the number address spaces to divide the memory into" , -1),
    INIT_PARAM_DFLT(cpu_id, "the ID of the processor that owns this cache", -1),
    INIT_PARAM_DFLT(multiprog_workload, "true if this is a multiprogrammed workload", false),
    
    INIT_PARAM_DFLT(dirProtocolName, "directory protocol to use", "none"),
    INIT_PARAM_DFLT(dirProtocolDoTrace, "should protocol actions be traced", false),
    INIT_PARAM_DFLT(dirProtocolTraceStart, "when should protocol tracing start", 0),
    INIT_PARAM_DFLT(dirProtocolDumpInterval, "how often should protocol stats be dumped?", 0),
    
    INIT_PARAM_DFLT(is_shared, "is this a shared cache?", false),
    INIT_PARAM_DFLT(is_read_only, "is this an instruction cache?", false),
    INIT_PARAM_DFLT(use_static_partitioning, "does this cache use static capacity partitioning?", false),
    INIT_PARAM_DFLT(use_mtp_partitioning, "does this cache use Multiple Time Sharing Partitions?", false),
    INIT_PARAM_DFLT(mtp_epoch_size, "the size of the MTP epoch", 10000000),
    INIT_PARAM_DFLT(static_part_start_tick, "the tick where cache part. enforcement will start", -1),
    INIT_PARAM_DFLT(detailed_sim_start_tick, "the tick where detailed simulation (and profiling) starts", -1),
    INIT_PARAM_DFLT(detailed_sim_end_tick, "the tick where detailed simulation ends", 0),
    INIT_PARAM_DFLT(use_static_partitioning_for_warmup, "if true, static partitioning is used in the warm up phase", false),
    
    INIT_PARAM_DFLT(do_modulo_addr, "use modulo operator to choose bank", false),
    INIT_PARAM_DFLT(bank_id, "the bank ID of this cache bank", -1),
    INIT_PARAM_DFLT(bank_count, "the number of cache banks", -1),
    INIT_PARAM_DFLT(simulate_contention, "true if this cache simulates contention", false),
    
    INIT_PARAM_DFLT(do_copy, "perform fast copies in the cache", false),
    INIT_PARAM_DFLT(protocol, "coherence protocol to use in the cache", NULL),
    INIT_PARAM_DFLT(trace_addr, "address to trace", 0),

    INIT_PARAM_DFLT(hash_delay, "time in cycles of hash access",1),
#if defined(USE_CACHE_IIC)
    INIT_PARAM_DFLT(repl, "replacement policy",NULL),
#endif
    INIT_PARAM_DFLT(compressed_bus,
                    "This cache connects to a compressed memory",
                    false),
    INIT_PARAM_DFLT(store_compressed, "Store compressed data in the cache",
		    false),
    INIT_PARAM_DFLT(adaptive_compression, "Use an adaptive compression scheme",
		    false),
    INIT_PARAM_DFLT(compression_latency,
		    "Latency in cycles of compression algorithm",
		    0),
    INIT_PARAM_DFLT(subblock_size,
		    "Size of subblock in IIC used for compression",
		    0),
    INIT_PARAM_DFLT(max_miss_count,
		    "The number of misses to handle before calling exit",
                    0),
    INIT_PARAM_DFLT(hier,
		    "Hierarchy global variables",
		    &defaultHierParams),
    INIT_PARAM_DFLT(addr_range, "The address range in bytes", 
		    vector<Range<Addr> >(1,RangeIn(0, MaxAddr))),
    INIT_PARAM_DFLT(mem_trace, "Memory trace to write accesses to", NULL),
    INIT_PARAM_DFLT(split, "Whether this is a partitioned cache", false),
    INIT_PARAM_DFLT(split_size, "the number of \"ways\" belonging to the LRU partition", 0),
    INIT_PARAM_DFLT(lifo, "whether you are using a LIFO repl. policy", false),
    INIT_PARAM_DFLT(two_queue, "whether the lifo should have two queue replacement", false),
    INIT_PARAM_DFLT(prefetch_miss, "wheter you are using the hardware prefetcher from Miss stream", false),
    INIT_PARAM_DFLT(prefetch_access, "wheter you are using the hardware prefetcher from Access stream", false),
    INIT_PARAM_DFLT(prefetcher_size, "Number of entries in the harware prefetch queue", 100),
    INIT_PARAM_DFLT(prefetch_past_page, "Allow prefetches to cross virtual page boundaries", false),
    INIT_PARAM_DFLT(prefetch_serial_squash, "Squash prefetches with a later time on a subsequent miss", false),
    INIT_PARAM_DFLT(prefetch_latency, "Latency of the prefetcher", 10),
    INIT_PARAM_DFLT(prefetch_degree, "Degree of the prefetch depth", 1),
    INIT_PARAM_DFLT(prefetch_policy, "Type of prefetcher to use", "none"),
    INIT_PARAM_DFLT(prefetch_cache_check_push, "Check if in cash on push or pop of prefetch queue", true),
    INIT_PARAM_DFLT(prefetch_use_cpu_id, "Use the CPU ID to seperate calculations of prefetches", true),
    INIT_PARAM_DFLT(prefetch_data_accesses_only, "Only prefetch on data not on instruction accesses", false),
    
    INIT_PARAM_DFLT(adaptive_mha, "Adaptive MHA object", NULL)
END_INIT_SIM_OBJECT_PARAMS(BaseCache)

#define BUILD_CACHE(t, comp, b, c) do {					\
        Prefetcher<CacheTags<t, comp>, b> *pf; \
        if (pf_policy == "tagged") {      \
           pf = new   \
                TaggedPrefetcher<CacheTags<t, comp>, b>(prefetcher_size, \
                                                        !prefetch_past_page, \
                                                        prefetch_serial_squash, \
                                                        prefetch_cache_check_push, \
                                                        prefetch_data_accesses_only, \
                                                        prefetch_degree,  \
                                                        prefetch_latency); \
        }            \
        else if (pf_policy == "cdc") {       \
           pf = new  \
                CDCPrefetcher<CacheTags<t, comp>, b>(prefetcher_size, \
                                                     !prefetch_past_page, \
                                                     prefetch_serial_squash, \
                                                     prefetch_cache_check_push, \
                                                     prefetch_data_accesses_only, \
                                                     prefetch_degree, \
                                                     prefetch_latency); \
	} \
        else { \
           pf = new  \
                RPTPrefetcher<CacheTags<t, comp>, b>(prefetcher_size, \
                                                        !prefetch_past_page, \
                                                        prefetch_serial_squash, \
                                                        prefetch_cache_check_push, \
                                                        prefetch_data_accesses_only, \
                                                        prefetch_degree, \
                                                        prefetch_latency); \
        } \
            \
        DirectoryProtocol<CacheTags<t, comp> > *directory_protocol = NULL;\
        if(dir_protocol_name == "stenstrom"){\
            directory_protocol = new StenstromProtocol<CacheTags<t, comp> >(name,\
                                                                            "stenstrom",\
                                                                            dirProtocolDoTrace,\
                                                                            dirProtocolDumpInterval,\
                                                                            dirProtocolTraceStart);\
        }\
        else{\
            directory_protocol = NULL;\
        }\
        \
        Cache<CacheTags<t, comp>, b, c>::Params params(tagStore, mq, coh, directory_protocol, \
						       do_copy, base_params, \
						       in_bus, out_bus, in_interconnect, out_interconnect, pf,  \
                                                       prefetch_access, cpu_count, cpu_id, \
                                                       multiprog_workload, is_shared, is_read_only,\
                                                       do_modulo_addr, bank_id,\
                                                       bank_count, adaptive_mha,\
                                                       use_static_partitioning, use_mtp_partitioning, static_part_start_tick,\
            detailed_sim_start_tick, mtp_epoch_size, simulate_contention, use_static_partitioning_for_warmup, detailed_sim_end_tick, memory_address_offset, memory_address_parts); \
        Cache<CacheTags<t, comp>, b, c> *retval =			\
	       new Cache<CacheTags<t, comp>, b, c>(getInstanceName(), hier, \
	       					   params);		\
        \
        if(directory_protocol != NULL) directory_protocol->setCache(retval);\
        \
        if(in_interconnect == NULL && out_interconnect == NULL){                                            \
            /* bus interconnect, use old code */\
            if (in_bus == NULL) {						\
	       retval->setSlaveInterface(new MemoryInterface<Cache<CacheTags<t, comp>, b, c> >(getInstanceName(), hier, retval, mem_trace)); \
	    } else {							\
	       retval->setSlaveInterface(new SlaveInterface<Cache<CacheTags<t, comp>, b, c>, Bus>(getInstanceName(), hier, retval, in_bus, mem_trace)); \
	    }								\
	   retval->setMasterInterface(new MasterInterface<Cache<CacheTags<t, comp>, b, c>, Bus>(getInstanceName(), hier, retval, out_bus)); \
	   out_bus->rangeChange();						\
        }                                                               \
        else if(in_interconnect != NULL && out_interconnect == NULL){ \
            /* this is the L2 cache */ \
            \
            /* creating crossbar interface */\
            retval->setSlaveInterface(new InterconnectSlave<Cache<CacheTags<t, comp>, b, c> >(getInstanceName(), in_interconnect, retval, hier));\
            /* creating memory bus interfaces */\
            retval->setMasterInterface(new MasterInterface<Cache<CacheTags<t, comp>, b, c>, Bus>(getInstanceName(), hier, retval, out_bus)); \
            out_bus->rangeChange();						\
            in_interconnect->rangeChange();\
            \
        }\
        else{\
            /* this is an L1 cache */\
            retval->setSlaveInterface(new MemoryInterface<Cache<CacheTags<t, comp>, b, c> >(getInstanceName(), hier, retval, mem_trace)); \
            retval->setMasterInterface(new InterconnectMaster<Cache<CacheTags<t, comp>, b, c> >(getInstanceName(), out_interconnect, retval, hier));\
            out_interconnect->rangeChange();\
        }\
        \
        return retval;							\
    } while (0)

#define BUILD_CACHE_PANIC(x) do {			\
	panic("%s not compiled into M5", x);		\
    } while (0)

#if defined(USE_LZSS_COMPRESSION)
#define BUILD_COMPRESSED_CACHE(TAGS, tags, b, c) do { \
	if (compressed_bus || store_compressed){			\
	    CacheTags<TAGS, LZSSCompression> *tagStore =		\
		new CacheTags<TAGS, LZSSCompression>(tags,		\
						     compression_latency, \
						     true, store_compressed, \
						     adaptive_compression,   \
                                                     prefetch_miss); \
	    BUILD_CACHE(TAGS, LZSSCompression, b, c);			\
	} else {							\
	    CacheTags<TAGS, NullCompression> *tagStore =		\
		new CacheTags<TAGS, NullCompression>(tags,		\
						     compression_latency, \
						     true, store_compressed, \
						     adaptive_compression,   \
                                                     prefetch_miss); \
	    BUILD_CACHE(TAGS, NullCompression, b, c);			\
	}								\
    } while (0)
#else
#define BUILD_COMPRESSED_CACHE(TAGS, tags, b, c) do { \
	if (compressed_bus || store_compressed){			\
	    BUILD_CACHE_PANIC("compressed caches");			\
	} else {							\
	    CacheTags<TAGS, NullCompression> *tagStore =		\
		new CacheTags<TAGS, NullCompression>(tags,		\
						      compression_latency, \
						      true, store_compressed, \
						      adaptive_compression    \
                                                      prefetch_miss); \
	    BUILD_CACHE(TAGS, NullCompression, b, c);			\
	}								\
    } while (0)
#endif

#if defined(USE_CACHE_FALRU)
#define BUILD_FALRU_CACHE(b,c) do {			    \
	FALRU *tags = new FALRU(block_size, size, latency); \
	BUILD_COMPRESSED_CACHE(FALRU, tags, b, c);		\
    } while (0)
#else
#define BUILD_FALRU_CACHE(b, c) BUILD_CACHE_PANIC("falru cache")
#endif

#if defined(USE_CACHE_LRU)
#define BUILD_LRU_CACHE(b, c) do {				\
        LRU *tags = new LRU(numSets, block_size, assoc, latency, bank_count, false);	\
	BUILD_COMPRESSED_CACHE(LRU, tags, b, c);			\
    } while (0)
#else
#define BUILD_LRU_CACHE(b, c) BUILD_CACHE_PANIC("lru cache")
#endif

#if defined(USE_CACHE_SPLIT)
#define BUILD_SPLIT_CACHE(b, c) do {					\
	Split *tags = new Split(numSets, block_size, assoc, split_size, lifo, \
				two_queue, latency);		\
	BUILD_COMPRESSED_CACHE(Split, tags, b, c);			\
    } while (0)
#else
#define BUILD_SPLIT_CACHE(b, c) BUILD_CACHE_PANIC("split cache")
#endif

#if defined(USE_CACHE_SPLIT_LIFO)
#define BUILD_SPLIT_LIFO_CACHE(b, c) do {				\
	SplitLIFO *tags = new SplitLIFO(block_size, size, assoc,        \
                                        latency, two_queue, -1);	\
	BUILD_COMPRESSED_CACHE(SplitLIFO, tags, b, c);			\
    } while (0)
#else
#define BUILD_SPLIT_LIFO_CACHE(b, c) BUILD_CACHE_PANIC("lifo cache")
#endif

#if defined(USE_CACHE_IIC)
#define BUILD_IIC_CACHE(b ,c) do {			\
	IIC *tags = new IIC(iic_params);		\
	BUILD_COMPRESSED_CACHE(IIC, tags, b, c);	\
    } while (0)
#else
#define BUILD_IIC_CACHE(b, c) BUILD_CACHE_PANIC("iic")
#endif

#define BUILD_CACHES(b, c) do {				\
	if (repl == NULL) {				\
	    if (numSets == 1) {				\
		BUILD_FALRU_CACHE(b, c);		\
	    } else {					\
		if (split == true) {			\
		    BUILD_SPLIT_CACHE(b, c);		\
		} else if (lifo == true) {		\
		    BUILD_SPLIT_LIFO_CACHE(b, c);	\
		} else {				\
		    BUILD_LRU_CACHE(b, c);		\
		}					\
	    }						\
	} else {					\
	    BUILD_IIC_CACHE(b, c);			\
	}						\
    } while (0)

#define BUILD_COHERENCE(b) do {						\
	if (protocol == NULL) {						\
	    UniCoherence *coh = new UniCoherence();			\
	    BUILD_CACHES(b, UniCoherence);				\
	} else {							\
	    SimpleCoherence *coh = new SimpleCoherence(protocol);	\
	    BUILD_CACHES(b, SimpleCoherence);				\
	}								\
    } while (0)

CREATE_SIM_OBJECT(BaseCache)
{
    
    string name = getInstanceName();
    int numSets = size / (assoc * block_size);
    
    string pf_policy = prefetch_policy;
    if (subblock_size == 0) {
	subblock_size = block_size;
    }
    
    string dir_protocol_name = dirProtocolName;
    if(dir_protocol_name != "none"){
        if(dir_protocol_name != "stenstrom"){
            fatal("Unknown directory protocol");
        }
    }

    // Build BaseCache param object
    BaseCache::Params base_params(addr_range, latency,
				  block_size, max_miss_count, cpu_count);

    //Warnings about prefetcher policy
    if (pf_policy == "none" && (prefetch_miss || prefetch_access)) {
	panic("With no prefetcher, you shouldn't prefetch from" 
	      " either miss or access stream\n");	
    }
    if ((pf_policy == "tagged" || pf_policy == "stride" || 
	 pf_policy == "ghb") && !(prefetch_miss || prefetch_access)) {
	warn("With this prefetcher you should chose a prefetch"  
	     " stream (miss or access)\nNo Prefetching will occur\n");
    }
    if ((pf_policy == "tagged" || pf_policy == "stride" || 
	 pf_policy == "ghb") && prefetch_miss && prefetch_access) {
	panic("Can't do prefetches from both miss and access"
	      " stream\n");
    }
    if (pf_policy != "tagged" && pf_policy != "rpt" &&
	pf_policy != "cdc"    && pf_policy != "none") {
	panic("Unrecognized form of a prefetcher: %s, try using"
	      "['none','rpt','tagged','cdc']\n", pf_policy);
    }

#if defined(USE_CACHE_IIC)    
    // Build IIC params
    IIC::Params iic_params;
    iic_params.size = size;
    iic_params.numSets = numSets;
    iic_params.blkSize = block_size;
    iic_params.assoc = assoc;
    iic_params.hashDelay = hash_delay;
    iic_params.hitLatency = latency;
    iic_params.rp = repl;
    iic_params.subblockSize = subblock_size;
#else
    const void *repl = NULL;
#endif

    /* FIXME: no events not supported */
    //if (mshrs == 1 || out_bus->doEvents() == false) {
	//BlockingBuffer *mq = new BlockingBuffer(true);
	//BUILD_COHERENCE(BlockingBuffer);
    //} else {
	MissQueue *mq = new MissQueue(mshrs, tgts_per_mshr, write_buffers,
				      true, prefetch_miss);
				      
	BUILD_COHERENCE(MissQueue);
    //}
    return NULL;
}

REGISTER_SIM_OBJECT("BaseCache", BaseCache)


#endif //DOXYGEN_SHOULD_SKIP_THIS
