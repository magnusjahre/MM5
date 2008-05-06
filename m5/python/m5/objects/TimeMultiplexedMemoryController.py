from m5 import *
from TimingMemoryController import TimingMemoryController

class TimeMultiplexedMemoryController(TimingMemoryController):
    type = 'TimeMultiplexedMemoryController'
    queue_size = Param.Int(64, "Max request queue size")
