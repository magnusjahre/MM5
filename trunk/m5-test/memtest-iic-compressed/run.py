from m5 import *
class L1Cache(BaseCache):
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
    latency = 10 * Parent.clock.period
    mshrs = 32
    tgts_per_mshr = 16
    do_copy = True
    compressed_bus = True
    store_compressed = True
    repl = GenRepl(num_pools=16, fresh_res=31, pool_res=12)
    compression_latency = 10 * Parent.clock.period
    subblock_size = 16

class MainMem(BaseMemory):
    compressed = True
    latency = 100 * Parent.clock.period
    uncacheable_latency = 1000 * Parent.clock.period
    do_writes = True

root = Root()
root.toL2Bus = Bus(width=64, clock=2*Parent.clock.period)
root.toMemBus = Bus(width=16, clock=4*Parent.clock.period)
root.L1 = L1Cache(in_bus=NULL, out_bus=Parent.toL2Bus)
root.L2 = L2Cache(in_bus=Parent.toL2Bus, out_bus=Parent.toMemBus)
root.SDRAM = MainMem(in_bus=Parent.toMemBus)
root.mainMem = MainMemory()
root.checkMem = MainMemory()
root.hier = HierParams(do_data=True, do_events=True)
root.test = MemTest(cache=Parent.L1, main_mem=Parent.mainMem,
                    check_mem=Parent.checkMem, max_loads=5000000)

