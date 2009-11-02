from m5 import *

class MissBandwidthPolicy(SimObject):
    type = 'MissBandwidthPolicy'
    abstract = True
    period = Param.Tick("The number of clock cycles between each decision")
    interferenceManager = Param.InterferenceManager("The system's interference manager")
