from m5 import *

###############################################################################
# CACHES
###############################################################################

mshrParamName="BASEMSHRS"

class BaseL1Cache(BaseCache):
    in_bus = NULL
    size = '64kB'
    assoc = 2
    block_size = 64
    write_buffers = 4
    
    #if mshrParamName in env:
    #    mshrs = int(env[mshrParamName])
    #else:
    #    mshrs = 16
    
    mshrs = 16
    
    cpu_count = int(env['NP'])
    is_shared = False
    simulate_contention = False
    
    if int(env['NP']) == 2:
        latency = 3 * Parent.clock.period
    elif int(env['NP']) == 4:
        latency = 3 * Parent.clock.period
    elif int(env['NP']) == 8:
        latency = 2 * Parent.clock.period
    elif int(env['NP']) == 16:
        latency = 2 * Parent.clock.period
    elif int(env['NP']) == 1:
        assert 'MEMORY-ADDRESS-PARTS' in env
        if int(env['MEMORY-ADDRESS-PARTS']) == 2:
            latency = 3 * Parent.clock.period
        elif int(env['MEMORY-ADDRESS-PARTS']) == 4:
            latency = 3 * Parent.clock.period
        elif int(env['MEMORY-ADDRESS-PARTS']) == 8:
            latency = 2 * Parent.clock.period
        elif int(env['MEMORY-ADDRESS-PARTS']) == 16:
            latency = 2 * Parent.clock.period
        else:
            panic("L1 cache: unknown latency for single")
    else:
        panic("L1 cache: unknown latency for cpu count")

class IL1(BaseL1Cache):
    mshrs = 16 # do not change instruction MSHRs
    is_read_only = True
    tgts_per_mshr = 4

class DL1(BaseL1Cache):
    is_read_only = False
    
    tgts_per_mshr = 32
    if "L1-CACHE-TARGETS" in env:
        tgts_per_mshr = int(env["L1-CACHE-TARGETS"])

class L2Bank(BaseCache):
    size = '2MB' # 1MB * 4 banks = 4MB total cache size
    assoc = 16
    block_size = 64
    latency = 18 * Parent.clock.period
    
    #mshrs = 1024
    #tgts_per_mshr = 1024
    #write_buffers = 1024
    
    mshrs = 16
    tgts_per_mshr = 4
    #tgts_per_mshr = 8
    write_buffers = 16
    
    cpu_count = int(env['NP'])
    is_shared = True
    is_read_only = False
    simulate_contention = False # done in new crossbar impl
    
    if int(env['NP']) == 1:
        static_partitioning_div_factor = int(env['MEMORY-ADDRESS-PARTS'])
    
    def setModuloAddr(self, bankID, bank_count):
        self.do_modulo_addr = True
        self.bank_id = bankID
        self.bank_count = bank_count
    
    def setAddrRange(self, bankID, bank_count):
        offset = MaxAddr / bank_count
        if bankID == 0:
            self.addr_range = AddrRange(0, offset)
        elif bankID == (bank_count-1):
            self.addr_range = AddrRange((bankID*offset)+1, MaxAddr)
        else:
            self.addr_range = AddrRange((bankID*offset)+1, ((bankID+1)*offset))

class CommonLargeCache(BaseCache):
    block_size = 64
    cpu_count = int(env['NP'])

    is_read_only = False
    simulate_contention = False # done in new crossbar impl

class PrivateCache1M(CommonLargeCache):
    size = '1MB'
    assoc = 4
    is_shared = False
    
    if mshrParamName in env:
        mshrs = int(env[mshrParamName])
    else:
        mshrs = 16
    
    tgts_per_mshr = 4
    write_buffers = 16
    
    if int(env['NP']) == 2:
        latency = 9 * Parent.clock.period
    elif int(env['NP']) == 4:
        latency = 9 * Parent.clock.period
    elif int(env['NP']) == 8:
        latency = 6 * Parent.clock.period
    elif int(env['NP']) == 16:
        latency = 5 * Parent.clock.period
    elif int(env['NP']) == 1:
        assert 'MEMORY-ADDRESS-PARTS' in env
        if int(env['MEMORY-ADDRESS-PARTS']) == 2:
            latency = 9 * Parent.clock.period
        elif int(env['MEMORY-ADDRESS-PARTS']) == 4:
            latency = 9 * Parent.clock.period
        elif int(env['MEMORY-ADDRESS-PARTS']) == 8:
            latency = 6 * Parent.clock.period
        elif int(env['MEMORY-ADDRESS-PARTS']) == 16:
            latency = 5 * Parent.clock.period
        else:
            panic("Priv 1M cache: Unknown latency for single")
    else:
        panic("Priv 1M cache: unknown latency for cpu count")
    

def getBufferMultFac():
    multfac = int(env["NP"])
    if int(env['NP']) == 1:
        multfac = int(env['MEMORY-ADDRESS-PARTS'])
    return multfac

