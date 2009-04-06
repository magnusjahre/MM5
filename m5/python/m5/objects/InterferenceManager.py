from m5 import *

class InterferenceManager(SimObject):
    type = 'InterferenceManager'
    cpu_count = Param.Int("Number of CPUs")


