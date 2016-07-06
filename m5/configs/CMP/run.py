from m5 import *
import TestPrograms
import Spec2000
import Spec2006
import workloads
import hog_workloads
import bw_workloads
import deterministic_fw_wls as fair_workloads
import single_core_fw as single_core
import simpoints
import os
import shutil
import pickle
import subprocess
from DetailedConfig import *

from resourcePartition import ResourcePartition
from workloadcls import Workload

simrootdir = os.getenv("SIMROOT")
if simrootdir == None:
  panic("Envirionment variable SIMROOT not set. Quitting..")

###############################################################################
# Constants
###############################################################################

L2_BANK_COUNT = 4
all_protocols = ['none', 'msi', 'mesi', 'mosi', 'moesi', 'stenstrom']
snoop_protocols = ['msi', 'mesi', 'mosi', 'moesi']
directory_protocols = ['stenstrom']

miss_bw_policies = ["fairness", "hmos", "none", "stp", "aggregateIPC"]

partitioningSchemes = ["Conventional", "StaticUniform", "MTP", "UCP"]

FW_NOT_USED_SIZE = 100*10**12
SIM_TICKS_NOT_USED_SIZE = 20*10**9
SIM_TICKS_NOT_USED_SIZE_SMALL = 1000

###############################################################################
# Convenience Methods
###############################################################################

def initTypedWorkload(name):
    infile = open(simrootdir+"/m5/configs/CMP/typewls.pkl")
    typedwls = pickle.load(infile)
    infile.close()
    
    np = int(env["NP"])
    
    if np not in typedwls.keys():
        panic("No typed workloads for requested CPU count "+str(np))
    
    bmname = env['BENCHMARK'].split("-")
    if len(bmname) != 3:
        panic("Malformed typed workload "+env['BENCHMARK'])
    
    type = bmname[1]
    if type not in typedwls[np]:
        panic("Unknown type "+type+" in workload "+env['BENCHMARK'])
    
    id = int(bmname[2])
    if id < 0 or id >= len(typedwls[np][type]):
        panic("Unknown workload id "+str(id)+" in workload "+env['BENCHMARK'])
    
    return createWorkload(typedwls[np][type][id].benchmarks)
    
def createWorkload(workload):
    simwl = []
    
    idcnt = 0
    for b in workload:
        if b.startswith("s6-"):
            bm = Spec2006.parseBenchmarkString(b)
        else:
            if b[-1] == '0':
                b = b[0:len(b)-1]
            bm = Spec2000.parseBenchmarkString(b)
        if bm == None:
            panic("Unknown benchmark "+b+" in workload")
            
        bm.cpuID = idcnt
        simwl.append(bm)
        idcnt += 1
    
    return simwl

def setNFQParams(useTrafficGenerator, controllerID, optPart):
    if useTrafficGenerator:
        length = int(env["NP"])+2
    else:
        length = int(env["NP"])+1
        
    root.membus[controllerID].memory_controller.num_cpus = int(env["NP"])
    
    priorities = [1.0 / float(length) for i in range(length)]
    
    if optPart != None:
        if len(optPart.utils) != length:
            panic("The number of partitions does not match the number of CPUs")
        priorities = optPart.utils
    else:
        if "NFQ-PRIORITIES" in env:
            pris = env["NFQ-PRIORITIES"].split(",")
            
            if len(pris) != int(env["NP"])+1:
                panic("You need to provide NFQ priorities for both all cores and shared writebacks (and the traffic generator if available)")
                
            for i in range(length):
                priorities[i] = float(pris[i])
        
    if sum(priorities) != 1.0:
        panic("The provided NFQ priorities must add up to 1")
    
    root.membus[controllerID].memory_controller.priorities = priorities

