from m5 import *
AddToPath('..')
from DetailedUniConfig import *
import Benchmarks

BaseCPU.workload = Benchmarks.AnagramShort()
BaseCPU.max_insts_any_thread = 500000
root = DetailedStandAlone()
