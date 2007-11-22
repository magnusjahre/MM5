from m5 import *
class PipeTrace(SimObject):
    type = 'PipeTrace'
    exit_when_done = Param.Bool(False,
        "terminate simulation when done collecting ptrace")
    file = Param.String('', "output file name")
    range = Param.String('', "range of cycles to trace")
    statistics = VectorParam.String("stats to include in pipe-trace")
