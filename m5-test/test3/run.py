from m5 import *
AddToPath('..')
from DetailedUniConfig import *
import Benchmarks

BaseCPU.workload = [ Benchmarks.AnagramLongCP(), Benchmarks.GCCLongCP() ]
BaseCPU.max_insts_any_thread = 1000000
root = DetailedStandAlone()
