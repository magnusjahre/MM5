from m5 import *
import Splash2
import TestPrograms
import Spec2000
import workloads
import hog_workloads
import bw_workloads
import deterministic_fw_wls as fair_workloads
import single_core_fw as single_core
from DetailedConfig import *

###############################################################################
# Constants
###############################################################################

L2_BANK_COUNT = 4
all_protocols = ['none', 'msi', 'mesi', 'mosi', 'moesi', 'stenstrom']
snoop_protocols = ['msi', 'mesi', 'mosi', 'moesi']
directory_protocols = ['stenstrom']

###############################################################################
# Convenience Methods
###############################################################################

def createMemBus(bankcnt):
    assert 'MEMORY-BUS-CHANNELS' in env
    assert bankcnt >= int(env['MEMORY-BUS-CHANNELS'])
    banksPerBus = bankcnt / int(env['MEMORY-BUS-CHANNELS'])
    
    root.membus = [ConventionalMemBus() for i in range(int(env['MEMORY-BUS-CHANNELS']))]
    root.ram = [SDRAM(in_bus=root.membus[i]) for i in range(int(env['MEMORY-BUS-CHANNELS']))]
    
    for i in range(int(env['MEMORY-BUS-CHANNELS'])):
        root.membus[i].memory_controller = ReadyFirstMemoryController()
        root.membus[i].adaptive_mha = root.adaptiveMHA

def initSharedCache(bankcnt):
    if int(env['NP']) == 4:
        root.SharedCache = [SharedCache8M() for i in range(bankcnt)]
    elif int(env['NP']) == 8:
        root.SharedCache = [SharedCache16M() for i in range(bankcnt)]
    else:
        root.SharedCache = [SharedCache32M() for i in range(bankcnt)]
   
def setUpSharedCache(bankcnt):
    
    assert 'MEMORY-BUS-CHANNELS' in env
    assert bankcnt >= int(env['MEMORY-BUS-CHANNELS'])
    banksPerBus = bankcnt / int(env['MEMORY-BUS-CHANNELS'])
    
    curbus = 0
    buscnt = 0
    for i in range(bankcnt):
        root.SharedCache[i].in_interconnect = root.interconnect
        root.SharedCache[i].out_bus = root.membus[curbus]
        root.SharedCache[i].use_static_partitioning_for_warmup = True
        root.SharedCache[i].bank_count = bankcnt
        root.SharedCache[i].bank_id = i
        root.SharedCache[i].adaptive_mha = root.adaptiveMHA
        root.SharedCache[i].do_modulo_addr = True
        if l2mshrs != -1:
            root.SharedCache[i].mshrs = l2mshrs
        if l2mshrTargets != -1:
            root.SharedCache[i].tgts_per_mshr = l2mshrTargets
        buscnt += 1
        if buscnt % banksPerBus == 0:
            curbus += 1

###############################################################################
# Check command line options
###############################################################################

if "HELP" in env:
    print >>sys.stderr, '\nNCAR M5 Minimal Command Line:\n'
    print >>sys.stderr, './m5.opt -ENP=4 -EBENCHMARK=1 -EPROTOCOL=none -EINTERCONNECT=crossbar -EFASTFORWARDTICKS=1000 -ESIMULATETICKS=1000 -ESTATSFILE=test.txt ../../configs/CMP/run.py\n'
    print >>sys.stderr, 'Example trace argument: --Trace.flags=\"Cache\"\n'
    print >>sys.stderr, 'For other options see the top of the run.py file.\n'
    panic("Printed help text, quitting...")
    
if 'NP' not in env:
    panic("No number of processors was defined.\ne.g. -ENP=4\n")

if env['PROTOCOL'] not in all_protocols:
  #panic('No/Invalid cache coherence protocol specified!')
    env['PROTOCOL'] = 'none'

if 'BENCHMARK' not in env:
    panic("The BENCHMARK environment variable must be set!\ne.g. \
    -EBENCHMARK=Cholesky\n")

if not "ISEXPERIMENT" in env:
    if 'FASTFORWARDTICKS' not in env:
        panic("The FASTFORWARDTICKS environment variable must be set!\n\
        e.g. -EFASTFORWARDTICKS=10000\n")

