from m5 import *

class InterferenceManager(SimObject):
    type = 'InterferenceManager'
    cpu_count = Param.Int("Number of CPUs")    
    sample_size = Param.Int("Number of requests")
    reset_interval = Param.Int("Number of requests after which the measurements are reset")

