from m5 import *
from TimingMemoryController import TimingMemoryController

class FCFSMemoryController(TimingMemoryController):
    type = 'FCFSTimingMemoryController'
    queue_size = Param.Int(64, "Max request queue size")
