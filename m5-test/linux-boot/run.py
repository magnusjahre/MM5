from m5 import *
AddToPath('..')
from TsunamiSystem import *

SDRAM.latency = '50ns'
ToMemBus.clock = '2GHz'
root = TsunamiRoot(clock = '2GHz', system = LinuxSystem())
