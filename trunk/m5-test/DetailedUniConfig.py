from m5 import *
from MemConfig import *
from FuncUnitConfig import *

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
    toL2Bus = ToL2Bus()
    l2 = L2(in_bus=Parent.toL2Bus, out_bus=Parent.toMemBus)
    dcache = DL1(out_bus=Parent.toL2Bus)
    icache = IL1(out_bus=Parent.toL2Bus)
    iq = StandardIQ(size = 64, caps = [0, 0, 0, 0])
    iq_comm_latency = 1
    fupools = DefaultFUP()
    lsq_size = 32
    rob_size = 196
    rob_caps = [0, 0, 0, 0]
    storebuffer_size = 32
    width = 8
    issue_bandwidth = [8, 8]
    prioritized_issue = False
    thread_weights = [1, 1, 1, 1]
    dispatch_to_issue = 1
    decode_to_dispatch = 15
    mispred_recover = 3
    fetch_branches = 3
    ifq_size = 32
    num_icache_ports = 1
    branch_pred = DefaultBranchPred()

class DetailedStandAlone(Root):
    cpu = DetailedCPU()
    toMemBus = ToMemBus()
    ram = SDRAM(in_bus=Parent.toMemBus)
