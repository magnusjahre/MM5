from m5 import *
from BaseHier import BaseHier

class Bus(BaseHier):
    type = 'Bus'
    clock = Param.Clock("bus frequency")
    infinite_writeback = Param.Bool(False, "Infinite Writeback Queue")
    readqueue_size = Param.Int(64, "Max size of read queue")
    writequeue_size = Param.Int(64, "Max size of write queue")
    prewritequeue_size = Param.Int(64, "Max size of prewriteback queue")
    reserved_slots = Param.Int(2, "Numer of activations reserved for reads")
    start_trace = Param.Int(0, "Point to start tracing")
    trace_interval = Param.Int(100000, "How often to trace")
    width = Param.Int("bus width in bytes")
    adaptive_mha = Param.AdaptiveMHA("Adaptive MHA Object")
    cpu_count = Param.Int("Number of CPUs in the system")
    bank_count = Param.Int("The number of L2 cache banks in the system")

