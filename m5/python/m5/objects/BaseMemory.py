from m5 import *
from BaseMem import BaseMem

class BaseMemory(BaseMem):
    type = 'BaseMemory'
    abstract = True
    compressed = Param.Bool(False, "This memory stores compressed data.")
    do_writes = Param.Bool(False, "update memory")
    in_bus = Param.Bus(NULL, "incoming bus object")
    snarf_updates = Param.Bool(True,
        "update memory on cache-to-cache transfers")
    uncacheable_latency = Param.Latency('0ns', "uncacheable latency")
    if build_env['FULL_SYSTEM']:
        func_mem = Param.FunctionalMemory(Parent.physmem,
                               "corresponding functional memory object")
