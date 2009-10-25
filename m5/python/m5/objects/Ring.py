from m5 import *
from Interconnect import Interconnect

class Ring(Interconnect):
    type = 'Ring'
    detailed_sim_start_tick = Param.Tick("The tick detailed simulation starts")
    single_proc_id = Param.Int("the expected CPU ID if there is only one processor")
    interference_manager = Param.InterferenceManager("InterferenceManager object")
    fixed_roundtrip_latency = Param.Int("Models infinite bandwidth, fixed latency shared memory system")