if 'SIMULATETICKS' not in env and 'SIMINSTS' not in env \
and 'ISEXPERIMENT' not in env:
    panic("One of the SIMULATETICKS/SIMINSTS/ISEXPERIMENT environment \
variables must be set!\ne.g. -ESIMULATETICKS=10000\n")

if 'INTERCONNECT' not in env:
    if env['MEMORY-SYSTEM'] == 'Legacy': 
        panic("The INTERCONNECT environment variable must be set!\ne.g. \
    -EINTERCONNECT=bus\n")

if 'STATSFILE' not in env:
    panic('No statistics file name given! (-ESTATSFILE=foobar.txt)')
    
coherenceTrace = False
coherenceTraceStart = 0
if 'TRACE' in env:
    if env['PROTOCOL'] not in directory_protocols:
        panic('Tracing is only supported for directory protocols');
    coherenceTrace = True
    coherenceTraceStart = env['TRACE']
    print >>sys.stderr, 'warning: Protocol tracing is turned on!'
    
inDumpInterval = 0
if 'DUMPCCSTATS' in env:
    inDumpInterval = int(env['DUMPCCSTATS'])

icProfileStart = -1
if 'PROFILEIC' in env:
    icProfileStart = int(env['PROFILEIC'])

progressInterval = 0
if 'PROGRESS' in env:
    progressInterval = int(env['PROGRESS'])

# MSHR parameters
l1dmshrTargets = -1
l1mshrsData = -1
if 'MSHRSL1D' in env and 'MSHRL1DTARGETS' in env:
    l1mshrsData = int(env['MSHRSL1D'])
    l1dmshrTargets = int(env['MSHRL1DTARGETS'])

l1mshrsInst = -1
l1imshrTargets = -1
if 'MSHRSL1I' in env and 'MSHRL1ITARGETS' in env:
    l1mshrsInst = int(env['MSHRSL1I'])
    l1imshrTargets = int(env['MSHRL1ITARGETS'])

l2mshrTargets = -1
l2mshrs = -1
if 'MSHRSL2' in env and 'MSHRL2TARGETS' in env:
    l2mshrs = int(env['MSHRSL2'])
    l2mshrTargets = int(env['MSHRL2TARGETS'])

useAdaptiveMHA = False
if "USE-ADAPTIVE-MHA" in env:
    useAdaptiveMHA = True
    if 'ADAPTIVE-MHA-LOW-THRESHOLD' not in env:
        panic("A low threshold must be given when using the adaptive MHA (-EADAPTIVE-MHA-LOW-THRESHOLD)")
    if 'ADAPTIVE-MHA-HIGH-THRESHOLD' not in env:
        panic("A high threshold must be given when using the adaptive MHA (-EADAPTIVE-MHA-HIGH-THRESHOLD)")
    if 'ADAPTIVE-REPEATS' not in env:
        panic("The number of repeats to make a desicion must be given (-EADAPTIVE-REPEATS)")

useFairAdaptiveMHA = False
if "USE-FAIR-AMHA" in env:
    if 'FAIR-RESET-COUNTER' not in env:
        panic("The number of events to process must be given (-EFAIR-RESET-COUNTER)")
    #if 'FAIR-REDUCTION-THRESHOLD' not in env:
        #panic("A reduction threshold must be given (-EFAIR-REDUCTION-THRESHOLD)")
    #if 'FAIR-MIN-IP-ALLOWED' not in env:
        #panic("A minimum IP value must be given (-EFAIR-MIN-IP-ALLOWED)")
    useFairAdaptiveMHA = True

if "CACHE-PARTITIONING" in env:
    if env["CACHE-PARTITIONING"] == "Conventional" \
    or env["CACHE-PARTITIONING"] == "StaticUniform" \
    or env["CACHE-PARTITIONING"] == "MTP":
        pass
    else:
        panic("Only Conventional and StaticUniform cache partitioning are available")

if "MEMORY-BUS-SCHEDULER" in env:
    if env["MEMORY-BUS-SCHEDULER"] == "FCFS" \
    or env["MEMORY-BUS-SCHEDULER"] == "RDFCFS" \
    or env["MEMORY-BUS-SCHEDULER"] == "FNFQ" \
    or env["MEMORY-BUS-SCHEDULER"] == "TNFQ":
        pass
    else:
        panic("Only FCFS, RDFCFS, TNFQ and FNFQ memory bus schedulers are supported")

