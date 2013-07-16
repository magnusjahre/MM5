from m5 import *
from ControllerInterference import ControllerInterference

class DuBoisInterference(ControllerInterference):
    type = 'DuBoisInterference'
    memory_controller = Param.TimingMemoryController("Associated memory controller")
    cpu_count = Param.Int("Number of cpus")
    use_ora = Param.Bool("Use the ORA to estimate bus service interference")
