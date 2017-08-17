from m5 import *
from TimingMemoryController import TimingMemoryController

class PriorityScheme(Enum): vals = ['FCFS', 'RoW']
class PagePolicy(Enum): vals = ['ClosedPage', 'OpenPage']

class RDFCFSMemoryController(TimingMemoryController):
    type = 'RDFCFSTimingMemoryController'
    readqueue_size = Param.Int("Max size of read queue")
    writequeue_size = Param.Int("Max size of write queue")
    reserved_slots = Param.Int("Number of activations reserved for reads")
    inf_write_bw = Param.Bool("Infinite writeback bandwidth")
    page_policy = Param.PagePolicy("Controller page policy")
    priority_scheme = Param.PriorityScheme("Controller priority scheme")
    rf_limit_all_cpus = Param.Int("Private latency estimation ready first limit")
    starvation_threshold = Param.Int("Maximum number of non-oldest requests to issue in a row")
