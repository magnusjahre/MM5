from m5 import *
from BaseHier import BaseHier

class Bus(BaseHier):
    type = 'Bus'
    clock = Param.Clock("bus frequency")
    width = Param.Int("bus width in bytes")
    adaptive_mha = Param.AdaptiveMHA("Adaptive MHA Object")
    cpu_count = Param.Int("Number of CPUs in the system")
    bank_count = Param.Int("The number of L2 cache banks in the system")
    memory_controller = Param.TimingMemoryController("Memory Controller Object")

