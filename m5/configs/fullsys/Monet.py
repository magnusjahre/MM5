from m5 import *
from Config import *
from Memory import *
from System import *

class MonetBranchPred(BranchPred):
    pred_class='hybrid'
    local_hist_regs='1ki'
    local_hist_bits=10
    local_index_bits=9
    local_xor=False
    local_hist_regs='4ki'
    global_hist_bits=12
    global_index_bits=12
    global_xor=False
    choice_index_bits=12
    choice_xor=False
    ras_size=16 # I don't know if this is correct  
    btb_size=128
    btb_assoc=128

class MonetU0(FUDesc):
    opList = [ OpDesc(opClass='IntAlu'),
               OpDesc(opClass='IprAccess', opLat = int(env.get('IPRDELAY',3)), 
                       issueLat = int(env.get('IPRDELAY',3))) ]
    count = 1

class MonetU1(FUDesc):
    opList = [ OpDesc(opClass='IntAlu'), 
               OpDesc(opClass='IntMult', opLat=7)]
    count = 1

class MonetL(FUDesc):
    opList = [ OpDesc(opClass='MemRead'), 
               OpDesc(opClass='MemWrite'),
               OpDesc(opClass='IntAlu') ]
    count = 2

class MonetFP_ALU(FUDesc):
    opList = [ OpDesc(opClass='FloatAdd', opLat=4),
               OpDesc(opClass='FloatCmp', opLat=4),
               OpDesc(opClass='FloatCvt', opLat=4),
               OpDesc(opClass='FloatDiv', opLat=15, issueLat=12),
               OpDesc(opClass='FloatSqrt', opLat=33, issueLat=30)]
    count = 1

class MonetFP_Mult(FUDesc):
    opList = [ OpDesc(opClass='FloatMult', opLat=4)]
    count = 1
   
class MonetFUP(FuncUnitPool):
    FUList = [ MonetU0(), MonetU1(), MonetL(), MonetFP_Mult(), MonetFP_ALU() ]

class Monet(FullCPU):
    iq = StandardIQ(size=64, caps = [0, 0, 0, 0])
    iq_comm_latency = 1
    branch_pred=MonetBranchPred()
    fupools=MonetFUP()
    lsq_size=64 # really 32 load 32 store
    rob_size=80
    width=4
    decode_to_dispatch=2 # this is a complete guess
    mispred_recover=3
    fetch_branches=3
    ifq_size=32
    num_icache_ports=1
    storebuffer_size=16
    icache=Parent.icache
    dcache=Parent.dcache
    # dtb.size=128
    # itb.size=128
    pc_sample_interval=100*Parent.clock.period
    if env['FREQUENCY'] == '500MHz':
        fault_handler_delay=45
    elif env['FREQUENCY'] == '666666667Hz':
        fault_handler_delay=38
    if env['TLB'] == 'fasttlb':
        fault_handler_delay=0

class MonetIL1Cache(BaseCache):
    size='64kB'
    assoc=2
    block_size=64
    tgts_per_mshr=16
    if env['FREQUENCY'] == '500MHz':
        latency='2ns'
    elif env['FREQUENCY'] == '666666667Hz':
        latency='1.5ns'
    mshrs=8

class MonetDL1Cache(BaseCache):
    size='64kB'
    assoc=2
    block_size=64
    tgts_per_mshr=16
    if env['FREQUENCY'] == '500MHz':
        latency='4ns'
    elif env['FREQUENCY'] == '666666667Hz':
        latency='3ns'
    mshrs=8

class MonetL1DBus(Bus):
    width=16 
    clock=1.5*Parent.clock

class MonetL1IBus(Bus):
    width=16
    clock=1.5*Parent.clock

class MonetCacheBridge(BusBridge):
    max_buffer=16
    latency='16ns'

class MonetL2Bus(Bus):
    width=16
    if env['FREQUENCY'] == '500MHz':
        clock='333MHz'
    elif env['FREQUENCY'] == '666666667Hz':
        clock='445MHz'

