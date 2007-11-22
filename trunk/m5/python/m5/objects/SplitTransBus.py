from m5 import *
from Interconnect import Interconnect

class SplitTransBus(Interconnect):
    type = 'SplitTransBus'
    pipelined = Param.Bool("true if the bus has pipelined transmission and arbitration")