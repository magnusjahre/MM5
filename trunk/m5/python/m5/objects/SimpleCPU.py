from m5 import *
from BaseCPU import BaseCPU

class SimpleCPU(BaseCPU):
    type = 'SimpleCPU'
    width = Param.Int(1, "CPU width")
    function_trace = Param.Bool(False, "Enable function trace")
    function_trace_start = Param.Tick(0, "Cycle to start function trace")