L2BankSize = -1
if "L2BANKSIZE" in env:
    L2BankSize = int(env["L2BANKSIZE"])
    
fairCrossbar = False
if "USE-FAIR-CROSSBAR" in env:
    fairCrossbar = True
    
###############################################################################
# Root, CPUs and L1 caches
###############################################################################

HierParams.cpu_count = int(env['NP'])

root = DetailedStandAlone()
if progressInterval > 0:
    root.progress_interval = progressInterval

# Create CPUs
BaseCPU.workload = Parent.workload
root.simpleCPU = [ CPU(defer_registration=True,)
                   for i in xrange(int(env['NP'])) ]
root.detailedCPU = [ DetailedCPU(defer_registration=True) 
                     for i in xrange(int(env['NP'])) ]

# Create L1 caches
if env['INTERCONNECT'] == 'bus':
    root.L1dcaches = [ DL1(out_bus=Parent.interconnect) 
                       for i in xrange(int(env['NP'])) ]
    root.L1icaches = [ IL1(out_bus=Parent.interconnect) 
                       for i in xrange(int(env['NP'])) ]
else:
    root.L1dcaches = [ DL1(out_interconnect=Parent.interconnect)
                       for i in xrange(int(env['NP'])) ]
    root.L1icaches = [ IL1(out_interconnect=Parent.interconnect)
                       for i in xrange(int(env['NP'])) ]

if env['PROTOCOL'] != 'none':
    if env['PROTOCOL'] in snoop_protocols:
        for cache in root.L1dcaches:
            cache.protocol = CoherenceProtocol(protocol=env['PROTOCOL'])
    elif env['PROTOCOL'] in directory_protocols:
        for cache in root.L1dcaches:
            cache.dirProtocolName = env['PROTOCOL']
            cache.dirProtocolDoTrace = coherenceTrace
            if coherenceTraceStart != 0:
                cache.dirProtocolTraceStart = coherenceTraceStart
            cache.dirProtocolDumpInterval = inDumpInterval
        
# Connect L1 caches to CPUs
for i in xrange(int(env['NP'])):
    root.simpleCPU[i].dcache = root.L1dcaches[i]
    root.simpleCPU[i].icache = root.L1icaches[i]
    root.detailedCPU[i].dcache = root.L1dcaches[i]
    root.detailedCPU[i].icache = root.L1icaches[i]
    root.simpleCPU[i].cpu_id = i
    root.detailedCPU[i].cpu_id = i
    root.L1dcaches[i].cpu_id = i
    root.L1icaches[i].cpu_id = i
    root.L1dcaches[i].memory_address_offset = i
    root.L1dcaches[i].memory_address_parts = int(env['NP'])
    root.L1icaches[i].memory_address_offset = i
    root.L1icaches[i].memory_address_parts = int(env['NP'])

if int(env['NP']) == 1:
    assert 'MEMORY-ADDRESS-OFFSET' in env and 'MEMORY-ADDRESS-PARTS' in env
    root.L1dcaches[0].memory_address_offset = int(env['MEMORY-ADDRESS-OFFSET'])
    root.L1dcaches[0].memory_address_parts = int(env['MEMORY-ADDRESS-PARTS'])
    root.L1icaches[0].memory_address_offset = int(env['MEMORY-ADDRESS-OFFSET'])
    root.L1icaches[0].memory_address_parts = int(env['MEMORY-ADDRESS-PARTS'])

if l1mshrsData != -1:
    for l1 in root.L1dcaches:
        l1.mshrs = l1mshrsData
        l1.tgts_per_mshr = l1dmshrTargets

if l1mshrsInst != -1:
    for l1 in root.L1icaches:
        l1.mshrs = l1mshrsInst
        l1.tgts_per_mshr = l1imshrTargets


# create the adaptiveMHA
# might only be used for tracing memory bus usage
root.adaptiveMHA = AdaptiveMHA()
root.adaptiveMHA.cpuCount = int(env["NP"])

if 'AMHA-PERIOD' in env:
    root.adaptiveMHA.sampleFrequency = int(env['AMHA-PERIOD'])
