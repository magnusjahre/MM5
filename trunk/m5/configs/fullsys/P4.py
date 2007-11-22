from m5 import *
from Config import *
from Memory import *
from System import *
from FuncUnit import *

class P4BranchPred(BranchPred):
    pred_class='hybrid'
    local_hist_regs='2ki'
    local_hist_bits=11
    local_index_bits=11
    local_xor=False
    global_hist_bits=13
    global_index_bits=13
    global_xor=False
    choice_index_bits=13
    choice_xor=False
    ras_size=16
    btb_size='4ki'
    btb_assoc=4

class Pentium4(FullCPU):
    iq = StandardIQ(size=64, caps = [0, 0, 0, 0])
    iq_comm_latency = 1
    branch_pred=P4BranchPred()
    fupools=DefaultFUP()
    lsq_size=48
    rob_size=128
    width=4
    decode_to_dispatch=15
    mispred_recover=3
    fetch_branches=3
    ifq_size=32
    num_icache_ports=1
    storebuffer_size=24
    icache=Parent.icache
    dcache=Parent.dcache
    pc_sample_interval= 100 * Parent.clock.period

class IL1Cache(BaseCache):
    size='16kB'
    assoc=2
    block_size=64
    tgts_per_mshr=16
    latency=Parent.clock.period
    mshrs=4

class DL1Cache(BaseCache):
    size='8kB'
    assoc=4
    block_size=64
    tgts_per_mshr=16
    latency=2*Parent.clock.period
    mshrs=32

class L1DBus(Bus):
    width=64
    clock=2*Parent.clock.period

class L1IBus(Bus):
    width=8
    clock=2*Parent.clock.period

class CacheBridge(BusBridge):
    max_buffer=16
    latency='0ns'

class L2Bus(Bus):
    width=64
    clock=2*Parent.clock.period

class L2Cache(BaseCache):
    size='256kB'
    assoc=8
    block_size=64
    latency=19*Parent.clock.period
    mshrs=6
    tgts_per_mshr=16

class MainMem(BaseMemory):
    latency=40*Parent.clock.period
    addr_range=AddrRange('512MB')

def P4SimpleCache(Processor = Pentium4, **kwargs):
    self = Processor(**kwargs)
    self.icache = IL1Cache(in_bus = Null, out_bus = Parent.l2bus)
    self.dcache = DL1Cache(in_bus = Null, out_bus = Parent.l2bus)
    self.l2bus = L2Bus()
    self.l2 = L2Cache(in_bus = Parent.l2bus, out_bus = Parent.membus)
    return self

def P4FullCache(Processor = Pentium4, **kwargs):
    self = Processor(**kwargs)
    self.icache = IL1Cache(in_bus = Null, out_bus = Parent.ibus)
    self.dcache = DL1Cache(in_bus = Null, out_bus = Parent.dbus)
    self.ibus = L1IBus()
    self.dbus = L1DBus()
    self.ibridge = CacheBridge(in_bus=Parent.ibus, out_bus=Parent.l2bus)
    self.dbridge = CacheBridge(in_bus=Parent.dbus, out_bus=Parent.l2bus)
    self.l2bus = L2Bus()
    self.l2 = L2Cache(in_bus = Parent.l2bus, out_bus = Parent.membus)
    return self

def P4System(Processor, **kwargs):
    self = TsunamiSystem()
    self.cpu = Processor()
    return self

def P4Memory(System, **kwargs):
    self = MemoryBase(System, **kwargs)
    self.membus = Bus(width=8, clock='266.666666MHz')
    self.ram = MainMem(in_bus = Parent.membus)
    self.iobridge = BusBridge(max_buffer=16, latency='180ns',
                              in_bus=Parent.membus, out_bus=Parent.iobus)

    self.iobus = Bus(width=4, clock='66.666666MHz')

    self.pcibridge = BusBridge(max_buffer=16, latency='200ns',
                               in_bus=Parent.iobus, out_bus=Parent.pcibus)
    self.pcibus = Bus(width=4, clock='33.333333MHz')

    self.nibridge = BusBridge(max_buffer=16, latency='200ns',
                              in_bus=Parent.iobus, out_bus=Parent.nibus)
    self.nibus = Bus(width=4, clock='33.333333MHz')
    for i in xrange(len(self.tsunami.etherdev)):
        self.tsunami.etherdev[i].io_bus = Parent.nibus

    return self
