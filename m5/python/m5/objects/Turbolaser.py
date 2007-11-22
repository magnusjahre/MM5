from m5 import *
from Device import PioDevice
from Device import FooPioDevice
from Platform import Platform

class TlaserClock(SimObject):
    type = 'TlaserClock'
    delay = Param.Latency('1ms', "number of cycles to delay clock start")
    frequency = Param.Frequency('1200Hz', "clock interrupt frequency")
    intr_control = Param.IntrControl(Parent.any, "interrupt controller")

class TlaserIpi(PioDevice):
    type = 'TlaserIpi'
    tlaser = Param.Turbolaser(Parent.any, "Turbolaser")
    addr = 0xff8e000000

class TlaserMBox(SimObject):
    type = 'TlaserMBox'
    physmem = Param.PhysicalMemory(Parent.any, "physical memory")

class TlaserMC146818(PioDevice):
    type = 'TlaserMC146818'
    clock = Param.TlaserClock(Parent.any, "turbolaser clock")
    time = Param.Int(1136073600,
        "System time to use (0 for actual time, default is 1/1/06")
    addr = 0xffb0000000

class TlaserNodeType(Enum):
    vals = ["KFTHA", "KFTIA", "MS7CC", "SingleProc4M", "SingleProc16M",
            "DualProc4M", "DualProc16M"]

class TlaserNode(PioDevice):
    type = 'TlaserNode'
    node_type = Param.TlaserNodeType("Node Type")
    number = Param.Int("Node Number")
    tlaser = Param.Turbolaser(Parent.any, "Turbolaser")

class TlaserPciDev(FooPioDevice):
    type = 'TlaserPciDev'
    VendorID = Param.UInt16("Vendor ID")
    DeviceID = Param.UInt16("Device ID")
    Command = Param.UInt16(0, "Command")
    Status = Param.UInt16(0, "Status")
    Revision = Param.UInt8(0, "Device")
    ProgIF = Param.UInt8(0, "Programming Interface")
    SubClassCode = Param.UInt8(0, "Sub-Class Code")
    ClassCode = Param.UInt8(0, "Class Code")
    CacheLineSize = Param.UInt8(0, "System Cacheline Size")
    LatencyTimer = Param.UInt8(0, "PCI Latency Timer")
    HeaderType = Param.UInt8(0, "PCI Header Type")
    BIST = Param.UInt8(0, "Built In Self Test")

    BAR0 = Param.UInt32(0x00, "Base Address Register 0")
    BAR1 = Param.UInt32(0x00, "Base Address Register 1")
    BAR2 = Param.UInt32(0x00, "Base Address Register 2")
    BAR3 = Param.UInt32(0x00, "Base Address Register 3")
    BAR4 = Param.UInt32(0x00, "Base Address Register 4")
    BAR5 = Param.UInt32(0x00, "Base Address Register 5")
    BAR0Size = Param.UInt32(0, "Base Address Register 0 Size")
    BAR1Size = Param.UInt32(0, "Base Address Register 1 Size")
    BAR2Size = Param.UInt32(0, "Base Address Register 2 Size")
    BAR3Size = Param.UInt32(0, "Base Address Register 3 Size")
    BAR4Size = Param.UInt32(0, "Base Address Register 4 Size")
    BAR5Size = Param.UInt32(0, "Base Address Register 5 Size")

    CardbusCIS = Param.UInt32(0x00, "Cardbus Card Information Structure")
    SubsystemID = Param.UInt16(0x00, "Subsystem ID")
    SubsystemVendorID = Param.UInt16(0x00, "Subsystem Vendor ID")
    ExpansionROM = Param.UInt32(0x00, "Expansion ROM Base Address")
    InterruptLine = Param.UInt8(0x00, "Interrupt Line")
    InterruptPin = Param.UInt8(0x00, "Interrupt Pin")
    MaximumLatency = Param.UInt8(0x00, "Maximum Latency")
    MinimumGrant = Param.UInt8(0x00, "Minimum Grant")

class TlaserPcia(PioDevice):
    type = 'TlaserPcia'
    addr = 0xc780000000

class TlaserSerial(PioDevice):
    type = 'TlaserSerial'
    sernum = Param.UInt32(0xFAFAFAFAL, "Serial Number")
    addr = 0xffc7000000

class Turbolaser(Platform):
    type = 'Turbolaser'
    clock = Param.TlaserClock(Parent.any, "turbolaser clock")
    mbox = Param.TlaserMBox(Parent.any, "message box")
