from m5 import *
from ControllerInterference import ControllerInterference

class FCFSControllerInterference(ControllerInterference):
    type = 'FCFSControllerInterference'
    memory_controller = Param.TimingMemoryController("Associated memory controller")
    cpu_count = Param.Int("Number of cpus")
