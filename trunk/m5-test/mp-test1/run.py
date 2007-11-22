from m5 import *
AddToPath('..')
from SimpleConfig import *
import Benchmarks

BaseCPU.workload = Parent.workload
SimpleStandAlone.cpu = [ CPU() for i in xrange(4) ]
SimpleStandAlone.workload = Benchmarks.Radix()
root = SimpleStandAlone()
