from m5 import *
class Sampler(SimObject):
    type = 'Sampler'
    phase0_cpus = VectorParam.BaseCPU("vector of actual CPUs to run in phase 0")
    phase1_cpus = VectorParam.BaseCPU("vector of actual CPUs to run in phase 1")
    periods = VectorParam.Tick("vector of per-cpu sample periods")
