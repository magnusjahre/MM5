from m5 import *
from BaseHier import BaseHier

class Interconnect(BaseHier):
    type = 'Interconnect'
    abstract = True
    width = Param.Int("interconnect transfer line width in bytes")
    clock = Param.Clock("interconnect frequency")
    transferDelay = Param.Int("interconnect transfer delay in interconnect cycles")
    arbitrationDelay = Param.Int("interconnect arbitration delay in interconnect cycles")
    cpu_count = Param.Int("the number of CPUs in the system");
