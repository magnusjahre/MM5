import m5

# specify base part of all object file names
file_bases = ['AdaptiveMHA',
              'AlphaConsole',
              'AlphaFullCPU',
              'AlphaTLB',
              'ASMPolicy', #Magnus
              'BadDevice',
              'BaseCPU',
              'BaseCache',
              'BaseHier',
              'BaseMem',
              'BaseMemory',
              'BasePolicy',
              'Butterfly', # Magnus
              'BranchPred',
              'Bus',
              'BusBridge',
              'CacheInterference',
              'CachePartitioning',
              'CoherenceProtocol',
              'ControllerInterference',
              'Crossbar', # Magnus
              'Debug',
              'Device',
              'DiskImage',
              'DuBoisInterference',
              'EqualizeSlowdownPolicy', #Magnus
              'Ethernet',
              'ExeTrace',
              'FastCPU',
              'FCFSInterference',
              'FCFSMemoryController',
              'FetchTrace',
              'FixedBandwidthMemoryController',
              'FreebsdSystem',
              'FullCPU',
              'FunctionalMemory',
              'HierParams',
              'Ide',
              'IdealInterconnect', #Magnus
              'Interconnect', #Magnus
              'InterconnectProfile', #Magnus
              'InterferenceManager',
              'IntervalStats',
              'IntrControl',
              'ITCA',
              'LinuxSystem',
              'MainMemory',
              'MemTest',
              'MemoryController',
              'MemoryTrace',
              'MemoryOverlapEstimator',
              'MemoryOverlapTable',
              'MissBandwidthPolicy',
              'ModelThrottlingPolicy',
              'MultipleTimeSharingPartitions',
              'NFQMemoryController',
              'NoBandwidthPolicy',
              'Pci',
              'PeerToPeerLink',
              'PhysicalMemory',
              'PipeTrace',
              'Platform',
              'Process',
              'PseudoInst',
              'PerformanceDirectedPolicy',
              'Random',
              'Repl',
              'RDFCFSInterference',
              'RDFCFSMemoryController',
              'Ring',
              'Root',
              'Sampler',
              'Scsi',
              'Serialize',
              'SimConsole',
              'SimpleCPU',
              'SimpleDisk',
              'SplitTransBus',
              'Statistics',
              'System',
              'ThrottleControl',
              'TimingMemoryController',
              'Trace',
              'TrafficGenerator',
              'Tru64System',
              'Tsunami',
              'Uart',
              'UtilityBasedPartitioning']

if m5.build_env['ALPHA_TLASER']:
    file_bases += [ 'DmaEngine' ]
    file_bases += [ 'Turbolaser' ]


# actual file names end in ".py"
file_names = [f + ".py" for f in file_bases]

# import all specified files
for f in file_bases:
    exec "from %s import *" % f

# only export actual SimObject classes when "from objects import *" is used.
g = globals()
__all__ = []
for sym,val in g.items():
    if isinstance(val, type) and issubclass(val, (SimObject, ParamContext)):
        __all__.append(sym)
