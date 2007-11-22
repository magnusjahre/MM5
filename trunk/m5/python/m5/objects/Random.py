from m5 import *
class Random(ParamContext):
    type = 'Random'
    seed = Param.UInt32(1, "seed to random number generator");

