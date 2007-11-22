from m5 import *
AddToPath('..')
from TsunamiSystem import *

L2.size = '1MB'
L2.assoc = 4
L2.mshrs = 32
DetailedCPU.iq.size = 256
DetailedCPU.lsq_size = 32
DetailedCPU.rob_size = 512
DetailedCPU.pc_sample_interval = '40MHz'

SDRAM.latency = '10ns'
ToMemBus.clock = '4GHz'
root = TsunamiRoot(clock = '4GHz', system = DetailedLinuxSystem())