else:
    root.adaptiveMHA.sampleFrequency = 500000

if 'DUMP-INTERFERENCE' in env:
    root.adaptiveMHA.numReqsBetweenIDumps = int(env['DUMP-INTERFERENCE'])
    
for cpu in root.detailedCPU:
    cpu.adaptiveMHA = root.adaptiveMHA
    
for l1 in root.L1dcaches:
    l1.adaptive_mha = root.adaptiveMHA
    
for l1 in root.L1icaches:
    l1.adaptive_mha = root.adaptiveMHA
    
if useAdaptiveMHA:
    root.adaptiveMHA.lowThreshold = float(env["ADAPTIVE-MHA-LOW-THRESHOLD"])
    root.adaptiveMHA.highThreshold = float(env["ADAPTIVE-MHA-HIGH-THRESHOLD"])
    root.adaptiveMHA.neededRepeats = int(env["ADAPTIVE-REPEATS"])
    root.adaptiveMHA.onlyTraceBus = False
    root.adaptiveMHA.useFairMHA = False

    if "ADAPTIVE-MHA-LIMIT" in env:
        root.adaptiveMHA.tpUtilizationLimit = float(env["ADAPTIVE-MHA-LIMIT"])
    
elif useFairAdaptiveMHA:
    root.adaptiveMHA.lowThreshold = 0.0 # not used
    root.adaptiveMHA.highThreshold = 1.0 # not used
    if "ADAPTIVE-REPEATS" in env:
        root.adaptiveMHA.neededRepeats = int(env["ADAPTIVE-REPEATS"])
    else:
        root.adaptiveMHA.neededRepeats = 0
    root.adaptiveMHA.onlyTraceBus = False
    root.adaptiveMHA.useFairMHA = True
    
    root.adaptiveMHA.resetCounter = int(env["FAIR-RESET-COUNTER"])
    
    if "FAIR-REDUCTION-THRESHOLD" in env:
        root.adaptiveMHA.reductionThreshold = float(env["FAIR-REDUCTION-THRESHOLD"])
    else:
        root.adaptiveMHA.reductionThreshold = 0.0
        
    if "FAIR-MIN-IP-ALLOWED"in env:
        root.adaptiveMHA.minInterferencePointAllowed = float(env["FAIR-MIN-IP-ALLOWED"])
    else:
        root.adaptiveMHA.minInterferencePointAllowed = 0.0
else:
    root.adaptiveMHA.lowThreshold = 0.0 # not used
    root.adaptiveMHA.highThreshold = 1.0 # not used
    root.adaptiveMHA.neededRepeats = 1 # not used
    root.adaptiveMHA.onlyTraceBus = True
    root.adaptiveMHA.useFairMHA = False

if "STATICASYMMETRICMHA" in env:
    tmpSAMList = env["STATICASYMMETRICMHA"].split(',')
    for i in range(env["NP"]):
        tmpSAMList[i] = int(tmpSAMList[i])
    root.adaptiveMHA.staticAsymmetricMHA = tmpSAMList
else:
    tmpSAMList = []
    for i in range(env["NP"]):
        tmpSAMList.append(-1)
    root.adaptiveMHA.staticAsymmetricMHA = tmpSAMList


###############################################################################
# Fast-forwarding
###############################################################################

uniformPartStart = -1
cacheProfileStart = -1
simulationEnds = -1
if env['BENCHMARK'] in Splash2.benchmarkNames:
    # Scientific workloads
    root.sampler = Sampler()
    root.sampler.phase0_cpus = Parent.simpleCPU
    root.sampler.phase1_cpus = Parent.detailedCPU
    if 'ISEXPERIMENT' in env and env['PROTOCOL'] in directory_protocols:
        root.sampler.periods = [0, 50000000000] # sampler is not used
        root.adaptiveMHA.startTick = 0
        for cpu in root.detailedCPU:
            cpu.max_insts_any_thread = \
                Splash2.instructions[int(env['NP'])][env['BENCHMARK']]
    elif 'SIMINSTS' in env:
        root.sampler.periods = [0, 50000000000] # sampler is not used
        root.adaptiveMHA.startTick = 0
        for cpu in root.detailedCPU:
            cpu.max_insts_any_thread = int(env['SIMINSTS'])
    elif 'FASTFORWARDTICKS' not in env:
        fwticks, simticks = Splash2.fastforward[env['BENCHMARK']]
        root.sampler.periods = [fwticks, simticks]
        root.adaptiveMHA.startTick = fwticks
        uniformPartStart = fwticks
    else:
        root.sampler.periods = [env['FASTFORWARDTICKS'], 
                                int(env['SIMULATETICKS'])]
        root.adaptiveMHA.startTick = int(env['FASTFORWARDTICKS'])
        uniformPartStart = int(env['FASTFORWARDTICKS'])
            
    root.setCPU(root.simpleCPU)

