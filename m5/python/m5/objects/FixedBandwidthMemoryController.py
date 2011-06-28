from m5 import *
from TimingMemoryController import TimingMemoryController

class FixedBandwidthMemoryController(TimingMemoryController):
    type = 'FixedBandwidthMemoryController'
    queue_size = Param.Int("Max request queue size")
    cpu_count  = Param.Int("Number of cores")
    starvation_threshold = Param.Int("Number of consecutive requests that are allowed to bypass the oldest requests")
    ready_threshold = Param.Int("Number of ready requests that are allowed to bypass requests from cores with more tokens")
