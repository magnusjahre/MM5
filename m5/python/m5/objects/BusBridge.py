from m5 import *
from BaseHier import BaseHier

class BusBridge(BaseHier):
    type = 'BusBridge'
    max_buffer = Param.Int(8, "The number of requests to buffer")
    in_bus = Param.Bus("The bus to forward from")
    out_bus = Param.Bus("The bus to forward to")
    latency = Param.Latency('0ns', "The latency of this bridge")
    ack_writes = Param.Bool(False, "Should this bridge ack writes")
    ack_delay = Param.Latency('0ns', "The latency till the bridge acks a write")