elif env['BENCHMARK'] in single_core.configuration:
    assert int(env['NP']) == 1
    
    fwticks = 0
    if 'ISEXPERIMENT' in env:
        fwticks = single_core.configuration[env['BENCHMARK']][1]
    else:
        fwticks = int(env['FASTFORWARDTICKS'])
    fwticks = fwticks
    
    warmup = 0
    if 'SIMULATETICKS' in env:
        assert 'SIMINTS' not in env
        simulateCycles = int(env['SIMULATETICKS'])
        if 'ISEXPERIMENT' in env:
            simulateCycles = int(env['SIMULATETICKS'] + warmup)
            Statistics.dump_reset = True
            Statistics.dump_cycle = fwticks + warmup
        else:
            warmup = 0
            
    else:
        assert 'SIMULATETICKS' not in env
        simulateCycles = 1000000000 # max cycles, hopefully not necessary
        for cpu in root.detailedCPU:
            cpu.max_insts_any_thread = int(env['SIMINSTS'])
    
    root.sampler = Sampler()
    root.sampler.phase0_cpus = Parent.simpleCPU
    root.sampler.phase1_cpus = Parent.detailedCPU
    root.sampler.periods = [fwticks, simulateCycles]
    
    root.adaptiveMHA.startTick = fwticks + warmup
    uniformPartStart = fwticks + warmup
    cacheProfileStart = fwticks + warmup
    simulationEnds = fwticks + simulateCycles
    Bus.switch_at = fwticks + warmup

    
elif not (env['BENCHMARK'].isdigit() 
    or env['BENCHMARK'].startswith("hog") 
    or env['BENCHMARK'].startswith("bw")
    or env['BENCHMARK'].startswith("fair")):
    # Simulator test workloads
    root.sampler = Sampler()
    root.sampler.phase0_cpus = Parent.simpleCPU
    root.sampler.phase1_cpus = Parent.detailedCPU
    root.sampler.periods = [int(env['FASTFORWARDTICKS']),
                            int(env['SIMULATETICKS'])] 
    root.adaptiveMHA.startTick = int(env['FASTFORWARDTICKS'])
    uniformPartStart = int(env['FASTFORWARDTICKS'])
    cacheProfileStart = int(env['FASTFORWARDTICKS'])
    simulationEnds = int(env['FASTFORWARDTICKS']) + int(env['SIMULATETICKS'])
    root.setCPU(root.simpleCPU)
else:
    # Multi-programmed workload
    root.samplers = [ Sampler() for i in xrange(int(env['NP'])) ]

    if 'ISEXPERIMENT' in env:
        if env['BENCHMARK'].startswith("hog"):
            tmpBM = env['BENCHMARK'].replace("hog","")
            fwCycles = hog_workloads.hog_workloads[int(env['NP'])][int(tmpBM)][1]
            fwCycles.append(1000000000) # the bw hog is fastforwarded 1 billion clock cycles
        elif env['BENCHMARK'].startswith("bw"):
            tmpBM = env['BENCHMARK'].replace("bw","")
            fwCycles = bw_workloads.bw_workloads[int(env['NP'])][int(tmpBM)][1]
        elif env['BENCHMARK'].startswith("fair"):
            tmpBM = env['BENCHMARK'].replace("fair","")
            fwCycles = fair_workloads.workloads[int(env['NP'])][int(tmpBM)][1]
        else:
            fwCycles = \
                workloads.workloads[int(env['NP'])][int(env['BENCHMARK'])][1]
    else:
        fwCycles = [int(env['FASTFORWARDTICKS']) 
                    for i in xrange(int(env['NP']))]
    
    simulateStart = max(fwCycles)
    if 'ISEXPERIMENT' in env:
        # warm up removed as it will influence alone and shared configs differently
        warmup = 0
        simulateCycles = int(env['SIMULATETICKS']) + warmup
        Statistics.dump_reset = True
        Statistics.dump_cycle = simulateStart + warmup
    else:
        warmup = 0
        simulateCycles = int(env['SIMULATETICKS'])
    
    root.adaptiveMHA.startTick = simulateStart + warmup
    uniformPartStart = simulateStart + warmup
    cacheProfileStart = simulateStart + warmup
    simulationEnds = simulateStart + simulateCycles
    Bus.switch_at = simulateStart + warmup

    for i in xrange(int(env['NP'])):
        root.samplers[i].phase0_cpus = [Parent.simpleCPU[i]]
        root.samplers[i].phase1_cpus = [Parent.detailedCPU[i]]
        root.samplers[i].periods = [fwCycles[i], simulateCycles 
                                    + (simulateStart - fwCycles[i])]

    root.setCPU(root.simpleCPU)

