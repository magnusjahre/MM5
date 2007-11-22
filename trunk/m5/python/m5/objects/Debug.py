from m5 import *
class Debug(ParamContext):
    type = 'Debug'
    break_cycles = VectorParam.Tick("cycle(s) to create breakpoints")
