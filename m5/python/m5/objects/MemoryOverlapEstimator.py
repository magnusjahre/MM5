from m5 import *
from FullCPU import FullCPU
from BaseCache import BaseCache

class MemoryOverlapEstimator(SimObject):
    type = 'MemoryOverlapEstimator'
    id = Param.Int("ID")
    #cpu = Param.FullCPU("The CPU")
    #cache = Param.BaseCache("The cache")    
