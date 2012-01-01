from m5 import *
from Interconnect import Interconnect

class PeerToPeerLink(Interconnect):
    type = 'PeerToPeerLink'
    detailed_sim_start_tick = Param.Tick("The tick detailed simulation starts")
    interference_manager = Param.InterferenceManager("InterferenceManager object")
