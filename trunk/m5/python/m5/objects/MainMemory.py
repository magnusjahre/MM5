from m5 import *
from FunctionalMemory import FunctionalMemory

class MainMemory(FunctionalMemory):
    type = 'MainMemory'
    do_data = Param.Bool(False, "dummy param")