def createMemBus(bankcnt, optPart, useMissBWPolicy):
    assert 'MEMORY-BUS-CHANNELS' in env
    
    channels = int(env['MEMORY-BUS-CHANNELS'])
    
    assert bankcnt >= channels
    banksPerBus = bankcnt / channels
    
    root.membus = [ConventionalMemBus() for i in range(channels)]
    root.ram = [SDRAM(in_bus=root.membus[i]) for i in range(channels)]
    
    useTrafficGenerator = False
    if "GENERATE-BACKGROUND-TRAFFIC" in env:
        useTrafficGenerator = True
        root.trafficGenerators = [TrafficGenerator(membus=root.membus[i],use_id=int(env["NP"])) for i in range(channels)]
    
    for i in range(channels):
        
        if env["MEMORY-BUS-SCHEDULER"] == "RDFCFS":
            
            root.membus[i].memory_controller = ReadyFirstMemoryController()
            
            if "MEMORY-BUS-PAGE-POLICY" in env:
                root.membus[i].memory_controller.page_policy = env["MEMORY-BUS-PAGE-POLICY"]
            if "MEMORY-BUS-PRIORITY-SCHEME" in env:
                root.membus[i].memory_controller.priority_scheme = env["MEMORY-BUS-PRIORITY-SCHEME"]
        
        elif env["MEMORY-BUS-SCHEDULER"] == "TNFQ":
            root.membus[i].memory_controller = ThroughputNFQMemoryController()
            setNFQParams(useTrafficGenerator, i, optPart)
            
        elif env["MEMORY-BUS-SCHEDULER"] == "FNFQ":
            root.membus[i].memory_controller = FairNFQMemoryController()
            setNFQParams(useTrafficGenerator, i, optPart)
            
        elif env["MEMORY-BUS-SCHEDULER"] == "FCFS":
            root.membus[i].memory_controller = InOrderMemoryController()
        
        elif env["MEMORY-BUS-SCHEDULER"] == "FBW":
            root.membus[i].memory_controller = FixedBandwidthController()
            
            if "FBW-STARVATION-THRESHOLD" in env:
                root.membus[i].memory_controller.starvation_threshold = int(env["FBW-STARVATION-THRESHOLD"])
                
            if "FBW-READY-THRESHOLD" in env:
                root.membus[i].memory_controller.ready_threshold = int(env["FBW-READY-THRESHOLD"])
            
        else:
            panic("Unkown memory bus scheduler")
            
        root.membus[i].adaptive_mha = root.adaptiveMHA
        root.membus[i].interference_manager = root.interferenceManager
        
        if useMissBWPolicy:
            root.membus[i].policy = root.globalPolicy    
        
        if "MEMORY-BUS-MAX-UTIL" in env:
            root.membus[i].utilization_limit = float(env["MEMORY-BUS-MAX-UTIL"])
    
    if int(env['NP']) > 1:
        if env["MEMORY-BUS-INTERFERENCE-SCHEME"] == "DIEF": 
        
            root.controllerInterference = [RDFCFSControllerInterference(memory_controller=root.membus[i].memory_controller) for i in range(channels)]
            for i in range(channels):
                if "READY-FIRST-LIMIT-ALL-CPUS" in env:
                    root.controllerInterference[i].rf_limit_all_cpus = int(env["READY-FIRST-LIMIT-ALL-CPUS"])
                if "CONTROLLER-INTERFERENCE-BUFFER-SIZE" in env:
                    root.controllerInterference[i].buffer_size = int(env["CONTROLLER-INTERFERENCE-BUFFER-SIZE"])
                if "USE-AVERAGE-ALONE-LATENCIES" in env:
                    assert env["USE-AVERAGE-ALONE-LATENCIES"] == "F" or env["USE-AVERAGE-ALONE-LATENCIES"] == "T"
                    if env["USE-AVERAGE-ALONE-LATENCIES"] == "T":
                        root.controllerInterference[i].use_average_lats = True
                    else:
                        root.controllerInterference[i].use_average_lats = False
                if "USE-PURE-HEAD-POINTER-MODEL" in env:
                    assert env["USE-PURE-HEAD-POINTER-MODEL"] == "T" or env["USE-PURE-HEAD-POINTER-MODEL"] == "F"
                    if env["USE-PURE-HEAD-POINTER-MODEL"] == "T":
                        root.controllerInterference[i].pure_head_pointer_model = True
                    else:
                        root.controllerInterference[i].pure_head_pointer_model = False
        
            for i in range(channels):
                root.controllerInterference[i].cpu_count = int(env['NP'])
        
        elif env["MEMORY-BUS-INTERFERENCE-SCHEME"] == "DuBois":
            root.controllerInterference = [DuBoisInterference(memory_controller=root.membus[i].memory_controller) for i in range(channels)]
            
            for i in range(channels):
                if "DUBOIS-USE-ORA" in env:
                    root.controllerInterference[i].use_ora = env["DUBOIS-USE-ORA"]
            
            for i in range(channels):
                root.controllerInterference[i].cpu_count = int(env['NP'])            
        else:
            fatal("MEMORY-BUS-INTERFERENCE-SCHEME must be either DIEF or DuBois")  
        
def setUpCachePartitioning():
    
    if env["CACHE-PARTITIONING"] not in partitioningSchemes:
        panic("Cache partitioning scheme can be "+str(partitioningSchemes))
    
    if env["CACHE-PARTITIONING"] == "Conventional":
        return
    
    if env["CACHE-PARTITIONING"] == "StaticUniform":
        for bank in root.SharedCache:
            bank.static_cache_quotas = [bank.assoc / int(env["NP"]) for i in range(int(env["NP"]))]
        return   
    
    if env["CACHE-PARTITIONING"] == "MTP":
        root.cachePartitioning = MultipleTimeSharingPartitions()
    elif env["CACHE-PARTITIONING"] == "UCP":
        root.cachePartitioning = UtilityBasedPartitioning()
        
        assert "CACHE-PARTITIONING-SEARCH-ALG" in env
        root.cachePartitioning.searchAlgorithm = env["CACHE-PARTITIONING-SEARCH-ALG"]
    else:
        panic("Unknown cache partitioning scheme")         
  
    root.cachePartitioning.associativity = root.SharedCache[0].assoc
    root.cachePartitioning.np = int(env["NP"])
    root.cachePartitioning.cache_interference = root.cacheInterference
        
    if "CACHE-PARTITIONING-EPOCH-SIZE" in env:
        root.cachePartitioning.epoch_size = int(env["CACHE-PARTITIONING-EPOCH-SIZE"])
  
    for bank in root.SharedCache:
        bank.partitioning = root.cachePartitioning
        

def createCacheInterference(bank):
    cacheInt = CacheInterference()
    
    cacheInt.size = str(L2_BANK_COUNT * bank.size)+"B"
    cacheInt.assoc = bank.assoc
    cacheInt.blockSize = bank.block_size
    cacheInt.hitLatency = bank.latency
    
    cacheInt.cpuCount = int(env['NP']) 
    cacheInt.interferenceManager = root.interferenceManager
    
    if int(env['NP']) == 1:
        assert "MEMORY-ADDRESS-PARTS" in env
        cacheInt.divisionFactor = int(env["MEMORY-ADDRESS-PARTS"])
    else:
        cacheInt.divisionFactor = int(env["NP"])

    if "SHADOW-TAG-LEADER-SETS" in env:
        cacheInt.leaderSets = int(env["SHADOW-TAG-LEADER-SETS"]) 

    if "SHADOW-TAG-CONSTITUENCY-FACTOR" in env:
        cacheInt.constituencyFactor = float(env["SHADOW-TAG-CONSTITUENCY-FACTOR"]) 

    if "LOAD-IPP" in env:
        cacheInt.load_ipp = env["LOAD-IPP"]
    
    if "WRITEBACK-IPP" in env:
        cacheInt.writeback_ipp = env["WRITEBACK-IPP"]
            
    return cacheInt

