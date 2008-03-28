from m5 import *

###############################################################################
# CACHES
###############################################################################

class BaseL1Cache(BaseCache):
    in_bus = NULL
    size = '64kB'
    assoc = 8
    block_size = 64
    mshrs = 4
    tgts_per_mshr = 4
    cpu_count = int(env['NP'])
    is_shared = False

class IL1(BaseL1Cache):
    latency = Parent.clock.period
    is_read_only = True

class DL1(BaseL1Cache):
    latency = 3 * Parent.clock.period
    is_read_only = False

class L2Bank(BaseCache):
    size = '1MB' # 1MB * 4 banks = 4MB total cache size
    assoc = 8
    block_size = 64
    latency = 14 * Parent.clock.period
    mshrs = 8
    tgts_per_mshr = 4
    write_buffers = 16
    cpu_count = int(env['NP'])
    is_shared = True
    is_read_only = False
    
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
    width = 512
    clock = Parent.clock.period
    
class InterconnectBus(SplitTransBus):
    width = 512
    clock = 1 * Parent.clock.period
    transferDelay = 4
    arbitrationDelay = 5
    pipelined = False
    
class PipelinedBus(InterconnectBus):
    pipelined = True

class InterconnectIdeal(IdealInterconnect):
    width = 512 # the cache needs finite width
    clock = 1 * Parent.clock.period
    transferDelay = 0
    arbitrationDelay = 0
    
class InterconnectIdealWithDelay(IdealInterconnect):
    width = 512 # the cache needs finite width
    clock = 1 * Parent.clock.period
    transferDelay = 4
    arbitrationDelay = 5

class InterconnectCrossbar(Crossbar):
    width = 512
    clock = 1 * Parent.clock.period
    transferDelay = 4
    arbitrationDelay = 5
    
class InterconnectButterfly(Butterfly):
    width = 512
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
    #cpu_count = int(env['NP'])
    #bank_count = 4
    infinite_writeback = False
    readqueue_size = 16
    writequeue_size = 16
    prewritequeue_size = 0
    reserved_slots = 2
    trace_interval = 100000
    
#class NFQMemBus(NFQBus):
    #width = 8
    #clock = 4 * Parent.clock.period
    #cpu_count = int(env['NP'])
    #bank_count = 4
    
#class TimeMultiplexedMemBus(TimeMultiplexedBus):
    #width = 8
    #clock = 4 * Parent.clock.period
    #cpu_count = int(env['NP'])
    #bank_count = 4

class SDRAM(BaseMemory):
    #latency = 200 * Parent.clock.period
    latency = 112 * Parent.clock.period #ignored
    uncacheable_latency = 1000 * Parent.clock.period #ignored
