from m5 import *

class ThrottlingPolicy(Enum): vals = ['strict', 'average', 'token']
class CacheType(Enum): vals = ['private', 'shared']

class ThrottleControl(SimObject):    
    type = 'ThrottleControl'

    target_request_rate = Param.Float("The downstream request rate target for this cache")
    do_arrival_rate_trace = Param.Bool("Trace the arrival rate on every request (caution!)")
    throttling_policy = Param.ThrottlingPolicy("The policy to use to enforce throttles")
    cache_type = Param.CacheType("Type of cache (shared or private)")
    cpu_count = Param.Int("Number of cores")
    cache_cpu_id = Param.Int("The CPU ID of the attached cache (only for private caches)")
