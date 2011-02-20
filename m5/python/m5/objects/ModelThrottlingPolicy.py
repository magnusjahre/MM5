from m5 import *
from BasePolicy import BasePolicy

class ModelThrottlingPolicy(BasePolicy):    
    type = 'ModelThrottlingPolicy'
    
    verify = Param.Bool("Do verification (quit after first sample)")
