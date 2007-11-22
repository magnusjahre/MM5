from m5 import *
AddToPath('..')
from TsunamiSystem import *

AlphaConsole.cpu = Parent.cpu[0]
IntrControl.cpu = Parent.cpu[0]
LinuxSystem.cpu = [ SimpleCPU() for i in xrange(2) ]

SDRAM.latency = '50ns'
ToMemBus.clock = '2GHz'
root = TsunamiRoot(clock = '2GHz', system = LinuxSystem())

