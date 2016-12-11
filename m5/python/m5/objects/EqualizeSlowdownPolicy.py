from m5 import *
from BasePolicy import BasePolicy

class ESPSearchAlgorithm(Enum): vals = ['exhaustive', 'lookahead']

class EqualizeSlowdownPolicy(BasePolicy):    
    type = 'EqualizeSlowdownPolicy'
    searchAlgorithm = Param.ESPSearchAlgorithm("The algorithm to use to find the cache partition")
    allowNegativeMisses = Param.Bool("Allow negative misses in the performance model")
    maxSteps = Param.Int("Maximum number of changes from current allocation")
