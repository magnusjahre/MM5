from m5 import *
class BranchPred(SimObject):
    type = 'BranchPred'

    class PredictorType(Enum): vals = ['resetting', 'saturating']
    class PredictorClass(Enum): vals = ['hybrid', 'global', 'local']

    btb_assoc = Param.Int("BTB associativity")
    btb_size = Param.Int("number of entries in BTB")
    choice_index_bits = Param.Int(0, "choice predictor index bits")
    choice_xor = Param.Bool(False, "XOR choice hist w/PC (False: concatenate)")
    conf_pred_ctr_bits = Param.Int(0, "confidence predictor counter bits")
    conf_pred_ctr_thresh = Param.Int(0, "confidence predictor threshold")
    conf_pred_ctr_type = Param.PredictorType('saturating',
                                             "confidence predictor type")
    conf_pred_enable = Param.Bool(False, "enable confidence predictor")
    conf_pred_index_bits = Param.Int(0, "confidence predictor index bits")
    conf_pred_xor = Param.Bool(False, "XOR confidence predictor bits")
    global_hist_bits = Param.Int(0, "global predictor history reg bits")
    global_index_bits = Param.Int(0, "global predictor index bits")
    global_xor = Param.Bool(False, "XOR global hist w/PC (False: concatenate)")
    local_hist_bits = Param.Int(0, "local predictor history reg bits")
    local_hist_regs = Param.Int(0, "num. local predictor history regs")
    local_index_bits = Param.Int(0, "local predictor index bits")
    local_xor = Param.Bool(False, "XOR local hist w/PC (False: concatenate)")
    pred_class = Param.PredictorClass("predictor class")
    ras_size = Param.Int("return address stack size")
