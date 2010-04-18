from m5 import *

class TrafficGenerator(SimObject):
    type = 'TrafficGenerator'
    membus = Param.Bus("The memory bus to inject traffic into")
    use_id = Param.Int("The CPU ID to use for the requets")

