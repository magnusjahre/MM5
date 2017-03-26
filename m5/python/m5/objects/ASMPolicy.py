from m5 import *
from BasePolicy import BasePolicy

class ASMPolicy(BasePolicy):    
    type = 'ASMPolicy'
    epoch = Param.Int("Number of cycles in each epoch")
    