from m5 import *
AddToPath('..')
from DetailedUniConfig import *
import Benchmarks

BaseCPU.workload = [ Benchmarks.AnagramLongCP(), Benchmarks.GCCLongCP(),
                     Benchmarks.AnagramLong(), Benchmarks.GCCLong() ]
BaseCPU.max_insts_all_threads = 100000
root = DetailedStandAlone()
