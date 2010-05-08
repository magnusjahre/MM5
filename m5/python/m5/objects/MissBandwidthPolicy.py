from m5 import *
from BasePolicy import BasePolicy

class RequestEstimationMethod(Enum): vals = ['MWS', 'MLP']
class SearchAlgorithm(Enum): vals = ['exhaustive', 'bus-sorted', 'bus-sorted-log']

class MissBandwidthPolicy(BasePolicy):    
    type = 'MissBandwidthPolicy'
    
    busUtilizationThreshold = Param.Float("The actual bus utilzation to consider the bus as full")
    requestCountThreshold = Param.Float("The request intensity (requests / tick) to assume no request increase")
    requestEstimationMethod = Param.RequestEstimationMethod("The request estimation method to use")
    acceptanceThreshold = Param.Float("The performance improvement needed to accept new MHA")
    requestVariationThreshold = Param.Float("Maximum acceptable request variation")
    renewMeasurementsThreshold = Param.Int("Samples to keep MHA")
    searchAlgorithm = Param.SearchAlgorithm("The search algorithm to use")
    busRequestThreshold = Param.Float("The bus request intensity necessary to consider request increases")