class SharedCache8M(CommonLargeCache):
    size = '2MB' # 4 banks
    assoc = 16
    
    latency = 16 * Parent.clock.period
    is_shared = True
    
    if mshrParamName in env:
        mshrs = int(env[mshrParamName]) * getBufferMultFac()
        if mshrs < 16:
            mshrs = 16 * getBufferMultFac()
    else:
        mshrs = 16 * getBufferMultFac()
    
    tgts_per_mshr = 4
    write_buffers = 16 * getBufferMultFac()
    
    if int(env['NP']) == 1:
        static_partitioning_div_factor = int(env['MEMORY-ADDRESS-PARTS'])


class SharedCache16M(CommonLargeCache):
    size = '4MB' # 4 banks
    assoc = 16
    latency = 12 * Parent.clock.period
    is_shared = True
    
    if mshrParamName in env:
        mshrs = int(env[mshrParamName])*getBufferMultFac() 
        if mshrs < 16:
            mshrs = 16*getBufferMultFac()
    else:
        mshrs = 16*getBufferMultFac()
    
    tgts_per_mshr = 4
    write_buffers = 16*getBufferMultFac()

    if int(env['NP']) == 1:
        static_partitioning_div_factor = int(env['MEMORY-ADDRESS-PARTS'])


class SharedCache32M(CommonLargeCache):
    size = '8MB' # 4 banks
    assoc = 16
    latency = 12 * Parent.clock.period
    is_shared = True
    
    if mshrParamName in env:
        mshrs = int(env[mshrParamName])*getBufferMultFac()
        if mshrs < 16:
            mshrs = 16*getBufferMultFac()
    else:
        mshrs = 16*getBufferMultFac()
    
    tgts_per_mshr = 4
    write_buffers = 16*getBufferMultFac()
    
    if int(env['NP']) == 1:
        static_partitioning_div_factor = int(env['MEMORY-ADDRESS-PARTS'])

     


###############################################################################
# INTERCONNECT
###############################################################################

class ToL2Bus(Bus):
    width = 64
    clock = Parent.clock.period
    
class InterconnectBus(SplitTransBus):
    width = 64
    clock = 1 * Parent.clock.period
    transferDelay = 4
    arbitrationDelay = 5
    pipelined = False
    
class PipelinedBus(InterconnectBus):
    pipelined = True

class InterconnectIdeal(IdealInterconnect):
    width = 64 # the cache needs finite width
    clock = 1 * Parent.clock.period
    transferDelay = 0
    arbitrationDelay = 0
    
class InterconnectIdealWithDelay(IdealInterconnect):
    width = 64 # the cache needs finite width
    clock = 1 * Parent.clock.period
    transferDelay = 4
    arbitrationDelay = 5

class InterconnectCrossbar(Crossbar):
    width = 64
    clock = 1 * Parent.clock.period
    arbitrationDelay = 0 
    
    if int(env['NP']) == 2:
        transferDelay = 8
        pipe_stages = 2
    elif int(env['NP']) == 4:
        transferDelay = 8
        pipe_stages = 2
    elif int(env['NP']) == 8:
        transferDelay = 16
        pipe_stages = 4
    elif int(env['NP']) == 16:
        transferDelay = 30
        pipe_stages = 6
    elif int(env['NP']) == 1:
        assert 'MEMORY-ADDRESS-PARTS' in env
        if int(env['MEMORY-ADDRESS-PARTS']) == 2:
            transferDelay = 8
            pipe_stages = 2
        elif int(env['MEMORY-ADDRESS-PARTS']) == 4:
            transferDelay = 8
            pipe_stages = 2
        elif int(env['MEMORY-ADDRESS-PARTS']) == 8:
            transferDelay = 16
            pipe_stages = 4
        elif int(env['MEMORY-ADDRESS-PARTS']) == 16:
            transferDelay = 30
            pipe_stages = 6
        else:
            panic("Crossbar: unknown latency for single")
    else:
        panic("Crossbar: unknown latency for cpu count")
    
class InterconnectButterfly(Butterfly):
    width = 64
    clock = 1 * Parent.clock.period
    radix = 2
    banks = 4
    
    if int(env['NP']) == 2:
        # total delay is 10 clock cycles (2*2+3*2)
        transferDelay = 2 # per link transfer delay
        arbitrationDelay = 0 # arb in switches, no explicit delay
        switch_delay = 2
    elif int(env['NP']) == 4:
        # total delay is 10 clock cycles (2*3+1*4)
        transferDelay = 1 # per link transfer delay
        arbitrationDelay = 0 # arb in switches, no explicit delay
        switch_delay = 2 
    else:
        # total delay is 9 clock cycles (1*4+1*5)
        transferDelay = 1 # per link transfer delay
        arbitrationDelay = 0 # arb in switches, no explicit delay
        switch_delay = 1

