from m5 import *
class FetchTrace(ParamContext):
    type = 'FetchTrace'
    trace = VectorParam.String("dump trace of fetch activity")
