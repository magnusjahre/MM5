from m5 import *
from FullCPU import FullCPU
from BaseCache import BaseCache

class MemoryOverlapEstimator(SimObject):
    type = 'MemoryOverlapEstimator'
    id = Param.Int("ID")
    interference_manager = Param.InterferenceManager("Interference Manager Object")
    cpu_count = Param.Int("Number of CPUs")

