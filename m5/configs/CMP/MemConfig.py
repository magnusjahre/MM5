from m5 import *

###############################################################################
# CACHES
###############################################################################

class BaseL1Cache(BaseCache):
    in_bus = NULL
    size = '64kB'
    assoc = 8
    block_size = 64
    
    #Eliminates blocking FIXME!!!
    #mshrs = 64
    #write_buffers = 64
    #tgts_per_mshr = 64 
    
    mshrs = 16
    write_buffers = 8
    #tgts_per_mshr = 4
    tgts_per_mshr = 8

    cpu_count = int(env['NP'])
    is_shared = False
    simulate_contention = False

class IL1(BaseL1Cache):
    latency = Parent.clock.period
    is_read_only = True

class DL1(BaseL1Cache):
    latency = 3 * Parent.clock.period
    is_read_only = False

class L2Bank(BaseCache):
    size = '1MB' #'2MB' # 1MB * 4 banks = 4MB total cache size
    assoc = 8 #16
    block_size = 64
    latency = 14 * Parent.clock.period
    
    #mshrs = 64
    #tgts_per_mshr = 64
    #write_buffers = 64
    
    mshrs = 16
    #tgts_per_mshr = 4
    tgts_per_mshr = 8
    write_buffers = 16
    
    cpu_count = int(env['NP'])
    is_shared = True
    is_read_only = False
    simulate_contention = True
    
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
    transferDelay = 4
    arbitrationDelay = 4 # was 5, changed 7.6.08
    
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


###############################################################################
# MEMORY AND MEMORY BUS
###############################################################################

class ConventionalMemBus(Bus):
    width = 8
    clock = 4 * Parent.clock.period
    cpu_count = int(env['NP'])
    bank_count = 4

class ReadyFirstMemoryController(RDFCFSMemoryController):
    #readqueue_size = 64
    #writequeue_size = 64
    
    readqueue_size = 16
    writequeue_size = 16
    
    reserved_slots = 2
    
class FastForwardMemoryController(RDFCFSMemoryController):
    readqueue_size = 64
    writequeue_size = 64
    reserved_slots = 2
    
class InOrderMemoryController(FCFSMemoryController):
    queue_size = 64
    
class FairNFQMemoryController(NFQMemoryController):
    #rd_queue_size = 64
    #wr_queue_size = 64
    rd_queue_size = 16
    wr_queue_size = 16
    starvation_prevention_thres = 0
    num_cpus = int(env["NP"])
    processor_priority = 1
    writeback_priority = 1
    
class ThroughputNFQMemoryController(NFQMemoryController):
    rd_queue_size = 64
    wr_queue_size = 64
    starvation_prevention_thres = 3
    num_cpus = int(env["NP"])
    processor_priority = 1
    writeback_priority = 1
    
class SDRAM(BaseMemory):
    num_banks = 8
    RAS_latency = 4
    CAS_latency = 4
    precharge_latency = 4
    min_activate_to_precharge_latency = 12