if cacheProfileStart != -1:
    BaseCache.detailed_sim_start_tick = cacheProfileStart

if simulationEnds != -1:
    Bus.final_sim_tick = simulationEnds
    BaseCache.detailed_sim_end_tick = simulationEnds

###############################################################################
# Interconnect, L2 caches and Memory Bus
###############################################################################

if env['BENCHMARK'] in Splash2.benchmarkNames:
    BaseCache.multiprog_workload = False
else:
    BaseCache.multiprog_workload = True

if env['BENCHMARK'] in Splash2.benchmarkNames and 'FASTFORWARDTICKS' not in env:
    if 'PROFILEIC' in env:
        print >>sys.stderr, "warning: Production workload, ignoring user supplied profile start"
    icProfileStart = 0 #Splash2.fastforward[env['BENCHMARK']][0]

if env['BENCHMARK'].isdigit() and 'ISEXPERIMENT' in env:
    if 'PROFILEIC' in env:
        print >>sys.stderr, "warning: Production workload, ignoring user supplied profile start"
    fwCycles = workloads.workloads[int(env['NP'])][int(env['BENCHMARK'])][1]
    icProfileStart = max(fwCycles)


assert 'MEMORY-SYSTEM' in env

if env['MEMORY-SYSTEM'] == "Legacy":

    # All runs use modulo based L2 bank selection
    moduloAddr = True

    Interconnect.cpu_count = int(env['NP'])
    root.setInterconnect(env['INTERCONNECT'],
                         L2_BANK_COUNT,
                         icProfileStart,
                         moduloAddr,
                         useFairAdaptiveMHA,
                         fairCrossbar,
                         cacheProfileStart)

    root.setL2Banks()
    if env['PROTOCOL'] in directory_protocols:
        for bank in root.l2:
            bank.dirProtocolName = env['PROTOCOL']
            bank.dirProtocolDoTrace = coherenceTrace
        
            if coherenceTraceStart != 0:
                bank.dirProtocolTraceStart = coherenceTraceStart

    if l2mshrs != -1:
        for bank in root.l2:
            bank.mshrs = l2mshrs
            bank.tgts_per_mshr = l2mshrTargets
        
    for l2 in root.l2:
        l2.adaptive_mha = root.adaptiveMHA

    if env["CACHE-PARTITIONING"] == "StaticUniform":
        for bank in root.l2:
            bank.use_static_partitioning = True
            bank.static_part_start_tick = uniformPartStart
        
    if env["CACHE-PARTITIONING"] == "MTP":
        for bank in root.l2:
            bank.use_mtp_partitioning = True
            bank.use_static_partitioning = True
            bank.static_part_start_tick = uniformPartStart

    if cacheProfileStart != -1:
        for dc in root.L1dcaches:
            dc.detailed_sim_start_tick = cacheProfileStart
        for ic in root.L1dcaches:
            ic.detailed_sim_start_tick = cacheProfileStart
        for bank in root.l2:
            bank.detailed_sim_start_tick = cacheProfileStart

    if simulationEnds != -1:
        for bank in root.l2:
            bank.detailed_sim_end_tick = simulationEnds
        
    root.adaptiveMHA.printInterference = True
    root.adaptiveMHA.finalSimTick = simulationEnds

        
    if L2BankSize != -1:
        for bank in root.l2:
            bank.size = str(L2BankSize)+"kB"

    for bank in root.l2:
    #if int(env["NP"]) > 1:
        bank.use_static_partitioning_for_warmup = True
        bank.static_part_start_tick = uniformPartStart
    #else:
        #bank.use_static_partitioning_for_warmup = False

    # set up memory bus and memory controller
    root.toMemBus = ConventionalMemBus()
    root.toMemBus.adaptive_mha = root.adaptiveMHA

    if "INFINITE-MEM-BW" in env:
        root.toMemBus.infinite_bw = True

    #root.toMemBus.fast_forward_controller = FastForwardMemoryController()
        
    if env["MEMORY-BUS-SCHEDULER"] == "RDFCFS":
        root.toMemBus.memory_controller = ReadyFirstMemoryController()
        if "MEMORY-BUS-PAGE-POLICY" in env:
            root.toMemBus.memory_controller.page_policy = env["MEMORY-BUS-PAGE-POLICY"]
        if "MEMORY-BUS-PRIORITY-SCHEME" in env:
            root.toMemBus.memory_controller.priority_scheme = env["MEMORY-BUS-PRIORITY-SCHEME"]
    elif env["MEMORY-BUS-SCHEDULER"] == "FCFS":
        root.toMemBus.memory_controller = InOrderMemoryController()
    elif env["MEMORY-BUS-SCHEDULER"] == "FNFQ":
        root.toMemBus.memory_controller = FairNFQMemoryController()
    elif env["MEMORY-BUS-SCHEDULER"] == "TNFQ":
        root.toMemBus.memory_controller = ThroughputNFQMemoryController()
    else:
        # default is RDFCFS
        root.toMemBus.memory_controller = ReadyFirstMemoryController()