def initSharedCache(bankcnt, optPart):
    
    if "SHARED-CACHE-BANK-SIZE" in env:
        banksize = env["SHARED-CACHE-BANK-SIZE"]+"kB"
    else:
        banksize = None
    
    if int(env['NP']) == 4 or int(env['NP']) == 2:
        if banksize == None:
            banksize = str(2*(2**10))+"kB"        
        root.SharedCache = [SharedCache8M(size=banksize) for i in range(bankcnt)]
    elif int(env['NP']) == 8:
        if banksize == None:
            banksize = str(4*(2**10))+"kB"
        root.SharedCache = [SharedCache16M(size=banksize) for i in range(bankcnt)]
    elif int(env['NP']) == 16:
        if banksize == None:
            banksize = str(8*(2**10))+"kB"
        root.SharedCache = [SharedCache32M(size=banksize) for i in range(bankcnt)]
    elif int(env['NP']) == 1:
        assert 'MEMORY-ADDRESS-PARTS' in env
        if int(env['MEMORY-ADDRESS-PARTS']) == 4 or int(env['MEMORY-ADDRESS-PARTS']) == 2:
            if banksize == None:
                banksize = str(2*(2**10))+"kB"
            root.SharedCache = [SharedCache8M(size=banksize) for i in range(bankcnt)]
        elif int(env['MEMORY-ADDRESS-PARTS']) == 8:
            if banksize == None:
                banksize = str(4*(2**10))+"kB"
            root.SharedCache = [SharedCache16M(size=banksize) for i in range(bankcnt)]
        elif int(env['MEMORY-ADDRESS-PARTS']) == 16:
            if banksize == None:
                banksize = str(8*(2**10))+"kB"
            root.SharedCache = [SharedCache32M(size=banksize) for i in range(bankcnt)]
        else:
            panic("Shared Cache: No single cache configuration present")
    else:
        panic("No cache defined for selected CPU count")
        
    root.cacheInterference = createCacheInterference(root.SharedCache[0]) 
    
    if optPart != None:
        for bank in root.SharedCache:
            bank.static_cache_quotas = optPart.ways
    else:
        if "CACHE-PARTITIONING" in env:
            setUpCachePartitioning()
                
    if "WRITEBACK-OWNER-POLICY" in env:
        for bank in root.SharedCache:
            bank.writeback_owner_policy = env["WRITEBACK-OWNER-POLICY"]
   
    if "MAX-CACHE-WAYS" in env:
        if int(env["NP"]) != 1:
            panic("-EMAX-CACHE-WAYS only makes sense for single core experiments")
        for b in root.SharedCache:
            b.max_use_ways = int(env["MAX-CACHE-WAYS"])
    
    for bank in root.SharedCache:
        bank.interference_manager = root.interferenceManager
        bank.cache_interference = root.cacheInterference
        
        
   
def setUpSharedCache(bankcnt, detailedStartTick, useMissBWPolicy):
    
    root.sharedCacheThrottle = ThrottleControl()
    root.sharedCacheThrottle.cpu_count = int(env['NP'])
    root.sharedCacheThrottle.cache_type = "shared"
    if "DO-ARRIVAL-RATE-TRACE" in env:
        root.sharedCacheThrottle.do_arrival_rate_trace = bool(env["DO-ARRIVAL-RATE-TRACE"])
    if "CACHE-THROTLING-POLICY" in env:
        root.sharedCacheThrottle.throttling_policy = env["CACHE-THROTLING-POLICY"]
    
    if useMissBWPolicy:
        root.globalPolicy.sharedCacheThrottle = root.sharedCacheThrottle
    
    assert 'MEMORY-BUS-CHANNELS' in env
    assert bankcnt >= int(env['MEMORY-BUS-CHANNELS'])
    banksPerBus = bankcnt / int(env['MEMORY-BUS-CHANNELS'])
    
    curbus = 0
    buscnt = 0
    for i in range(bankcnt):
        root.SharedCache[i].in_interconnect = root.interconnect
        root.SharedCache[i].out_bus = root.membus[curbus]
        root.SharedCache[i].bank_count = bankcnt
        root.SharedCache[i].bank_id = i
        root.SharedCache[i].adaptive_mha = root.adaptiveMHA
        root.SharedCache[i].do_modulo_addr = True
        root.SharedCache[i].interference_manager = root.interferenceManager
        root.SharedCache[i].throttle_control = root.sharedCacheThrottle
        
        if useMissBWPolicy:
            root.SharedCache[i].miss_bandwidth_policy = root.globalPolicy
        
        buscnt += 1
        if buscnt % banksPerBus == 0:
            curbus += 1

def setUpModThrotPolicy():
    policy = ModelThrottlingPolicy()
    optionName = 'MODEL-THROTLING-POLICY' 
    if env["NP"] > 1:
        if env[optionName] == "none":
            policy.enforcePolicy = False
        else:
            policy.optimizationMetric = env[optionName]
            policy.enforcePolicy = True 
    else:
        policy.enforcePolicy = False

    assert "MODEL-THROTLING-POLICY-PERIOD" in env
    policy.period = int(env["MODEL-THROTLING-POLICY-PERIOD"])        
    policy.interferenceManager = root.interferenceManager
    policy.cpuCount = int(env["NP"])
    policy.performanceEstimationMethod = "no-mlp"
    
    assert "MODEL-THROTLING-IMPL-STRAT" in env
    policy.implStrategy = env["MODEL-THROTLING-IMPL-STRAT"]
    
    if int(env["NP"]) > 1: 
        assert "WRITE-STALL-TECH" in env
        policy.writeStallTechnique = env["WRITE-STALL-TECH"]
    else:
        policy.writeStallTechnique = "ws-none"
        
    if int(env["NP"]) > 1: 
        assert "EROB-STALL-TECH" in env
        policy.emptyROBStallTechnique = env["EROB-STALL-TECH"]
    else:
        policy.emptyROBStallTechnique = "rst-none"
        
    if int(env["NP"]) > 1: 
        assert "PB-STALL-TECH" in env
        policy.privateBlockedStallTechnique = env["PB-STALL-TECH"]
    else:
        policy.privateBlockedStallTechnique = "pbs-none"
    
    statOptName = "MODEL-THROTLING-POLICY-STATIC" 
    if statOptName in env:
        tmpArrList = env[statOptName].split(',')
        if len(tmpArrList) != int(env['NP']) and len(tmpArrList) != int(env['NP'])+1:
            panic("Length of static policy list must be equal to the number of cores, list is "+str(env[statOptName]))
        
        arrList = [float(tmpArrList[i]) for i in range(int(env['NP']))]
        policy.staticArrivalRates=tmpArrList
    
    return policy

