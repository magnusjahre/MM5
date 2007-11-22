from m5 import *
class PseudoInst(ParamContext):
    type = 'PseudoInst'
    quiesce = Param.Bool(True, "enable quiesce instructions")
    statistics = Param.Bool(True, "enable statistics pseudo instructions")
    checkpoint = Param.Bool(True, "enable checkpoint pseudo instructions")