elif env['MEMORY-SYSTEM'] == "CrossbarBased":

    bankcnt = 4
    createMemBus(bankcnt)
    initSharedCache(bankcnt)

    root.interconnect = InterconnectCrossbar()
    root.interconnect.cpu_count = int(env['NP'])
    root.interconnect.detailed_sim_start_tick = cacheProfileStart
    root.interconnect.shared_cache_writeback_buffers = root.SharedCache[0].write_buffers
    root.interconnect.shared_cache_mshrs = root.SharedCache[0].mshrs
    root.interconnect.adaptive_mha = root.adaptiveMHA

    setUpSharedCache(bankcnt)

elif env['MEMORY-SYSTEM'] == "RingBased":

    panic("Ring based CMP config not implemented")

else:
    panic("MEMORY-SYSTEM parameter must be Legacy, CrossbarBased or RingBased")


###############################################################################
# Workloads
###############################################################################

# Storage for multiprogrammed workloads
prog = []

###############################################################################
# SPLASH-2 
###############################################################################

if env['BENCHMARK'] == 'Cholesky':
    root.workload = Splash2.Cholesky()
elif env['BENCHMARK'] == 'FFT':
    root.workload = Splash2.FFT()
elif env['BENCHMARK'] == 'LUContig':
    root.workload = Splash2.LU_contig()
elif env['BENCHMARK'] == 'LUNoncontig':
    root.workload = Splash2.LU_noncontig()
elif env['BENCHMARK'] == 'Radix':
    root.workload = Splash2.Radix()
elif env['BENCHMARK'] == 'Barnes':
    root.workload = Splash2.Barnes()
elif env['BENCHMARK'] == 'FMM':
    root.workload = Splash2.FMM()
elif env['BENCHMARK'] == 'OceanContig':
    root.workload = Splash2.Ocean_contig()
elif env['BENCHMARK'] == 'OceanNoncontig':
    root.workload = Splash2.Ocean_noncontig()
elif env['BENCHMARK'] == 'Raytrace':
    root.workload = Splash2.Raytrace()
elif env['BENCHMARK'] == 'WaterNSquared':
    root.workload = Splash2.Water_nsquared()
elif env['BENCHMARK'] == 'WaterSpatial':
    root.workload = Splash2.Water_spatial()

