from m5 import *
from Config import *
from Memory import *
from FuncUnit import *

#
# CacheCPU configuration
#
class CacheCPU(SimpleCPU):
    dcache = Parent.dcache
    icache = Parent.icache

class DedicatedCPU(SimpleCPU):
    dcache = BaseL1Cache(out_bus = Parent.l2bus,
                 size = '16kB',
                 latency = 3 * Parent.clock_cycle,
                 mshrs = 32)
    icache = BaseL1Cache(out_bus = Parent.l2bus,
                 size = '16kB',
                 latency = 1 * Parent.clock_cycle,
                 mshrs = 8)
    clock = '1GHz'
    
#
# FullCPU configuration
#
class DefaultBranchPred(BranchPred):
    pred_class = 'hybrid'
    local_hist_regs = '2ki'
    local_hist_bits = 11
    local_index_bits = 11
    local_xor = False
    global_hist_bits = 13
    global_index_bits = 13
    global_xor = False
    choice_index_bits = 13
    choice_xor = False
    ras_size = 16
    btb_size = '4ki'
    btb_assoc = 4

class DetailedCPU(FullCPU):
    num_icache_ports = 1
    branch_pred = DefaultBranchPred()
    ifq_size = 32
    iq = StandardIQ(size = 64, caps = [0, 0, 0, 0])
    iq_comm_latency = 1
    fupools = DefaultFUP()
    lsq_size = 32
    rob_size = 128
    storebuffer_size = 32
    width = 4
    decode_to_dispatch = 15
    mispred_recover = 3
    fetch_branches = 3
    itb = AlphaITB()
    dtb = AlphaDTB()
    dcache = Parent.dcache
    icache = Parent.icache
    pc_sample_interval = 100 * Parent.clock.period
