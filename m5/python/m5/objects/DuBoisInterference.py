from m5 import *
from ControllerInterference import ControllerInterference

class DuBoisInterference(ControllerInterference):
    type = 'DuBoisInterference'
    memory_controller = Param.TimingMemoryController("Associated memory controller")
    cpu_count = Param.Int("Number of cpus")
