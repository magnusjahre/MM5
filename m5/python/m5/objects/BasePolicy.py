from m5 import *

class PerformanceEstimationMethod(Enum): vals = ['latency-mlp', 'ratio-mws', "latency-mlp-sreq", "no-mlp", "no-mlp-cache", "cpl", "cpl-cwp", "cpl-cwp-ser", "bois"]
class OptimizationMetric(Enum): vals = ['hmos', 'stp', "fairness", "aggregateIPC"]
class WriteStallTechnique(Enum): vals = ['ws-none', 'ws-shared', 'ws-latency', 'ws-ratio']
class PrivateBlockedStallTechnique(Enum): vals = ['pbs-none', 'pbs-shared']
class EmptyROBStallTechnique(Enum): vals = ['rst-none', 'rst-shared', 'rst-ratio']

class BasePolicy(SimObject):
    type = 'BasePolicy'
    abstract = True
    
    interferenceManager = Param.InterferenceManager("The system's interference manager")   
    period = Param.Tick("The number of clock cycles between each decision")
    cpuCount = Param.Int("The number of cores in the system")
    performanceEstimationMethod = Param.PerformanceEstimationMethod("The method to use for performance estimations")
    persistentAllocations = Param.Bool("Use persistent allocations")
    iterationLatency = Param.Int("The number of cycles it takes to evaluate one MHA")
    optimizationMetric = Param.OptimizationMetric("The metric to optimize for")
    enforcePolicy = Param.Bool("Should the policy be enforced?")
    sharedCacheThrottle = Param.ThrottleControl("The shared cache throttle")
    privateCacheThrottles = VectorParam.ThrottleControl("The private cache throttles")
    writeStallTechnique = Param.WriteStallTechnique("The technique to use to estimate private write stalls")
    privateBlockedStallTechnique = Param.PrivateBlockedStallTechnique("The technique to use to estimate private blocked stalls")
    emptyROBStallTechnique = Param.EmptyROBStallTechnique("The technique to use to estimate private mode empty ROB stalls")