def setUpEqualizeSlowdownPolicy():
    policy = EqualizeSlowdownPolicy()
    optionName = 'EQUAL-SD-' 
    if env[optionName+"POLICY"] == "off":
        policy.enforcePolicy = False
    else:
        assert env[optionName+"POLICY"] ==  "on"
        policy.enforcePolicy = True 

    assert optionName+"PERIOD" in env
    policy.period = int(env[optionName+"PERIOD"])        
    policy.interferenceManager = root.interferenceManager
    policy.cpuCount = int(env["NP"])
    
    assert optionName+"PERF-METHOD" in env
    policy.performanceEstimationMethod = env[optionName+"PERF-METHOD"]
    
    assert optionName+"OPTIM-METRIC" in env
    policy.optimizationMetric = env[optionName+"OPTIM-METRIC"]
    
    assert optionName+"SEARCH-ALG" in env
    policy.searchAlgorithm = env[optionName+"SEARCH-ALG"]
    
    if optionName+"ALLOW-NEG-MISSES":
        policy.allowNegativeMisses = env[optionName+"ALLOW-NEG-MISSES"]
    
    if int(env["NP"]) > 1: 
        assert "WRITE-STALL-TECH" in env
        policy.writeStallTechnique = env["WRITE-STALL-TECH"]
    else:
        policy.writeStallTechnique = "ws-none"
        
    if int(env["NP"]) > 1: 
        assert "EROB-STALL-TECH" in env
        policy.emptyROBStallTechnique = env["EROB-STALL-TECH"]
    else:
        policy.emptyROBStallTechnique = "rst-none"
        
    if int(env["NP"]) > 1: 
        assert "PB-STALL-TECH" in env
        policy.privateBlockedStallTechnique = env["PB-STALL-TECH"]
    else:
        policy.privateBlockedStallTechnique = "pbs-none"
    
    return policy

def setUpPerfDirPolicy():
    perfDirPolicy = PerformanceDirectedPolicy()
    optionName = 'PERFORMANCE-DIR-POLICY' 
    if env["NP"] > 1:
        if env[optionName] == "none":
            perfDirPolicy.enforcePolicy = False
        else:
            perfDirPolicy.optimizationMetric = env[optionName]
            perfDirPolicy.enforcePolicy = True 
    else:
        perfDirPolicy.enforcePolicy = False

    assert "PERFORMANCE-DIR-POLICY-PERIOD" in env
    perfDirPolicy.period = int(env["PERFORMANCE-DIR-POLICY-PERIOD"])        
    perfDirPolicy.interferenceManager = root.interferenceManager
    perfDirPolicy.cpuCount = int(env["NP"])
    perfDirPolicy.performanceEstimationMethod = "no-mlp"
    
    assert "WRITE-STALL-TECH" in env
    perfDirPolicy.writeStallTechnique = env["WRITE-STALL-TECH"]
    
    return perfDirPolicy

def setUpMissBwPolicy():
    if env['MISS-BW-POLICY'] not in miss_bw_policies:
        panic("Miss bandwidth policy "+str(env['MISS-BW-POLICY'])+" is invalid. Available policies: "+str(miss_bw_policies))
    
    missBandwidthPolicy = MissBandwidthPolicy()
    if env["NP"] > 1:
        if env['MISS-BW-POLICY'] == "none":
            missBandwidthPolicy.enforcePolicy = False
        else:
            missBandwidthPolicy.optimizationMetric = env['MISS-BW-POLICY']
            missBandwidthPolicy.enforcePolicy = True 
    else:
        if env['MISS-BW-POLICY'] != "none":
            warn("NP is 1 and MISS-BW-POLICY != none, ignoring policy argument!")
        missBandwidthPolicy.enforcePolicy = False

    assert "MISS-BW-POLICY-PERIOD" in env
    missBandwidthPolicy.period = int(env["MISS-BW-POLICY-PERIOD"])        
    missBandwidthPolicy.interferenceManager = root.interferenceManager
    missBandwidthPolicy.cpuCount = int(env["NP"])

    if int(env["NP"]) > 1: 
        assert "WRITE-STALL-TECH" in env
        missBandwidthPolicy.writeStallTechnique = env["WRITE-STALL-TECH"]
    else:
        missBandwidthPolicy.writeStallTechnique = "ws-none"
        
    if int(env["NP"]) > 1: 
        assert "EROB-STALL-TECH" in env
        missBandwidthPolicy.emptyROBStallTechnique = env["EROB-STALL-TECH"]
    else:
        missBandwidthPolicy.emptyROBStallTechnique = "rst-none"
        
    if int(env["NP"]) > 1: 
        assert "PB-STALL-TECH" in env
        missBandwidthPolicy.privateBlockedStallTechnique = env["PB-STALL-TECH"]
    else:
        missBandwidthPolicy.privateBlockedStallTechnique = "pbs-none"
        
    if "MISS-BW-MAX-DAMP" in env:
        missBandwidthPolicy.maximumDamping = float(env["MISS-BW-MAX-DAMP"])
    
    if "MISS-BW-HYBRID-BUFFER-SIZE" in env:
        missBandwidthPolicy.hybridBufferSize = int(env["MISS-BW-HYBRID-BUFFER-SIZE"])
    
    if "MISS-BW-HYBRID-DEC-ERROR" in env:
        missBandwidthPolicy.hybridDecisionError = float(env["MISS-BW-HYBRID-DEC-ERROR"])
        
    if "MISS-BW-REQ-METHOD" in env:
        missBandwidthPolicy.requestEstimationMethod = env["MISS-BW-REQ-METHOD"]
    else:
        missBandwidthPolicy.requestEstimationMethod = "MWS"

    if "MISS-BW-PERF-METHOD" in env:
        missBandwidthPolicy.performanceEstimationMethod = env["MISS-BW-PERF-METHOD"]
    else:
        missBandwidthPolicy.performanceEstimationMethod = "ratio-mws"

    if "MISS-BW-PERSISTENT" in env:
        missBandwidthPolicy.persistentAllocations = bool(env["MISS-BW-PERSISTENT"])

    if "MISS-BW-RENEW-THRESHOLD" in env:
        missBandwidthPolicy.renewMeasurementsThreshold = int(env["MISS-BW-RENEW-THRESHOLD"])

    if "MISS-BW-SEARCH-ALG" in env:
        missBandwidthPolicy.searchAlgorithm = env["MISS-BW-SEARCH-ALG"]

    if "MISS-BW-IT-LAT" in env:
        missBandwidthPolicy.iterationLatency = env["MISS-BW-IT-LAT"]

    if "MISS-BW-ACCEPTANCE-THRESHOLD" in env:
        missBandwidthPolicy.acceptanceThreshold = float(env["MISS-BW-ACCEPTANCE-THRESHOLD"])

    if "MISS-BW-BUS-UTIL-THRESHOLD" in env:
        missBandwidthPolicy.busUtilizationThreshold = float(env["MISS-BW-BUS-UTIL-THRESHOLD"])
    
    if "MISS-BW-REQ-INTENSITY-THRESHOLD" in env:
        missBandwidthPolicy.requestCountThreshold = float(env["MISS-BW-REQ-INTENSITY-THRESHOLD"])
        
    if "MISS-BW-REQ-VARIATION-THRESHOLD" in env:
        missBandwidthPolicy.requestVariationThreshold = float(env["MISS-BW-REQ-VARIATION-THRESHOLD"])
    
    if "MISS-BW-BUS-REQ-INTENSITY-THRESHOLD" in env:
        missBandwidthPolicy.busRequestThreshold = float(env["MISS-BW-BUS-REQ-INTENSITY-THRESHOLD"])
    
    return missBandwidthPolicy

