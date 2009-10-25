from m5 import *
from Interconnect import Interconnect

class Crossbar(Interconnect):
    type = 'Crossbar'
    use_NFQ_arbitration = Param.Bool("If true, Network Fair Queuing arbitration is used")
    detailed_sim_start_tick = Param.Tick("The tick detailed simulation starts")
    shared_cache_writeback_buffers = Param.Int("Number of writeback buffers in the shared cache")
    shared_cache_mshrs = Param.Int("Number of MSHRs in the shared cache")
    pipe_stages = Param.Int("Crossbar pipeline stages")
    interference_manager = Param.InterferenceManager("InterferenceManager object")
    fixed_roundtrip_latency = Param.Int("Models infinite bandwidth, fixed latency shared memory system")
