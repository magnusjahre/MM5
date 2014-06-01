from m5 import *

class InterferenceProbabilityPolicy(Enum): vals = ['float', 'fixed', 'fixed-private', 'sequential']

class CacheInterference(SimObject):
    type = 'CacheInterference'

    cpuCount = Param.Int("Number of cores")
    leaderSets = Param.Int("Number of leader sets")
    size = Param.MemorySize("Cache capacity in bytes")
    interference_probability_policy = Param.InterferenceProbabilityPolicy("interference probability policy to use")
    ipp_bits= Param.Int("The resolution of the probability (used in a subset of IPP modes)")
    interferenceManager= Param.InterferenceManager("Pointer to the interference manager")
    blockSize= Param.Int("Cache block size")
    assoc= Param.Int("Associativity")
    hitLatency = Param.Latency("The cache hit latency")
    divisionFactor = Param.Int("The number of cores in shared mode when run in private mode")
    constituencyFactor = Param.Float("The average percentage of blocks accessed in a constituency")