def getCheckpointDirectory(simpoint = -1):

    if env["NP"] > 1:
        npName = env["NP"]
    else:
        npName = env["MEMORY-ADDRESS-PARTS"]
    serializeBase = "cpt-"+npName+"-"+env["MEMORY-SYSTEM"]+"-"+env["BENCHMARK"]
    if simpoint != -1:
        return serializeBase+"-sp"+str(simpoint)
    return serializeBase+"-nosp"

def setGenerateCheckpointParams(checkpointAt, simpoint = -1):
    assert env["NP"] == 1
        
    # Simulation will terminate when the checkpoint is dumped
    fwticks = FW_NOT_USED_SIZE
    simulateCycles = SIM_TICKS_NOT_USED_SIZE_SMALL
    
    assert len(root.simpleCPU) == 1
    assert "BENCHMARK" in env
    root.simpleCPU[0].checkpoint_at_instruction = checkpointAt
    
    Serialize.dir = getCheckpointDirectory(simpoint)
    
    return fwticks, simulateCycles

def copyCheckpointFiles(directory):
    checkpointfiles = os.listdir(directory)
    
    for name in checkpointfiles:
        if name != "m5.cpt" and name != "m5.cpt.old":
            
            if not os.path.isdir(directory+"/"+name):
                print >> sys.stderr, "Linking/copying file "+name+" to current directory"
                #shutil.copy(directory+"/"+name, ".")
                #shutil.copy(directory+"/"+name, name+".clean
                if name.startswith("diskpages"):
                    newname = name.replace("diskpages", "diskpages-cpt")
                    nozipname = newname.replace(".zip","")
                    if os.path.exists(name):
                        os.remove(name)
                    if os.path.exists(name.replace(".zip","")):
                        os.remove(name.replace(".zip",""))
                        
                    if name.endswith(".zip"):
                        print >> sys.stderr, "Uncompressing "+name
                        subprocess.call(["unzip", "-o", directory+"/"+name])
                        os.rename("diskpages0.bin", nozipname) 
                    else:
                        successful = False
                        cnt = 0 
                        while not successful and cnt < 5:
                            try:
                                shutil.copy(directory+"/"+name, newname)
                                successful = True
                            except IOError:
                                print >> sys.stderr, "Copy failed, retry number"+str(cnt)
                                os.remove(newname)
                            except:
                                raise Exception("Unknown copy error")
                            cnt += 1
                        if not successful:
                            raise Exception("Could not copy file "+str(newname)+", retried 5 times")
                    continue
                else:
                    if os.path.exists(name):
                        os.remove(name)
                    os.symlink(directory+"/"+name, name)
                
                cleanname = name+".clean"
                if os.path.exists(cleanname):
                    os.remove(cleanname)
                os.symlink(directory+"/"+name, cleanname)
            else:
                print >> sys.stderr, "Skipping directory "+name

def readOptimalPartition():
    
    if int(env["NP"]) == 1:
        warn("Resource partitioning does not make sense for single cores, assuming baseline")
        return None
    
    if not os.path.exists(env['OPTIMAL-PARTITION-FILE']):
        panic("File "+env['OPTIMAL-PARTITION-FILE']+" not found")
    pklfile = open(env['OPTIMAL-PARTITION-FILE'])
    tmpData = pickle.load(pklfile)
    
    optPartMetricOptName = "OPTIMAL-PARTITION-METRIC"
    if optPartMetricOptName not in env:
        panic("-E"+optPartMetricOptName+" is needed for optimal partitions")
        
    if env[optPartMetricOptName] not in tmpData:
        panic("Provided partitioning file does not contain partitions for metric "+env[optPartMetricOptName])
    
    if env["BENCHMARK"] not in tmpData[env[optPartMetricOptName]]:
        panic("There is no data for workload "+env["BENCHMARK"]+" and metric "+env[optPartMetricOptName]+" in the provided partition file")
        
    return tmpData[env[optPartMetricOptName]][env["BENCHMARK"]]

