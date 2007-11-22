from m5 import *
from System import System

class LinuxSystem(System):
    type = 'LinuxSystem'
    system_type = 34
    system_rev = 1 << 10
