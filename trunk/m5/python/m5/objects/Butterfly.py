from m5 import *
from Interconnect import Interconnect

class Butterfly(Interconnect):
    type = 'Butterfly'
    switch_delay = Param.Int("The number CPU clock cycles to traverse a switch")
    radix = Param.Int("The switching-degree of the network switches")
    banks = Param.Int("The number of banks in the last-level cache")