from m5 import *
class ExecutionTrace(ParamContext):
    type = 'ExecutionTrace'
    speculative = Param.Bool(False, "capture speculative instructions")
    print_cycle = Param.Bool(True, "print cycle number")
    print_opclass = Param.Bool(True, "print op class")
    print_thread = Param.Bool(True, "print thread number")
    print_effaddr = Param.Bool(True, "print effective address")
    print_data = Param.Bool(True, "print result data")
    print_iregs = Param.Bool(False, "print all integer regs")
    print_fetchseq = Param.Bool(False, "print fetch sequence number")
    print_cpseq = Param.Bool(False, "print correct-path sequence number")
