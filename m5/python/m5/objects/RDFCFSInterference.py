from m5 import *
from ControllerInterference import ControllerInterference

class RDFCFSControllerInterference(ControllerInterference):
    type = 'RDFCFSControllerInterference'
    memory_controller = Param.TimingMemoryController("Associated memory controller")
    rf_limit_all_cpus = Param.Int("Private latency estimation ready first limit")
    do_ooo_insert = Param.Bool("If true, a reordering step is applied to the recieved requests (experimental)")
    cpu_count = Param.Int("Number of cpus")
    buffer_size = Param.Int("Buffer size per CPU")
    use_average_lats = Param.Bool("if true, the average latencies are used")
    pure_head_pointer_model = Param.Bool("if true, the queue is traversed from the headptr to the current item")