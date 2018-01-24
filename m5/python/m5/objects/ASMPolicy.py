from m5 import *
from BasePolicy import BasePolicy

class ASMSubpolicy(Enum): vals = ['ASM-MEM', 'ASM-MEM-EQUAL', 'ASM-CACHE', 'ASM-CACHE-MEM']

class ASMPolicy(BasePolicy):    
    type = 'ASMPolicy'
    epoch = Param.Int("Number of cycles in each epoch")
    subpolicy = Param.ASMSubpolicy("Internal ASM policy to use")
    maximumSpeedup = Param.Float("Cap speedup at this value")
