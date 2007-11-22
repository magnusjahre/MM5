from m5 import *
class IntervalStats(ParamContext):
    type = 'IntervalStats'
    file = Param.String("output file for interval statistics")
    exit_when_done = Param.Bool("exit after last cycle interval")
    enable_stat_triggers = Param.Bool("stats dump instruction triggers")
    cycle = Param.Counter("dump statistics every 'n' cycles")
    range = Param.Range(Range(0,0), "cycle range to dump")
    stats = VectorParam.String("statistics to dump")
    if False:
        inst = Param.Counter("dump statistics every 'n' instructions");
