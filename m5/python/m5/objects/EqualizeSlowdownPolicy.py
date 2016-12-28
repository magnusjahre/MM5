from m5 import *
from BasePolicy import BasePolicy

class ESPSearchAlgorithm(Enum): vals = ['exhaustive', 'lookahead']
class ESPGradientModel(Enum): vals = ['computed', 'global']

class EqualizeSlowdownPolicy(BasePolicy):    
    type = 'EqualizeSlowdownPolicy'
    searchAlgorithm = Param.ESPSearchAlgorithm("The algorithm to use to find the cache partition")
    maxSteps = Param.Int("Maximum number of changes from current allocation")
    gradientModel = Param.ESPGradientModel("The model to use to estimate the LLC miss gradient")
    lookaheadCap  = Param.Int("The maximum allocation in each round for the lookahead algorithm (0 == associativtiy == no cap)")
