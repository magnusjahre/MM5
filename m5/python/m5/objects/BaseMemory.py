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
    
    bus_frequency = Param.Int("Bus frequency in MHz")
    num_banks = Param.Int("Number of banks")
    
    RAS_latency = Param.Int("RAS-to-CAS latency (bus cycles)")
    CAS_latency = Param.Int("CAS latency (bus cycles)")
    precharge_latency = Param.Int("precharge latency (bus cycles)")
    min_activate_to_precharge_latency = Param.Int("Minimum activate to precharge time (bus cycles)")
    
    static_memory_latency = Param.Bool("Return the same latency for all data transfers")
    
    if build_env['FULL_SYSTEM']:
        func_mem = Param.FunctionalMemory(Parent.physmem,
                               "corresponding functional memory object")
