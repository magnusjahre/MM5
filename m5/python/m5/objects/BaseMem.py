from m5 import *
from BaseHier import BaseHier

class BaseMem(BaseHier):
    type = 'BaseMem'
    abstract = True
    addr_range = VectorParam.AddrRange(AllMemory, "The address range in bytes")
    latency = Param.Latency('0ns', "latency")
