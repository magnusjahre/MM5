from m5 import *
class MemoryTrace(ParamContext):
    type = 'MemoryTrace'
    trace = Param.String("dump memory traffic <filename>")
    thread = Param.Int(0, "which thread to trace")
    spec = Param.Bool(False, "trace misspeculated execution")

class MemTraceReader(SimObject):
    type = 'MemTraceReader'
    abstract = True
    filename = Param.String("trace file")

class M5Reader(MemTraceReader):
    type = 'M5Reader'

class IBMReader(MemTraceReader):
    type = 'IBMReader'

class ITXReader(MemTraceReader):
    type = 'ITXReader'

class MemTraceWriter(SimObject):
    type = 'MemTraceReader'
    abstract = True
    filename = Param.String("trace file")

class M5Writer(MemTraceWriter):
    type = 'M5Writer'

class ITXWriter(MemTraceWriter):
    type = 'ITXWriter'

class TraceCPU(SimObject):
    type = 'TraceCPU'
    data_trace = Param.MemTraceReader(NULL, "data trace")
    dcache = Param.BaseMem(NULL, "data cache")
    icache = Param.BaseMem(NULL, "instruction cache")

class OptCPU(SimObject):
    type = 'OptCPU'
    data_trace = Param.MemTraceReader(NULL, "data trace")
    assoc = Param.Int("associativity")
    block_size = Param.Int("block size in bytes")
    size = Param.MemorySize("capacity in bytes")
