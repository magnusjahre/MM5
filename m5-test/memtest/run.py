from m5 import *
class DL1Cache(BaseCache):
    size = '64kB'
    assoc = 2
    block_size = 64
    tgts_per_mshr = 16
    latency = 3 * Parent.clock.period
    mshrs = 32

class L2Cache(BaseCache):
    size = '64kB'
    assoc = 4
    block_size = 64
    tgts_per_mshr = 16
    latency = 10 * Parent.clock.period
    mshrs = 32
    do_copy = True

root = Root()
root.hier = HierParams(do_data=True, do_events=True)
root.toL2Bus = Bus(width=64, clock=2*Parent.clock.period)
root.toMemBus = Bus(width=16, clock=4*Parent.clock.period)
root.toMemBusSlow = Bus(width=16, clock=8*Parent.clock.period)
root.DL1 = DL1Cache(in_bus=NULL, out_bus=Parent.toL2Bus)
root.L2 = L2Cache(in_bus=Parent.toL2Bus, out_bus=Parent.toMemBus)
root.busBridge = BusBridge(in_bus=Parent.toMemBus, out_bus=Parent.toMemBusSlow,
                           max_buffer=16)
root.SDRAM = BaseMemory(in_bus=Parent.toMemBusSlow,
                        latency=100*Parent.clock.period,
                        uncacheable_latency=1000*Parent.clock.period,
                        do_writes=True)
root.mainMem = MainMemory()
root.checkMem = MainMemory()
root.test = MemTest(cache=Parent.DL1, main_mem=Parent.mainMem,
                    check_mem=Parent.checkMem, max_loads=5000000)