def setUpOverlapMeasurement():

    root.overlapTables = [MemoryOverlapTable() for i in  xrange(int(env['NP']))]
    root.ITCAs = [ITCA(cpu_id=i) for i in  xrange(int(env['NP']))]
    
    for ot in root.overlapTables:
        if "MOT-REQ-SIZE" in env: 
            ot.request_table_size = int(env["MOT-REQ-SIZE"])
        if "MOT-UL-SIZE" in env:
            ot.unknown_table_size = int(env["MOT-UL-SIZE"])

    root.overlapEstimators = [MemoryOverlapEstimator(id=i, interference_manager=root.interferenceManager, cpu_count=int(env['NP'])) for i in  xrange(int(env['NP']))]

    for i in range(int(env["NP"])):
        root.overlapEstimators[i].overlapTable = root.overlapTables[i]
        if "SHARED-STALL-IDENT" in env:
            root.overlapEstimators[i].shared_stall_heuristic = env["SHARED-STALL-IDENT"]
        if "CPL-TABLE-SIZE" in env:
            root.overlapEstimators[i].cpl_table_size = int(env["CPL-TABLE-SIZE"])
            
    for i in range(int(env["NP"])):
        root.overlapEstimators[i].itca = root.ITCAs[i]
        if "ITCA-CPU-STALL-POLICY" in env:
            root.ITCAs[i].cpu_stall_policy = env["ITCA-CPU-STALL-POLICY"]
        if "ITCA-ITIP" in env:
            root.ITCAs[i].itip = env["ITCA-ITIP"]
        if "ITCA-VERIFY" in env:
            root.ITCAs[i].do_verification = env["ITCA-VERIFY"]

def warn(message):
    print >> sys.stderr, "Warning: "+message

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
    -EBENCHMARK=fair01\n")

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

if "MEMORY-BUS-SCHEDULER" in env:
    if env["MEMORY-BUS-SCHEDULER"] == "FCFS" \
    or env["MEMORY-BUS-SCHEDULER"] == "RDFCFS" \
    or env["MEMORY-BUS-SCHEDULER"] == "FNFQ" \
    or env["MEMORY-BUS-SCHEDULER"] == "TNFQ" \
    or env["MEMORY-BUS-SCHEDULER"] == "FBW":
        pass
    else:
        panic("Only FCFS, RDFCFS, TNFQ, FNFQ and FBW memory bus schedulers are supported")

L2BankSize = -1
if "L2BANKSIZE" in env:
    L2BankSize = int(env["L2BANKSIZE"])
    
fairCrossbar = False
if "USE-FAIR-CROSSBAR" in env:
    fairCrossbar = True
    
if 'MEMORY-SYSTEM' not in env:
    panic("Specify which -EMEMORY-SYSTEM to use (Legacy, CrossbarBased or RingBased)")
    
###############################################################################
# Root
###############################################################################

HierParams.cpu_count = int(env['NP'])

root = DetailedStandAlone()
if progressInterval > 0:
    root.progress_interval = progressInterval

###############################################################################
# Adaptive MHA
###############################################################################

root.adaptiveMHA = AdaptiveMHA()
root.adaptiveMHA.cpuCount = int(env["NP"])

if 'AMHA-PERIOD' in env:
    root.adaptiveMHA.sampleFrequency = int(env['AMHA-PERIOD'])
else:
    root.adaptiveMHA.sampleFrequency = 500000

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
#  Interference Manager and Miss Bandwidth Policy
###############################################################################

root.interferenceManager = InterferenceManager()
root.interferenceManager.cpu_count = int(env["NP"])

if "INTERFERENCE-MANAGER-RESET-INTERVAL" in env:
    root.interferenceManager.reset_interval = int(env["INTERFERENCE-MANAGER-RESET-INTERVAL"])
if "INTERFERENCE-MANAGER-SAMPLE-SIZE" in env:
    root.interferenceManager.sample_size = int(env["INTERFERENCE-MANAGER-SAMPLE-SIZE"])

useMissBWPolicy = False
if 'MISS-BW-POLICY' in env:
    root.globalPolicy = setUpMissBwPolicy()
    useMissBWPolicy = True

if 'PERFORMANCE-DIR-POLICY' in env:
    root.globalPolicy = setUpPerfDirPolicy()
    
if 'MODEL-THROTLING-POLICY' in env:
    root.globalPolicy = setUpModThrotPolicy()
    useMissBWPolicy = True

if 'EQUAL-SD-POLICY' in env:
    root.globalPolicy = setUpEqualizeSlowdownPolicy()
    useMissBWPolicy = True

###############################################################################
#  CPUs and L1 caches
###############################################################################

sss = -1
if "SIMPOINT-SAMPLE-SIZE" in env:
    sss = int(env["SIMPOINT-SAMPLE-SIZE"])

setUpOverlapMeasurement()

BaseCPU.workload = Parent.workload
root.simpleCPU = [ CPU(defer_registration=True,simpoint_bbv_size=sss)
                   for i in xrange(int(env['NP'])) ]
root.detailedCPU = [ DetailedCPU(defer_registration=True,adaptiveMHA=root.adaptiveMHA,interferenceManager=root.interferenceManager,overlapEstimator=root.overlapEstimators[i]) for i in xrange(int(env['NP'])) ]

if useMissBWPolicy:
    for cpu in root.detailedCPU:
        cpu.basePolicy = root.globalPolicy

if 'COMMIT-TRACE-FREQUENCY' in env:
    for r in root.detailedCPU:
        r.commit_trace_frequency = int(env['COMMIT-TRACE-FREQUENCY'])

