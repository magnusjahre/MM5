from m5 import *
from CachePartitioning import CachePartitioning

class UCPSearchAlgorithm(Enum): vals = ['exhaustive', 'lookahead']

class UtilityBasedPartitioning(CachePartitioning):
    type = 'UtilityBasedPartitioning'
    searchAlgorithm = Param.UCPSearchAlgorithm("The algorithm to use to find the cache partition")