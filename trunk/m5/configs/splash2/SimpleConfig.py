from m5 import *

class L1Cache(BaseCache):
    size = '64kB'
    assoc = 2
    block_size = 64
    latency = Parent.clock.period
    mshrs = 4
    tgts_per_mshr = 8
    protocol = CoherenceProtocol(protocol='msi')

class CPU(SimpleCPU):
    icache = L1Cache(out_bus=Parent.toMembus)
    dcache = L1Cache(out_bus=Parent.toMembus)

class SimpleStandAlone(Root):
    cpu = CPU()
    hier = HierParams(do_data=False, do_events=False)
    toMembus = Bus(width=16, clock=1*Parent.clock.period)
    dram = BaseMemory(in_bus=Parent.toMembus,
                      latency=100*Parent.clock.period,
                      uncacheable_latency=1000*Parent.clock.period)