if 'COMMIT-TRACE-INSTRUCTION-FILE' in env:
    assert int(env['NP']) == 1, "Tracing on irregular instruction samples only makes sense for private mode experiments"
    f = open(env['COMMIT-TRACE-INSTRUCTION-FILE'])
    ifdata = f.read()
    try:
        ifinsts = [int(ifdata.split(",")[i]) for i in range(len(ifdata.split(",")))]
    except:
        assert False, "Commit trace instruction file parse error in file "+env['COMMIT-TRACE-INSTRUCTION-FILE']
    ifinsts.sort()
    r.commit_trace_instructions = ifinsts
else:
    for r in root.detailedCPU:
        r.commit_trace_instructions = []

if env['MEMORY-SYSTEM'] == "CrossbarBased":
    root.L1dcaches = [ DL1(out_interconnect=Parent.interconnect) for i in xrange(int(env['NP'])) ]
    root.L1icaches = [ IL1(out_interconnect=Parent.interconnect) for i in xrange(int(env['NP'])) ]
                        
    for l1 in root.L1dcaches:
        l1.adaptive_mha = root.adaptiveMHA
        l1.interference_manager = root.interferenceManager
        l1.miss_bandwidth_policy = root.globalPolicy
        
    for l1 in root.L1icaches:
        l1.adaptive_mha = root.adaptiveMHA
        l1.interference_manager = root.interferenceManager
    
else:
    assert env['MEMORY-SYSTEM'] == "RingBased"
    root.PointToPointLink = [PointToPointLink(interference_manager=root.interferenceManager) for i in range(int(env['NP']))]
    root.L1dcaches = [ DL1(out_interconnect=root.PointToPointLink[i], overlapEstimator=root.overlapEstimators[i]) for i in range(int(env['NP'])) ]
    root.L1icaches = [ IL1(out_interconnect=root.PointToPointLink[i], overlapEstimator=root.overlapEstimators[i]) for i in xrange(int(env['NP'])) ]
    

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

###############################################################################
# Checkpoints and Simulation Time
###############################################################################

generateCheckpoint = False
if "GENERATE-CHECKPOINT" in env:
    generateCheckpoint = True

useCheckpointPath = ""
if "USE-CHECKPOINT" in env:
    useCheckpointPath = env["USE-CHECKPOINT"]

simInsts = -1
if "SIMINSTS" in env:
    simInsts = int(env["SIMINSTS"])
    
quitOnCPUID = -1
if "QUIT-ON-CPUID" in env:
    quitOnCPUID = int(env["QUIT-ON-CPUID"])

if "USE-SIMPOINT" in env:
    
    if "FASTFORWARDTICKS" in env or "SIMULATETICKS" in env:
        panic("simulation length parameters does not make sense with simpoints")
    
    simpointNum = int(env["USE-SIMPOINT"])
    assert simpointNum < simpoints.maxk
    
    if generateCheckpoint:
        
        if env["NP"] > 1:
            fatal("CMP checkpoints are generated from individual benchmark checkpoints")
        
        if useCheckpointPath != "":
            
            warn("Using simpoint checkpoint to generate different checkpoint, ignoring simpoint settings!")
            
            cptdir = useCheckpointPath+"/"+getCheckpointDirectory(simpointNum)
            copyCheckpointFiles(cptdir)
            root.checkpoint = cptdir
            
            fwticks, simulateCycles = setGenerateCheckpointParams(simInsts)
        else:
            fwticks, simulateCycles = setGenerateCheckpointParams(simpoints.simpoints[env["BENCHMARK"]][simpointNum][simpoints.FWKEY], simpointNum)
    else:
        
        fwticks = 1
        simulateCycles = SIM_TICKS_NOT_USED_SIZE
        
        for cpu in root.detailedCPU:
            cpu.min_insts_all_cpus = simpoints.intervalsize
    
        if simInsts == -1:
            simInsts = simpoints.intervalsize
        else:
            warn("Simulate instructions set, ignoring simpoints default. Statistics will not be representable!")
            
        if useCheckpointPath == "":
            panic("Checkpoint path must be provided")
        
        checkpointDirPath = useCheckpointPath+"/"+getCheckpointDirectory(simpointNum)
        copyCheckpointFiles(checkpointDirPath)
        

        root.checkpoint = checkpointDirPath
        for cpu in root.detailedCPU:
            cpu.min_insts_all_cpus = simInsts
            cpu.quit_on_cpu_id = quitOnCPUID 
else:
    
    if "SIMULATETICKS" in env and simInsts != -1:
        panic("Specfying both a max tick count and a max inst count does not make sense")
    
    if generateCheckpoint:
        assert simInsts != -1            
        fwticks, simulateCycles = setGenerateCheckpointParams(simInsts)
    else:
        if useCheckpointPath != "":
            
            fwticks = 1
            cptdir = useCheckpointPath+"/"+getCheckpointDirectory()
            copyCheckpointFiles(cptdir)
            root.checkpoint = cptdir
            
        else:
            assert "FASTFORWARDTICKS" in env
            fwticks = int(env["FASTFORWARDTICKS"])
        
        assert ("SIMULATETICKS" in env or simInsts != -1)
        if simInsts == -1:
            simulateCycles = int(env["SIMULATETICKS"])
        else:
            simulateCycles = SIM_TICKS_NOT_USED_SIZE
            for cpu in root.detailedCPU:
                cpu.min_insts_all_cpus = simInsts 
                cpu.quit_on_cpu_id = quitOnCPUID
        
root.sampler = Sampler()
root.sampler.phase0_cpus = Parent.simpleCPU
root.sampler.phase1_cpus = Parent.detailedCPU
root.sampler.periods = [fwticks, simulateCycles]

root.adaptiveMHA.startTick = fwticks
uniformPartStart = fwticks
cacheProfileStart = fwticks
Bus.switch_at = fwticks
BaseCache.detailed_sim_start_tick = fwticks
        

###############################################################################
# Interconnect, L2 caches and Memory Bus
###############################################################################

