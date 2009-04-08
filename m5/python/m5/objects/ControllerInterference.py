from m5 import *

class ControllerInterference(SimObject):
    type = 'ControllerInterference'
    memory_controller = Param.TimingMemoryController("Associated memory controller")
    rf_limit_all_cpus = Param.Int("Private latency estimation ready first limit")
    do_ooo_insert = Param.Bool("If true, a reordering step is applied to the recieved requests (experimental)")
    cpu_count = Param.Int("Number of cpus")
