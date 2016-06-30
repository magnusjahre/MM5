from m5 import *
from BasePolicy import BasePolicy

class ESPSearchAlgorithm(Enum): vals = ['exhaustive', 'lookahead']

class EqualizeSlowdownPolicy(BasePolicy):    
    type = 'EqualizeSlowdownPolicy'
    searchAlgorithm = Param.ESPSearchAlgorithm("The algorithm to use to find the cache partition")
