from m5 import *
class DirectoryCoherence(Enum): vals = ['stenstrom']

class DirectoryProtocol(SimObject):
    type = 'DirectoryProtocol'
    protocol = Param.DirectoryCoherence("name of coherence protocol")
    doTrace = Param.Bool("turns on tracing of coherence protocol actions");
    dumpInterval = Param.Int("number of clock cycles between each statsdump (0 turns it off)");
