from MemConfig import *

class DetailedCPU(DerivAlphaFullCPU):

    numThreads = 1

    decodeToFetchDelay=1
    renameToFetchDelay=1
    iewToFetchDelay=1
    commitToFetchDelay=1
    fetchWidth=8
    renameToDecodeDelay=1
    iewToDecodeDelay=1
    commitToDecodeDelay=1
    fetchToDecodeDelay=1
    decodeWidth=8
    iewToRenameDelay=1
    commitToRenameDelay=1
    decodeToRenameDelay=1
    renameWidth=8
    commitToIEWDelay=1
    renameToIEWDelay=1
    issueToExecuteDelay=1
    issueWidth=8
    executeWidth=8
    executeIntWidth=8
    executeFloatWidth=8
    executeBranchWidth=8
    executeMemoryWidth=8
        
    iewToCommitDelay=1
    renameToROBDelay=1
    commitWidth=16
    squashWidth=100
        
    local_predictor_size=4096
    local_ctr_bits=2
    local_history_table_size=1024
    local_history_bits=10
    global_predictor_size=4096
    global_ctr_bits=2
    global_history_bits=12
    choice_predictor_size=4096
    choice_ctr_bits=2
    BTBEntries=4096
    BTBTagSize=16
    RASSize=16

    LQEntries=4096
    SQEntries=4096
    LFSTSize=1024
    SSITSize=1024
        
    numPhysIntRegs=256
    numPhysFloatRegs=256
    numIQEntries=192
    numROBEntries=192
        
    instShiftAmt=2
    
    defer_registration= False
    toL2Bus = ToL2Bus()
    l2 = L2(in_bus=Parent.toL2Bus, out_bus=Parent.toMemBus)
        
class DetailedStandAlone(Root):
    toMemBus = ToMemBus()
    ram = SDRAM(in_bus=Parent.toMemBus)
    cpu = DetailedCPU()
