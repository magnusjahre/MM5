from m5 import *
AddToPath('..')
from SimpleConfig import *
import Benchmarks

BaseCPU.workload = Benchmarks.AnagramShort()
BaseCPU.max_insts_any_thread = 900000
root = SimpleStandAlone()
