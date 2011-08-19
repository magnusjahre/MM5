from m5 import *

class CachePartitioning(SimObject):
    type = 'CachePartitioning'
    abstract = True
    associativity = Param.Int("Cache associativity")
    epoch_size = Param.Tick("Size of an epoch")
    np = Param.Int("Number of cores")
    cache_interference = Param.CacheInterference("Pointer to the cache interference object")