###############################################################################
# SPEC 2000
###############################################################################

elif env['BENCHMARK'] == 'gzip':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.GzipSource())
elif env['BENCHMARK'] == 'vpr':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.VprPlace())
elif env['BENCHMARK'] == 'gcc':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Gcc166())
elif env['BENCHMARK'] == 'mcf':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Mcf())
elif env['BENCHMARK'] == 'crafty':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Crafty())
elif env['BENCHMARK'] == 'parser':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Parser())
elif env['BENCHMARK'] == 'eon':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Eon1())
elif env['BENCHMARK'] == 'perlbmk':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Perlbmk1())
elif env['BENCHMARK'] == 'gap':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Gap())
elif env['BENCHMARK'] == 'vortex1':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Vortex1())
elif env['BENCHMARK'] == 'bzip':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Bzip2Source())
elif env['BENCHMARK'] == 'twolf':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Twolf())
elif env['BENCHMARK'] == 'wupwise':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Wupwise())
elif env['BENCHMARK'] == 'swim':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Swim())
elif env['BENCHMARK'] == 'mgrid':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Mgrid())
elif env['BENCHMARK'] == 'applu':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Applu())
elif env['BENCHMARK'] == 'mesa':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Mesa())
elif env['BENCHMARK'] == 'galgel':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Galgel())
elif env['BENCHMARK'] == 'art':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Art1())
elif env['BENCHMARK'] == 'equake':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Equake())
elif env['BENCHMARK'] == 'facerec':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Facerec())
elif env['BENCHMARK'] == 'ammp':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Ammp())
elif env['BENCHMARK'] == 'lucas':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Lucas())
elif env['BENCHMARK'] == 'fma3d':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Fma3d())
elif env['BENCHMARK'] == 'sixtrack':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Sixtrack())
elif env['BENCHMARK'] == 'apsi':
    for i in range(int(env['NP'])):
        prog.append(Spec2000.Apsi())

###############################################################################
# Multi-programmed workloads
###############################################################################

elif env['BENCHMARK'].isdigit():
    prog = Spec2000.createWorkload(
               workloads.workloads[int(env['NP'])][int(env['BENCHMARK'])][0])

###############################################################################
# Multi-programmed bw intensive workloads
###############################################################################

elif env['BENCHMARK'].startswith("bw"):
    tmpBM = env['BENCHMARK'].replace("bw","")
    prog = Spec2000.createWorkload(
               bw_workloads.bw_workloads[int(env['NP'])][int(tmpBM)][0])

###############################################################################
# Multi-programmed fairness workloads
###############################################################################

elif env['BENCHMARK'].startswith("fair"):
    tmpBM = env['BENCHMARK'].replace("fair","")
    prog = Spec2000.createWorkload(
               fair_workloads.workloads[int(env['NP'])][int(tmpBM)][0])

###############################################################################
# Multi-programmed workloads with memory hog
###############################################################################

elif env['BENCHMARK'].startswith("hog"):
    tmpBM = env['BENCHMARK'].replace("hog","")
    prog = Spec2000.createWorkload(
               hog_workloads.hog_workloads[int(env['NP'])][int(tmpBM)][0])
    prog.append(TestPrograms.ThrashCache())


###############################################################################
# Single core workloads
###############################################################################

elif env['BENCHMARK'] in single_core.configuration:
    prog.append(Spec2000.createWorkload([single_core.configuration[env['BENCHMARK']][0]]))

###############################################################################
# Testprograms
###############################################################################

elif env['BENCHMARK'] == 'hello':
    root.workload = TestPrograms.HelloWorld()
elif env['BENCHMARK'] == 'thrashCache':
    root.workload = TestPrograms.ThrashCache()
elif env['BENCHMARK'] == 'thrashCacheAll':
    for i in range(int(env['NP'])):
        prog.append(TestPrograms.ThrashCache())
else:
    panic("The BENCHMARK environment variable was set to something improper\n")

# Create multi-programmed workloads
if prog != []:
    for i in range(int(env['NP'])):
        root.simpleCPU[i].workload = prog[i]
        root.detailedCPU[i].workload = prog[i]

###############################################################################
# Statistics
###############################################################################

root.stats = Statistics(text_file=env['STATSFILE'])