class PointToPointLink(PeerToPeerLink):
    width = 64
    clock = 1 * Parent.clock.period
    arbitrationDelay = -1 # not used
    cpu_count = env['NP']
    
    if int(env['NP']) == 2:
        transferDelay = 4
    elif int(env['NP']) == 4:
        transferDelay = 4
    elif int(env['NP']) == 8:
        transferDelay = 3
    elif int(env['NP']) == 16:
        transferDelay = 2
    elif int(env['NP']) == 1:
        assert 'MEMORY-ADDRESS-PARTS' in env
        if int(env['MEMORY-ADDRESS-PARTS']) == 2:
            transferDelay = 4
        elif int(env['MEMORY-ADDRESS-PARTS']) == 4:
            transferDelay = 4
        elif int(env['MEMORY-ADDRESS-PARTS']) == 8:
            transferDelay = 3
        elif int(env['MEMORY-ADDRESS-PARTS']) == 16:
            transferDelay = 2
        else:
            panic("P2P: unknown latency for single")
    else:
        panic("P2P: unknown latency for cpu count")
    
class RingInterconnect(Ring):
    width = 64
    clock = 1 * Parent.clock.period
    if int(env['NP']) == 16:
        transferDelay = 8
        arbitrationDelay = 4
    elif int(env['NP']) == 8 or int(env['NP']) == 4 or int(env['NP']) == 2:
        transferDelay = 4
        arbitrationDelay = 4
    elif int(env['NP']) == 1:
        assert 'MEMORY-ADDRESS-PARTS' in env
        if int(env['MEMORY-ADDRESS-PARTS']) == 16:
            transferDelay = 8
            arbitrationDelay = 4
        elif int(env['MEMORY-ADDRESS-PARTS']) == 8 or int(env['MEMORY-ADDRESS-PARTS']) == 4 or int(env['MEMORY-ADDRESS-PARTS']) == 2:
            transferDelay = 4
            arbitrationDelay = 4
        else:
            panic("Ring: unknown latency for single")
            
        assert 'MEMORY-ADDRESS-OFFSET' in env
        single_proc_id = int(env['MEMORY-ADDRESS-OFFSET'])
    else:
        fatal("Ring: unknown latency for cpu count")
        
    if int(env['NP']) == 1:
        cpu_count = int(env['MEMORY-ADDRESS-PARTS'])
    else:
        cpu_count = env['NP']

###############################################################################
# MEMORY AND MEMORY BUS
###############################################################################

class ConventionalMemBus(Bus):
    width = 8
    clock = 4 * Parent.clock.period
    cpu_count = int(env['NP'])
    bank_count = 4

class ReadyFirstMemoryController(RDFCFSMemoryController):
    readqueue_size = 64
    writequeue_size = 64
    inf_write_bw = False
    page_policy = "ClosedPage"
    priority_scheme = "FCFS"
    
#class FastForwardMemoryController(RDFCFSMemoryController):
    #readqueue_size = 64
    #writequeue_size = 64
    #reserved_slots = 2
    
class InOrderMemoryController(FCFSMemoryController):
    queue_size = 128
    
class FixedBandwidthController(FixedBandwidthMemoryController):
    queue_size = 128
    cpu_count = int(env['NP'])
    
class FairNFQMemoryController(NFQMemoryController):
    rd_queue_size = 64
    wr_queue_size = 64
    starvation_prevention_thres = 0
    
class ThroughputNFQMemoryController(NFQMemoryController):
    rd_queue_size = 64
    wr_queue_size = 64
    starvation_prevention_thres = 3
    
class DDR2(BaseMemory):
    bus_frequency = 400
    num_banks = 8
    max_active_bank_cnt = 4
    
    RAS_latency = 4
    CAS_latency = 4
    precharge_latency = 4
    min_activate_to_precharge_latency = 12
    
    write_recovery_time = 6
    internal_write_to_read = 3
    pagesize = 10 # 1kB = 2**10 from standard
    internal_read_to_precharge = 1
    data_time = 4 # BL=8, double data rate
    read_to_write_turnaround = 6
    internal_row_to_row = 3
    
    if "STATIC-MEMORY-LATENCY" in env:
        static_memory_latency = True
    else:
        static_memory_latency = False
        
class DDR4(BaseMemory):
    bus_frequency = 1333
    num_banks = 16
    max_active_bank_cnt = 16
    
    RAS_latency = 18
    CAS_latency = 18
    precharge_latency = 18
    min_activate_to_precharge_latency = 43
    
    write_recovery_time = 20
    internal_write_to_read = 7 
    pagesize = 10 # 1kB = 2**10 is OK for 8GB and 16GB configurations with 
    internal_read_to_precharge = 10
    data_time = 4 # BL=8, double data rate
    read_to_write_turnaround = 10
    internal_row_to_row = 61
    
    if "STATIC-MEMORY-LATENCY" in env:
        static_memory_latency = True
    else:
        static_memory_latency = False
