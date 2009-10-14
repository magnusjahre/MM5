from m5 import *
from BaseMem import BaseMem

class Prefetch(Enum): vals = ['none', 'tagged', 'stride', 'ghb']

class DirectoryProtocol(Enum): vals = ['none', 'stenstrom']

class WritebackOwnerPolicy(Enum): vals = ['unknown', 'owner', 'replacer', 'shadow-tags']

class InterferenceProbabilityPolicy(Enum): vals = ['float', 'fixed', 'fixed-private']

class BaseCache(BaseMem):
    type = 'BaseCache'
    adaptive_compression = Param.Bool(False,
        "Use an adaptive compression scheme")
    assoc = Param.Int("associativity")
    block_size = Param.Int("block size in bytes")
    compressed_bus = Param.Bool(False,
        "This cache connects to a compressed memory")
    compression_latency = Param.Latency('0ns',
        "Latency in cycles of compression algorithm")
    do_copy = Param.Bool(False, "perform fast copies in the cache")
    hash_delay = Param.Int(1, "time in cycles of hash access")
    in_bus = Param.Bus(NULL, "incoming bus object")
    lifo = Param.Bool(False, 
	"whether this NIC partition should use LIFO repl. policy")
    max_miss_count = Param.Counter(0,
        "number of misses to handle before calling exit")
    mem_trace = Param.MemTraceWriter(NULL,
                                     "memory trace writer to record accesses")
    mshrs = Param.Int("number of MSHRs (max outstanding requests)")
    out_bus = Param.Bus("outgoing bus object")
    prioritizeRequests = Param.Bool(False,
        "always service demand misses first")
    protocol = Param.CoherenceProtocol(NULL, "coherence protocol to use")
    repl = Param.Repl(NULL, "replacement policy")
    size = Param.MemorySize("capacity in bytes")
    split = Param.Bool(False, "whether or not this cache is split")
    split_size = Param.Int(0, 
	"How many ways of the cache belong to CPU/LRU partition")
    store_compressed = Param.Bool(False,
        "Store compressed data in the cache")
    subblock_size = Param.Int(0,
        "Size of subblock in IIC used for compression")
    tgts_per_mshr = Param.Int("max number of accesses per MSHR")
    trace_addr = Param.Addr(0, "address to trace")
    two_queue = Param.Bool(False, 
	"whether the lifo should have two queue replacement")
    write_buffers = Param.Int(8, "number of write buffers")
    prefetch_miss = Param.Bool(False,
         "wheter you are using the hardware prefetcher from Miss stream")
    prefetch_access = Param.Bool(False,
         "wheter you are using the hardware prefetcher from Access stream")
    prefetcher_size = Param.Int(100, 
         "Number of entries in the harware prefetch queue")
    prefetch_past_page = Param.Bool(False,
         "Allow prefetches to cross virtual page boundaries")
    prefetch_serial_squash = Param.Bool(False,
         "Squash prefetches with a later time on a subsequent miss")
    prefetch_degree = Param.Int(1,
         "Degree of the prefetch depth")
    prefetch_latency = Param.Tick(10,
         "Latency of the prefetcher")
    prefetch_policy = Param.Prefetch('none',
         "Type of prefetcher to use")
    prefetch_cache_check_push = Param.Bool(True,
         "Check if in cash on push or pop of prefetch queue")
    prefetch_use_cpu_id = Param.Bool(True,
         "Use the CPU ID to seperate calculations of prefetches")
    prefetch_data_accesses_only = Param.Bool(False,
         "Only prefetch on data not on instruction accesses")
    
    #in_crossbar = Param.Crossbar(NULL, "incoming crossbar") # Magnus
    #out_crossbar = Param.Crossbar(NULL, "outgoing crossbar") # Magnus
    
    in_interconnect = Param.Interconnect(NULL, "incoming interconnect") # Magnus
    out_interconnect = Param.Interconnect(NULL, "outgoing interconnect") # Magnus
    cpu_count = Param.Int("The number of cpus in the system") # Magnus
    cpu_id = Param.Int("The processor id of the owner CPU (only set this for L1 data caches)") # Magnus
    multiprog_workload = Param.Bool("True if this is a multiprogram workload") # Magnus
    memory_address_offset = Param.Int("the index of this processors memory space")
    memory_address_parts = Param.Int("the number address spaces to divide the memory into")
    
    #directory_protocol = Param.DirectoryProtocol(NULL, "Directory protocol for this cache") # Magnus
    is_shared = Param.Bool("True if this cache is shared by more than one core") # Magnus
    is_read_only = Param.Bool("True if this cache is an instruction cache") # Magnus
    
    use_static_partitioning = Param.Bool("True if this cache uses static uniform capacity partitioning") # Magnus
    use_mtp_partitioning = Param.Bool("True if this cache uses MTP partitioning") # Magnus
    mtp_epoch_size = Param.Tick("the size of the MTP epoch")
    static_part_start_tick = Param.Tick("The clock cycle to start enforcing a static cache share")
    detailed_sim_start_tick = Param.Tick("The tick where detailed simulation (and profiling) starts")
    use_static_partitioning_for_warmup = Param.Bool("if true, static partitioning is used in the warm up phase")
    static_partitioning_div_factor = Param.Int("factor to divide cache space during fw by when there is 1 cpu core")

    shadow_tag_leader_sets = Param.Int("number of leader sets to use in shadow tags (0 is full-map")

    # Directory protocol parameters
    dirProtocolName = Param.DirectoryProtocol('none', "name of coherence protocol")
    dirProtocolDoTrace = Param.Bool("turns on tracing of coherence protocol actions")
    dirProtocolTraceStart = Param.Tick("when should protocol tracing start")
    dirProtocolDumpInterval = Param.Int("number of clock cycles between each statsdump (0 turns it off)");
    
    # Modulo bank selection
    do_modulo_addr = Param.Bool("Use modulo operator to select bank")
    bank_id = Param.Int("The bank ID of this cache (only for modulo addressed banks)")
    bank_count = Param.Int("The number of cache banks (only for modulo addressed banks)")
    simulate_contention = Param.Bool("true if this cache simulates contention")
    
    adaptive_mha = Param.AdaptiveMHA("Adaptive MHA Object")
    interference_manager = Param.InterferenceManager("Interference Manager Object")
    
    writeback_owner_policy = Param.WritebackOwnerPolicy("The policy used for providing sender IDs to shared cache writebacks")
    interference_probability_policy = Param.InterferenceProbabilityPolicy("interference probability policy to use")
    ipp_bits = Param.Int("The resolution of the probability (used in a subset of IPP modes)")
