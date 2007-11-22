from m5 import *
class DmaEngine(SimObject):
    type = 'DmaEngine'
    bus = Param.Bus(NULL, "bus that we're attached to")
    channels = Param.Int(36, "number of dma channels")
    physmem = Param.PhysicalMemory(Parent.any, "physical memory")
    no_allocate = Param.Bool(True, "Should we allocate cache on read")
