from m5 import *
from BasePolicy import BasePolicy

class ASRPolicy(BasePolicy):    
    type = 'ASRPolicy'
    epoch = Param.Int("Number of cycles in each epoch")
    