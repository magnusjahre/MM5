from m5 import *
import Splash2
import TestPrograms
import Spec2000
import workloads
import hog_workloads
import bw_workloads
from DetailedConfig import *

###############################################################################
# Constants
###############################################################################

L2_BANK_COUNT = 4
all_protocols = ['none', 'msi', 'mesi', 'mosi', 'moesi', 'stenstrom']
snoop_protocols = ['msi', 'mesi', 'mosi', 'moesi']
directory_protocols = ['stenstrom']

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
  panic('No/Invalid cache coherence protocol specified!')

if 'BENCHMARK' not in env:
    panic("The BENCHMARK environment variable must be set!\ne.g. \
    -EBENCHMARK=Cholesky\n")

# Multi-programmed workloads (numbered 1 to N) reads fast-forward cycles 
# from a config file
# Splash benchmarks can read from config file
if not ((env['BENCHMARK'].isdigit()) or (env['BENCHMARK'] 
        in Splash2.benchmarkNames) or env['BENCHMARK'].startswith("hog") or env['BENCHMARK'].startswith("bw")):
    if 'FASTFORWARDTICKS' not in env:
        panic("The FASTFORWARDTICKS environment variable must be set!\n\
        e.g. -EFASTFORWARDTICKS=10000\n")

if 'SIMULATETICKS' not in env and 'SIMINSTS' not in env \
and 'ISEXPERIMENT' not in env:
    panic("One of the SIMULATETICKS/SIMINSTS/ISEXPERIMENT environment \
variables must be set!\ne.g. -ESIMULATETICKS=10000\n")

if 'INTERCONNECT' not in env:
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


if "CACHE-PARTITIONING" in env:
    if env["CACHE-PARTITIONING"] == "Conventional" or env["CACHE-PARTITIONING"] == "StaticUniform":
        pass
    else:
        panic("Only Conventional and StaticUniform cache partitioning are available")

if "MEMORY-BUS-SCHEDULER" in env:
    if env["MEMORY-BUS-SCHEDULER"] == "FCFS" \
    or env["MEMORY-BUS-SCHEDULER"] == "RDFCFS" \
    or env["MEMORY-BUS-SCHEDULER"] == "NFQ":
        pass
    else:
        panic("Only FCFS, RD-FCFS and NFQ memory bus schedulers are supported")

###############################################################################
# Root, CPUs and L1 caches
###############################################################################

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
root.adaptiveMHA.sampleFrequency = 100000
    
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
else:
    root.adaptiveMHA.lowThreshold = 0.0 # not used
    root.adaptiveMHA.highThreshold = 1.0 # not used
    root.adaptiveMHA.neededRepeats = 1 # not used
    root.adaptiveMHA.onlyTraceBus = True

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
elif not (env['BENCHMARK'].isdigit() or env['BENCHMARK'].startswith("hog") or env['BENCHMARK'].startswith("bw")):
    # Simulator test workloads
    root.sampler = Sampler()
    root.sampler.phase0_cpus = Parent.simpleCPU
    root.sampler.phase1_cpus = Parent.detailedCPU
    root.sampler.periods = [int(env['FASTFORWARDTICKS']),
                            int(env['SIMULATETICKS'])] 
    root.adaptiveMHA.startTick = int(env['FASTFORWARDTICKS'])
    uniformPartStart = int(env['FASTFORWARDTICKS'])
    cacheProfileStart = int(env['FASTFORWARDTICKS'])
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
        else:
            fwCycles = \
                workloads.workloads[int(env['NP'])][int(env['BENCHMARK'])][1]
    else:
        fwCycles = [int(env['FASTFORWARDTICKS']) 
                    for i in xrange(int(env['NP']))]
    
    simulateStart = max(fwCycles)
    if 'ISEXPERIMENT' in env:
        # add 10M cycles warm-up to avoid startup effects (and to make sure that stats are reset only once)
        warmup = 10000000
        simulateCycles = int(env['SIMULATETICKS'] + warmup)
        Statistics.dump_reset = True
        Statistics.dump_cycle = simulateStart + warmup
    else:
        warmup = 0
        simulateCycles = int(env['SIMULATETICKS'])
    
    root.adaptiveMHA.startTick = simulateStart + warmup
    uniformPartStart = simulateStart #use warm-up to converge on static cache alloc
    cacheProfileStart = simulateStart

    for i in xrange(int(env['NP'])):
        root.samplers[i].phase0_cpus = [Parent.simpleCPU[i]]
        root.samplers[i].phase1_cpus = [Parent.detailedCPU[i]]
        root.samplers[i].periods = [fwCycles[i], simulateCycles 
                                    + (simulateStart - fwCycles[i])]

    root.setCPU(root.simpleCPU)

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

# All runs use modulo based L2 bank selection
moduloAddr = True

Interconnect.cpu_count = int(env['NP'])
root.setInterconnect(env['INTERCONNECT'],
                     L2_BANK_COUNT,
                     icProfileStart,
                     moduloAddr)

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

if env["CACHE-PARTITIONING"] == "StaticUniform":
    for bank in root.l2:
        bank.use_static_partitioning = True
        bank.static_part_start_tick = uniformPartStart

if cacheProfileStart != -1:
    for bank in root.l2:
        bank.detailed_sim_start_tick = cacheProfileStart

# set up memory bus and memory controller
root.toMemBus = ConventionalMemBus()
root.toMemBus.adaptive_mha = root.adaptiveMHA

if env["MEMORY-BUS-SCHEDULER"] == "RDFCFS":
    root.toMemBus.memory_controller = ReadyFirstMemoryController()
elif env["MEMORY-BUS-SCHEDULER"] == "FCFS":
    root.toMemBus.memory_controller = InOrderMemoryController()
elif env["MEMORY-BUS-SCHEDULER"] == "NFQ":
    root.toMemBus.memory_controller = ThisNFQMemoryController()
else:
    # default is RDFCFS
    root.toMemBus.memory_controller = ReadyFirstMemoryController()

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
# Multi-programmed workloads with memory hog
###############################################################################

elif env['BENCHMARK'].startswith("hog"):
    tmpBM = env['BENCHMARK'].replace("hog","")
    prog = Spec2000.createWorkload(
               hog_workloads.hog_workloads[int(env['NP'])][int(tmpBM)][0])
    prog.append(TestPrograms.ThrashCache())


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
