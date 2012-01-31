from m5 import *
from FullCPU import FullCPU
from BaseCache import BaseCache

class SharedStallHeuristic(Enum): vals = ['shared-exists', 'rob', 'rob-write']

class MemoryOverlapEstimator(SimObject):
    type = 'MemoryOverlapEstimator'
    id = Param.Int("ID")
    interference_manager = Param.InterferenceManager("Interference Manager Object")
    cpu_count = Param.Int("Number of CPUs")
    shared_stall_heuristic = Param.SharedStallHeuristic("The heuristic that decides if a processor stall is due to a shared event")