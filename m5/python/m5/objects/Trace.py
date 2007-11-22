from m5 import *
class Trace(ParamContext):
    type = 'Trace'
    flags = VectorParam.String([], "categories to be traced")
    start = Param.Tick(0, "cycle to start tracing")
    bufsize = Param.Int(0, "circular buffer size (0 = send to file)")
    file = Param.String('cout', "trace output file")
    dump_on_exit = Param.Bool(False, "dump trace buffer on exit")
    ignore = VectorParam.String([], "name strings to ignore")

