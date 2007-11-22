from m5 import *
from FunctionalMemory import FunctionalMemory

class MemoryController(FunctionalMemory):
    type = 'MemoryController'
    capacity = Param.Int(64, "Maximum Number of children")
