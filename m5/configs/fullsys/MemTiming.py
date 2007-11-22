from m5 import *
from Config import *

class MonetIntALU(FUDesc):
    opList = [ OpDesc(opClass='IntAlu') ]
    count = 4

class MonetIntMultDiv(FUDesc):
    opList = [ OpDesc(opClass='IntMult', opLat=7) ] 
    count=1

class MonetFP_ALU(FUDesc):
    opList = [ OpDesc(opClass='FloatAdd', opLat=4),
               OpDesc(opClass='FloatCmp', opLat=4),
               OpDesc(opClass='FloatCvt', opLat=4) ]
    count = 1

class MonetFP_Mult(FUDesc):
    opList = [ OpDesc(opClass='FloatMult', opLat=4)]
    count = 1
    
#These numbers are for double precision calculations
class MonetFP_Div(FUDesc):
    opList = [ OpDesc(opClass='FloatDiv', opLat=15, issueLat=12) ]
    count = 1

#These numbers are for double precision calculations
class MonetFP_Sqrt(FUDesc):
    opList = [ OpDesc(opClass='FloatSqrt', opLat=33, issueLat=30) ]
    count = 1

class MonetReadPort(FUDesc):
    opList = [ OpDesc(opClass='MemRead') ]
    count = 0

class MonetWritePort(FUDesc):
    opList = [ OpDesc(opClass='MemWrite') ]
    count = 0

class MonetRdWrPort(FUDesc):
    opList = [ OpDesc(opClass='MemRead'), OpDesc(opClass='MemWrite') ]
    count = 2

class MonetIprPort(FUDesc):
    opList = [ OpDesc(opClass='IprAccess', opLat = 3, issueLat = 3) ]
    count = 1

class MonetFUP(FuncUnitPool):
    FUList = [ MonetIntALU(), MonetIntMultDiv(), MonetFP_ALU(), MonetFP_Mult(),
               MonetFP_Div(), MonetFP_Sqrt(), MonetReadPort(), MonetWritePort(), 
               MonetRdWrPort(), MonetIprPort() ]


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

class Monet(FullCPU):
    iq = StandardIQ(size=64, caps = [0, 0, 0, 0])
    iq_comm_latency = 1
    branch_pred=MonetBranchPred()
    fupools=MonetFUP()
    lsq_size=64 # really 32 load 32 store
    rob_size=80
    width=4
    decode_to_dispatch=8 # this is a complete guess
    mispred_recover=3
    fetch_branches=3
    ifq_size=32
    num_icache_ports=1
    storebuffer_size=16
    icache=Parent.icache
    dcache=Parent.dcache

class MonetIL1Cache(BaseCache):
    size='64kB'
    assoc=2
    block_size=64
    tgts_per_mshr=16
    latency='2c'
    mshrs=8

class MonetDL1Cache(BaseCache):
    size='64kB'
    assoc=2
    block_size=64
    tgts_per_mshr=16
    latency='2c'
    mshrs=8

class MonetL1DBus(Bus):
    width=16 
    clock='1.5c'

class MonetL1IBus(Bus):
    width=16
    clock='1.5c'

class MonetCacheBridge(BusBridge):
    max_buffer=16
    latency='8c'

class MonetL2Bus(Bus):
    width=16
    clock='1.5c'

class MonetL2Cache(BaseCache):
    size='4MB'
    assoc=1
    block_size=64
    latency='12c'
    mshrs=6
    tgts_per_mshr=16

def MonetSimpleCache(Processor = Monet, **kwargs):
    self = Processor(**kwargs)
    self.icache = MonetIL1Cache(in_bus = NULL, out_bus = Parent.l2bus)
    self.dcache = MonetDL1Cache(in_bus = NULL, out_bus = Parent.l2bus)
    self.l2bus = MonetL2Bus()
    self.l2 = MonetL2Cache(in_bus = Parent.l2bus, out_bus = Parent.sysbus)
    return self

    
class NonFullSysMonet(Root):
    clock = env['FREQUENCY']
    cpu = MonetSimpleCache(Processor=Monet())
    hier = HierParams(do_data=False, do_events=True)
    sysbus = Bus(width=8, clock='1.5c')
    membus = Bus(width=32, clock='83MHz')
    membridge = BusBridge(max_buffer=16, latency='20ns',
                                in_bus=Parent.sysbus, out_bus=Parent.membus)
    dram = BaseMemory(in_bus = Parent.membus, latency='32ns')
    
 
class MemLatRd(LiveProcess):
    cmd='/n/zeep/z/saidi/work/m5/build/ALPHA/lat_mem_rd_2MB 2 64'

BaseCPU.workload = MemLatRd()
root = NonFullSysMonet
