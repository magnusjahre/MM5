from m5 import *

class MissBandwidthPolicy(SimObject):
    type = 'MissBandwidthPolicy'
    abstract = True
    period = Param.Tick("The number of clock cycles between each decision")
    interferenceManager = Param.InterferenceManager("The system's interference manager")
    cpuCount = Param.Int("The number of cores in the system")
    busUtilizationThreshold = Param.Double("The actual bus utilzation to consider the bus as full")
    requestCountThreshold = Param.Double("The request intensity (requests / tick) to assume no request increase")
