from m5 import *
from BasePolicy import BasePolicy

class ASMPolicy(BasePolicy):    
    type = 'ASMPolicy'
    epoch = Param.Int("Number of cycles in each epoch")
    allocateLLC  = Param.Bool("Do LLC allocation?")
    maximumSpeedup = Param.Float("Cap speedup at this value")
