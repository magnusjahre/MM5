from m5 import *
from BasePolicy import BasePolicy

class ModelThrottlingPolicy(BasePolicy):    
    type = 'ModelThrottlingPolicy'
    
    verify = Param.Bool("Do verification (quit after first sample)")
    staticArrivalRates = VectorParam.Float("Static arrival rates to enforce")
