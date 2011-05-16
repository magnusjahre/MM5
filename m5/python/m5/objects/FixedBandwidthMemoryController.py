from m5 import *
from TimingMemoryController import TimingMemoryController

class FixedBandwidthMemoryController(TimingMemoryController):
    type = 'FixedBandwidthMemoryController'
    queue_size = Param.Int("Max request queue size")
    cpu_count  = Param.Int("Number of cores")
