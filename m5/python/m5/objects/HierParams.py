from m5 import *
class HierParams(SimObject):
    type = 'HierParams'
    do_data = Param.Bool("Store data in this hierarchy")
    do_events = Param.Bool("Simulate timing in this hierarchy")
    cpu_count = Param.Int("Number of CPUs")
