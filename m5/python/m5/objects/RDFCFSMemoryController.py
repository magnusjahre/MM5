from m5 import *
from TimingMemoryController import TimingMemoryController

class RDFCFSMemoryController(TimingMemoryController):
    type = 'RDFCFSTimingMemoryController'
    readqueue_size = Param.Int(64, "Max size of read queue")
    writequeue_size = Param.Int(64, "Max size of write queue")
    reserved_slots = Param.Int(2, "Number of activations reserved for reads")
