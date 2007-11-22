from m5 import *
from BaseCPU import BaseCPU

class DerivOoOCPU(BaseCPU):
    type = 'DerivOoOCPU'
    width = Param.Int(1, "CPU width")
    issueWidth = Param.Int(1, "CPU issue width")
    function_trace = Param.Bool(False, "Enable function trace")
    function_trace_start = Param.Tick(0, "Cycle to start function trace")

    def check(self):
        has_workload = self._hasvalue('workload')
        has_dtb = self._hasvalue('dtb')
        has_itb = self._hasvalue('itb')
        has_mem = self._hasvalue('mem')
        has_system = self._hasvalue('system')

        if has_workload:
            self.dtb.disable = True
            self.itb.disable = True
            self.mem.disable = True
            self.system.disable = True
            self.mult.disable = True

        if has_dtb or has_itb or has_mem or has_system:
            self.workload.disable = True
