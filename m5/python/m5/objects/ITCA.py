from m5 import *

#class ITCACPUStallPolicy(Enum): vals = ['dispatch', 'rob']
#class ITCAInstructionMissPolicy(Enum): vals = ['one', 'all']

class ITCA(SimObject):
    type = 'ITCA'
    cpu_id = Param.Int("CPU ID")
