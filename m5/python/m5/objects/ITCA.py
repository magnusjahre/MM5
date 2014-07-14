from m5 import *

class ITCACPUStallPolicy(Enum): vals = ['dispatch', 'rename', 'commit']
class ITCAInstructionPolicy(Enum): vals = ['one', 'all']

class ITCA(SimObject):
    type = 'ITCA'
    cpu_id = Param.Int("CPU ID")
    cpu_stall_policy = Param.ITCACPUStallPolicy("The signal that determines if the CPU is stalled")
    itip = Param.ITCAInstructionPolicy("How to handle intertask instruction misses")
    do_verification = Param.Bool("Turn on the verification trace (Warning: creates large files)")
