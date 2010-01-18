from m5 import *

class RequestEstimationMethod(Enum): vals = ['MWS', 'MLP']
class PerformanceEstimationMethod(Enum): vals = ['latency-mlp', 'ratio-mws', "latency-mlp-sreq", "no-mlp"]
class SearchAlgorithm(Enum): vals = ['exhaustive', 'bus-sorted', 'bus-sorted-log']

class MissBandwidthPolicy(SimObject):
    type = 'MissBandwidthPolicy'
    abstract = True
    period = Param.Tick("The number of clock cycles between each decision")
    interferenceManager = Param.InterferenceManager("The system's interference manager")
    cpuCount = Param.Int("The number of cores in the system")
    persistentAllocations = Param.Bool("Use persistent allocations")
    renewMeasurementsThreshold = Param.Int("Samples to keep MHA")
    
    requestEstimationMethod = Param.RequestEstimationMethod("The request estimation method to use")
    performanceEstimationMethod = Param.PerformanceEstimationMethod("The method to use for performance estimations")
    searchAlgorithm = Param.SearchAlgorithm("The search algorithm to use")
    iterationLatency = Param.Int("The number of cycles it takes to evaluate one MHA")
    
    busUtilizationThreshold = Param.Float("The actual bus utilzation to consider the bus as full")
    requestCountThreshold = Param.Float("The request intensity (requests / tick) to assume no request increase")
    acceptanceThreshold = Param.Float("The performance improvement needed to accept new MHA")
    requestVariationThreshold = Param.Float("Maximum acceptable request variation")
    useBusAccessInLatPred = Param.Bool("Use bus accesses in latency prediciton")
    busRequestThreshold = Param.Float("The bus request intensity necessary to consider request increases")