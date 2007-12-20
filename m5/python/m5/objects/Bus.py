from m5 import *
from BaseHier import BaseHier

class Bus(BaseHier):
    type = 'Bus'
    clock = Param.Clock("bus frequency")
    width = Param.Int("bus width in bytes")
    adaptive_mha = Param.AdaptiveMHA("Adaptive MHA Object")
    uniform_partitioning = Param.Bool("Partition bus bandwidth uniformly between cores")
