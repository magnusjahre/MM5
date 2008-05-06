from m5 import *
from TimingMemoryController import TimingMemoryController

class NFQMemoryController(TimingMemoryController):
    type = 'NFQMemoryController'
    rd_queue_size = Param.Int("Max read request queue size")
    wr_queue_size = Param.Int("Max write request queue size")
    starvation_prevention_thres = Param.Int("Starvation prevention threshold")