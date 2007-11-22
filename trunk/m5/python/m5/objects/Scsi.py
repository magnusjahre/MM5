from m5 import *
from Device import DmaDevice

class ScsiController(DmaDevice):
    type = 'ScsiController'
    tlaser = Param.Turbolaser(Parent.any, "Turbolaser")
    disks = VectorParam.ScsiDisk("Disks attached to this controller")
    engine = Param.DmaEngine(Parent.any, "DMA Engine")

class ScsiDevice(SimObject):
    type = 'ScsiDevice'
    abstract = True
    target = Param.Int("target scsi ID")

class ScsiNone(ScsiDevice):
    type = 'ScsiNone'

class ScsiDisk(ScsiDevice):
    type = 'ScsiDisk'
    delay = Param.Latency('0us', "fixed disk delay")
    image = Param.DiskImage("disk image")
    sector_size = Param.Int(512, "disk sector size")
    serial = Param.String("disk serial number")
    use_interface = Param.Bool(True, "use dma interface")
