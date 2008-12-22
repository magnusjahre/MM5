from m5 import *
from Interconnect import Interconnect

class Ring(Interconnect):
    type = 'Ring'
    detailed_sim_start_tick = Param.Tick("The tick detailed simulation starts")