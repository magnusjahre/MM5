from m5 import *
from BasePolicy import BasePolicy

class ImplementationStrategy(Enum): vals = ['nfq', 'throttle']

class ModelThrottlingPolicy(BasePolicy):    
    type = 'ModelThrottlingPolicy'
    
    verify = Param.Bool("Do verification (quit after first sample)")
    staticArrivalRates = VectorParam.Float("Static arrival rates to enforce")
    implStrategy = Param.ImplementationStrategy("The way to enforce the bandwidth quotas")