from m5 import *
from TimingMemoryController import TimingMemoryController

class PriorityScheme(Enum): vals = ['FCFS', 'RoW']
class PagePolicy(Enum): vals = ['ClosedPage', 'OpenPage']

class RDFCFSMemoryController(TimingMemoryController):
    type = 'RDFCFSTimingMemoryController'
    readqueue_size = Param.Int(64, "Max size of read queue")
    writequeue_size = Param.Int(64, "Max size of write queue")
    reserved_slots = Param.Int(2, "Number of activations reserved for reads")
    inf_write_bw = Param.Bool("Infinite writeback bandwidth")
    page_policy = Param.PagePolicy("Controller page policy")
    priority_scheme = Param.PriorityScheme("Controller priority scheme")
