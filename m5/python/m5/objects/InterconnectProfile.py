from m5 import *

class InterconnectProfile(SimObject):
    type = 'InterconnectProfile'
    traceSends = Param.Bool("Trace number of sends?")
    traceChannelUtil = Param.Bool("Trace channel utilisation?")
    traceStartTick = Param.Tick("The clock cycle to start the trace")
    interconnect = Param.Interconnect(NULL, "The interconnect to profile")