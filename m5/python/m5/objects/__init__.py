import m5

# specify base part of all object file names
file_bases = ['AdaptiveMHA',
              'AlphaConsole',
              'AlphaFullCPU',
              'AlphaTLB',
              'BadDevice',
              'BaseCPU',
              'BaseCache',
              'BaseHier',
              'BaseMem',
              'BaseMemory',
              'Butterfly', # Magnus
              'BranchPred',
              'Bus',
              'BusBridge',
              'CoherenceProtocol',
              'Crossbar', # Magnus
              'Debug',
              'Device',
              'DiskImage',
              'Ethernet',
              'ExeTrace',
              'FastCPU',
              'FCFSMemoryController',
              'FetchTrace',
              'FreebsdSystem',
              'FullCPU',
              'FunctionalMemory',
              'HierParams',
              'Ide',
              'IdealInterconnect', #Magnus
              'Interconnect', #Magnus
              'InterconnectProfile', #Magnus
              'IntervalStats',
              'IntrControl',
              'LinuxSystem',
              'MainMemory',
              'MemTest',
              'MemoryController',
              'MemoryTrace',
              'NFQMemoryController',
              'Pci',
              'PeerToPeerLink',
              'PhysicalMemory',
              'PipeTrace',
              'Platform',
              'Process',
              'PseudoInst',
              'Random',
              'Repl',
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
              'TimingMemoryController',
              'Trace',
              'Tru64System',
              'Tsunami',
              'Uart']

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
