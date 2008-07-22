from m5 import *
from MemConfig import *
from FuncUnitConfig import *

###############################################################################
# Branch Predictor
###############################################################################

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
    btb_size = '2ki'
    btb_assoc = 4

###############################################################################
# CPUs
###############################################################################

class DetailedCPU(FullCPU):

    iq = StandardIQ(size = 64, caps = [0, 0, 0, 0])
    iq_comm_latency = 1
    fupools = DefaultFUP()
    lsq_size = 32
    rob_size = 128
    rob_caps = [0, 0, 0, 0]
    storebuffer_size = 32
    width = 8
    issue_bandwidth = [8, 8]
    prioritized_issue = False
    thread_weights = [1, 1, 1, 1]
    dispatch_to_issue = 1
    decode_to_dispatch = 10
    mispred_recover = 3
    fetch_branches = 3
    ifq_size = 32
    num_icache_ports = 1
    branch_pred = DefaultBranchPred()
    
    def setCache(self, dcache, icache):
        self.dcache = dcache
        self.icache = icache

class CPU(SimpleCPU):
    
    def setCache(self, dcache, icache):
        self.dcache = dcache
        self.icache = icache

###############################################################################
# Root
###############################################################################

class DetailedStandAlone(Root):

    clock = '4000MHz'
    toMemBus = ConventionalMemBus()
    ram = SDRAM(in_bus=Parent.toMemBus)
    l2 = []
    
    def setCPU(self, inCPU):
        self.cpu = inCPU
    
    #def setNumCPUs(self, numCPUs):
        #self.interconnect.L1CacheCount = (numCPUs*2)
        
    def setInterconnect(self, optionString, L2BankCount, profileStart, moduloAddr, useFairAMHA, useFairCrossbar):
        if optionString == 'bus':
            self.interconnect = ToL2Bus()
            self.createL2(True, L2BankCount, moduloAddr)
        elif optionString == 'myBus':
            self.interconnect = InterconnectBus()
            self.createL2(False, L2BankCount, moduloAddr)
        elif optionString == 'crossbar':
            self.interconnect = InterconnectCrossbar()
            self.interconnect.use_NFQ_arbitration = useFairCrossbar
            self.createL2(False, L2BankCount, moduloAddr)
        elif optionString == 'ideal':
            self.interconnect = InterconnectIdeal()
            self.createL2(False, L2BankCount, moduloAddr)
        elif optionString == 'idealwdelay':
            self.interconnect = InterconnectIdealWithDelay()
            self.createL2(False, L2BankCount, moduloAddr)
        elif optionString == 'pipeBus':
            self.interconnect = PipelinedBus()
            self.createL2(False, L2BankCount, moduloAddr)
        elif optionString == 'butterfly':
            self.interconnect = InterconnectButterfly()
            self.createL2(False, L2BankCount, moduloAddr)
        else:
            panic('Unknown interconnect selected')
            
        if useFairAMHA:
            self.interconnect.adaptive_mha = self.adaptiveMHA
            
        if profileStart != -1 and optionString != 'bus':
            self.interconnectProfiler = InterconnectProfile()
            self.interconnectProfiler.traceSends = True
            self.interconnectProfiler.traceChannelUtil = True
            self.interconnectProfiler.traceStartTick = profileStart
            self.interconnectProfiler.interconnect = self.interconnect
            
    def createL2(self, bus, L2BankCount, moduloAddr):
        for bankID in range(0, L2BankCount):
            thisBank = None
            if bus:
                thisBank = L2Bank(in_bus=Parent.interconnect, out_bus=Parent.toMemBus)
            else:
                thisBank = L2Bank(in_interconnect=Parent.interconnect, out_bus=Parent.toMemBus)
                
            if moduloAddr and not bus:
                thisBank.setModuloAddr(bankID, L2BankCount)
            else:
                thisBank.setAddrRange(bankID, L2BankCount)
            self.l2.append(thisBank)

    def setL2Banks(self):
        self.L2Bank0 = self.l2[0]
        self.L2Bank1 = self.l2[1]
        self.L2Bank2 = self.l2[2]
        self.L2Bank3 = self.l2[3]