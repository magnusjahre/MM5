from m5 import *
from Interconnect import Interconnect

class Crossbar(Interconnect):
    type = 'Crossbar'
    use_NFQ_arbitration = Param.Bool("If true, Network Fair Queuing arbitration is used")
    detailed_sim_start_tick = Param.Tick("The tick detailed simulation starts")
    shared_cache_writeback_buffers = Param.Int("Number of writeback buffers in the shared cache")
    shared_cache_mshrs = Param.Int("Number of MSHRs in the shared cache")