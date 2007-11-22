from m5 import *
class BaseL1Cache(BaseCache):
    in_bus = NULL
    size = '64kB'
    assoc = 2
    block_size = 64
    tgts_per_mshr = 16

class IL1(BaseL1Cache):
    latency = Parent.clock.period
    mshrs = 8

class DL1(BaseL1Cache):
    latency = 3 * Parent.clock.period
    mshrs = 32
    repl = GenRepl(num_pools=16,fresh_res=30,pool_res=12)

class ToL2Bus(Bus):
    width = 64
    clock = Parent.clock.period

class L2(BaseCache):
    size = '2MB'
    assoc = '32ki'
    block_size = 64
    latency = 10 * Parent.clock.period
    mshrs = 92
    tgts_per_mshr = 16

class ToMemBus(Bus):
    width = 16
    clock = 1 * Parent.clock.period

class SDRAM(BaseMemory):
    latency = 100 * Parent.clock.period
    uncacheable_latency = 1000 * Parent.clock.period