class MonetL2Cache(BaseCache):
    size='4MB'
    assoc=1
    block_size=64
    if env['FREQUENCY'] == '500MHz':
        latency='12ns'
    elif env['FREQUENCY'] == '666666667Hz':
        latency='9ns'
    mshrs=6
    tgts_per_mshr=16

class MainMem(BaseMemory):
    latency='11ns'
    addr_range=AddrRange('512MB')

def MonetSimpleCache(CPUType, **kwargs):
    self = CPUType(**kwargs)
    self.icache = MonetIL1Cache(in_bus = NULL, out_bus = Parent.l2bus)
    self.dcache = MonetDL1Cache(in_bus = NULL, out_bus = Parent.l2bus)
    self.l2bus = MonetL2Bus()
    self.l2 = MonetL2Cache(in_bus = Parent.l2bus, out_bus = Parent.sysbus)
    return self

def MonetSystem(Processor, **kwargs):
    self = TsunamiSystem()
    self.cpu = Processor()

def MonetMemory(System, **kwargs):
    self = MemoryBase(System, **kwargs)
    self.membus = Bus(width=32, clock='83MHz')
    if env['FREQUENCY'] == '500MHz':
        self.sysbus = Bus(width=8, clock='333MHz')
        if env['EARLYACK'] == 'no':
            self.membridge = BusBridge(max_buffer=16,
                              latency='55ns',
                              in_bus=Parent.sysbus, out_bus=Parent.membus)
            self.iobridge = BusBridge(max_buffer=16,
                              latency='150ns',
                              in_bus=Parent.sysbus, out_bus=Parent.iobus)
        else:
            self.membridge = BusBridge(max_buffer=16,
                              latency='55ns',
                              in_bus=Parent.sysbus, out_bus=Parent.membus,
                              ack_writes=True,
                              ack_delay='55ns')
            self.iobridge = BusBridge(max_buffer=16,
                              latency='150ns',
                              in_bus=Parent.sysbus, out_bus=Parent.iobus,
                              ack_writes=True,
                              ack_delay='115ns')
        self.tsunami.cchip.pio_latency = 18
    elif env['FREQUENCY'] == '666666667Hz':
        self.sysbus = Bus(width=8, clock='445MHz')
        if env['EARLYACK'] == 'no':
            self.membridge = BusBridge(max_buffer=16,
                              latency='47ns',
                              in_bus=Parent.sysbus, out_bus=Parent.membus)
            self.iobridge = BusBridge(max_buffer=16,
                             latency='115ns',
                             in_bus=Parent.sysbus, out_bus=Parent.iobus)
        else:
            self.membridge = BusBridge(max_buffer=16,
                              latency='47ns',
                              in_bus=Parent.sysbus, out_bus=Parent.membus,
                              ack_writes=True,
                              ack_delay='47ns')
            self.iobridge = BusBridge(max_buffer=16,
                             latency='115ns',
                             in_bus=Parent.sysbus, out_bus=Parent.iobus,
                             ack_writes=True,
                             ack_delay='115ns')
        self.tsunami.cchip.pio_latency = 21
    else:
        panic("Invalid Frequency for Monet Configuration: %s." % self.cpu.clock)
 
    self.ram = MainMem(in_bus = Parent.membus)

    self.iobus = Bus(width=8, clock='166.666666MHz')

    self.pcibridge = BusBridge(max_buffer=16, latency='170ns',
                                 in_bus=Parent.iobus, out_bus=Parent.pcibus)
    self.pcibus = Bus(width=8, clock='33.333333MHz')

    self.nibridge = BusBridge(max_buffer=16, latency='170ns',
                              in_bus=Parent.iobus, out_bus=Parent.nibus)
    self.nibus = Bus(width=8, clock='33.333333MHz')
    for i in xrange(len(self.tsunami.etherdev)):
        self.tsunami.etherdev[i].io_bus = Parent.nibus
    self.tsunami.cchip.io_bus = Parent.iobus
    self.tsunami.pchip.io_bus = Parent.sysbus

    return self
    
