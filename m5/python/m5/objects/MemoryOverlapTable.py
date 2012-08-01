from m5 import *

class MemoryOverlapTable(SimObject):
    type = 'MemoryOverlapTable'
    request_table_size = Param.Int("The size of the request table")
    unknown_table_size = Param.Int("The size of the unknown latency table")
