from m5 import *
class Serialize(ParamContext):
    type = 'Serialize'
    dir = Param.String('cpt.%012d', "dir to stick checkpoint in")
    cycle = Param.Tick(0, "cycle to serialize")
    period = Param.Tick(0, "period to repeat serializations")
    count = Param.Int(10, "maximum number of checkpoints to drop")
