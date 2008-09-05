from m5 import *
from Interconnect import Interconnect

class Crossbar(Interconnect):
    type = 'Crossbar'
    use_NFQ_arbitration = Param.Bool("If true, Network Fair Queuing arbitration is used")
    detailed_sim_start_tick = Param.Tick("The tick detailed simulation starts")