BaseCache.multiprog_workload = True

fixedRoundtripLatency = -1
if 'FIXED-ROUNDTRIP-LATENCY' in env:
    fixedRoundtripLatency = int(env['FIXED-ROUNDTRIP-LATENCY'])

optPartData = None
if 'OPTIMAL-PARTITION-FILE' in env:
    optPartData = readOptimalPartition()

if env['MEMORY-SYSTEM'] == "Legacy":
    panic("Legacy memory system no longer supported")

elif env['MEMORY-SYSTEM'] == "CrossbarBased":

    bankcnt = 4
    createMemBus(bankcnt, optPartData, useMissBWPolicy)
    initSharedCache(bankcnt, optPartData)

    root.interconnect = InterconnectCrossbar()
    root.interconnect.cpu_count = int(env['NP'])
    root.interconnect.detailed_sim_start_tick = cacheProfileStart
    root.interconnect.shared_cache_writeback_buffers = root.SharedCache[0].write_buffers
    root.interconnect.shared_cache_mshrs = root.SharedCache[0].mshrs
    root.interconnect.adaptive_mha = root.adaptiveMHA
    root.interconnect.interference_manager = root.interferenceManager
    root.interconnect.fixed_roundtrip_latency = fixedRoundtripLatency

    setUpSharedCache(bankcnt, cacheProfileStart, useMissBWPolicy)

elif env['MEMORY-SYSTEM'] == "RingBased":

    bankcnt = 4
    createMemBus(bankcnt, optPartData, useMissBWPolicy)
    initSharedCache(bankcnt, optPartData)

    for link in root.PointToPointLink:
        link.detailed_sim_start_tick = cacheProfileStart
    
    root.interconnect = RingInterconnect()
    root.interconnect.adaptive_mha = root.adaptiveMHA
    root.interconnect.detailed_sim_start_tick = cacheProfileStart
    root.interconnect.interference_manager = root.interferenceManager
    root.interconnect.fixed_roundtrip_latency = fixedRoundtripLatency
    
    setUpSharedCache(bankcnt, cacheProfileStart, useMissBWPolicy)
    
    root.PrivateL2Cache = [PrivateCache1M() for i in range(env['NP'])]
    root.PrivateL2Throttles = [ThrottleControl(cpu_count=int(env['NP']), cache_cpu_id=i, cache_type="private") for i in range(env['NP'])]
    
    if useMissBWPolicy:
        root.globalPolicy.privateCacheThrottles = root.PrivateL2Throttles
    
    for i in range(env['NP']):
        root.PrivateL2Cache[i].in_interconnect = root.PointToPointLink[i]
        root.PrivateL2Cache[i].out_interconnect = root.interconnect
        root.PrivateL2Cache[i].cpu_id = i
        root.PrivateL2Cache[i].adaptive_mha = root.adaptiveMHA
        root.PrivateL2Cache[i].interference_manager = root.interferenceManager
        root.PrivateL2Cache[i].throttle_control = root.PrivateL2Throttles[i]
        
        if useMissBWPolicy:
            root.PrivateL2Cache[i].miss_bandwidth_policy = root.globalPolicy

        if int(env['NP']) == 1:
            root.PrivateL2Cache[i].memory_address_offset = int(env['MEMORY-ADDRESS-OFFSET'])
            root.PrivateL2Cache[i].memory_address_parts = int(env['MEMORY-ADDRESS-PARTS'])
            
        if "AGG-MSHR-MLP-EST" in env:
            root.PrivateL2Cache[i].use_aggregate_mlp_estimator = bool(env["AGG-MSHR-MLP-EST"])
            
        if "TARGET-REQUEST-RATE" in env:
            if(int(env["NP"]) != 1):
                fatal("TARGET-REQUEST-RATE option only makes sense for single core experiments")
            root.PrivateL2Cache[0].target_request_rate = int(env["TARGET-REQUEST-RATE"])
        
        if "DO-MSHR-TRACE" in env:
            root.PrivateL2Cache[i].do_mshr_trace = bool(env["DO-MSHR-TRACE"])
            
        if "DO-ARRIVAL-RATE-TRACE" in env:
            root.PrivateL2Throttles[i].do_arrival_rate_trace = bool(env["DO-ARRIVAL-RATE-TRACE"])
            
        if "CACHE-THROTLING-POLICY" in env:
            root.PrivateL2Throttles[i].throttling_policy = env["CACHE-THROTLING-POLICY"]
else:
    panic("MEMORY-SYSTEM parameter must be Legacy, CrossbarBased or RingBased")


###############################################################################
# Workloads
###############################################################################

if "PROCESS-MEM" in env:
    LiveProcess.maxMemMB = int(env["PROCESS-MEM"])

if "PROCESS-VE" in env:
    LiveProcess.victimEntries = int(env["PROCESS-VE"])

prog = []

if env['BENCHMARK'].startswith("fair"):
    tmpBM = env['BENCHMARK'].replace("fair","")
    prog = Spec2000.createWorkload(fair_workloads.workloads[int(env['NP'])][int(tmpBM)][0])

elif env['BENCHMARK'].startswith("t-"):
    prog = initTypedWorkload(env['BENCHMARK'])

elif env['BENCHMARK'] in single_core.configuration:
    prog.append(Spec2000.createWorkload([single_core.configuration[env['BENCHMARK']][0]]))

elif env['BENCHMARK'].startswith("s6"):
    prog = Spec2006.createWorkload([env['BENCHMARK']])

elif env['BENCHMARK'] == "hello":
    prog = [TestPrograms.HelloWorld(cpuID=i) for i in range(int(env["NP"]))]

else:
    panic("The BENCHMARK environment variable was set to something improper\n")

for i in range(int(env['NP'])):
    root.simpleCPU[i].workload = prog[i]
    root.detailedCPU[i].workload = prog[i]
    

###############################################################################
# Statistics
###############################################################################

root.stats = Statistics(text_file=env['STATSFILE'])
