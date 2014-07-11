from m5 import *

class ITCACPUStallPolicy(Enum): vals = ['dispatch', 'rename', 'rob']
#class ITCAInstructionMissPolicy(Enum): vals = ['one', 'all']

class ITCA(SimObject):
    type = 'ITCA'
    cpu_id = Param.Int("CPU ID")
    cpu_stall_policy = Param.ITCACPUStallPolicy("The signal that determines if the CPU is stalled")
