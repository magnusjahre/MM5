from m5 import *
from SysPaths import *
from DetailedUniConfig import *

class Image0(CowDiskImage):
    image_file = disk('tru64.cow')
    child = RawDiskImage(image_file = disk('tru64.img'), read_only=True)
    read_only = True

class Image1(RawDiskImage):
    image_file = disk('exit.img')
    read_only = True

class ScsiPciDev(TlaserPciDev):
    VendorID = 0x1291
    DeviceID = 0x1291
    ClassCode = 0x01
    InterruptLine = 0x09
    InterruptPin = 0x01

class EtherPciDev(TlaserPciDev):
    VendorID = 0x1291
    DeviceID = 0x1292
    ClassCode = 0x02
    BAR0 = 0x00010000
    BAR1 = 0x00010000
    BAR2 = 0x00010000
    BAR3 = 0x00010000
    BAR4 = 0x00010000
    BAR5 = 0x00010000
    InterruptLine = 0x09
    InterruptPin = 0x02

class Tlaser(Turbolaser):
    clock = TlaserClock()
    ipi = TlaserIpi()
    mbox = TlaserMBox()
    rtc = TlaserMC146818()
    uart = Uart8530(addr = 0xffa0000000, size = 4096)
    sernum = TlaserSerial()
    pcia = TlaserPcia()
    node0 = TlaserNode(addr=0xff88000000, number=0, node_type='SingleProc4M')
    node1 = TlaserNode(addr=0xff89000000, number=4, node_type='MS7CC')
    node2 = TlaserNode(addr=0xff8a000000, number=8, node_type='KFTIA')
    console = AlphaConsole(addr=0x8000a00000, disk=Parent.simple_disk)
    dma_engine = DmaEngine()
    disk0 = ScsiDisk(image=Parent.image0, target=1, serial='00Q8127609LSM0')
    disk1 = ScsiDisk(image=Parent.image1, target=2, serial='00Q8127609LSM1')
    scsi_pci_dev = ScsiPciDev(addr=0xc700040000)
    scsi = ScsiController(addr=0xc500000000,
                          disks=[Parent.disk0, Parent.disk1])
    ether_pci_dev = EtherPciDev(addr=0xc700080000)
    etherdev = EtherDev(addr=0xc500010000)
    etherint = EtherDevInt(device = Parent.etherdev)
    clock.delay = '200ms'

class TlaserSystem(Tru64System):
    memctrl = MemoryController()
    cpu = SimpleCPU()
    tlaser = Tlaser()
    physmem = PhysicalMemory(range = AddrRange('128MB'))
    intrctrl = IntrControl()
    image0 = Image0()
    image1 = Image1()
    simple_disk = SimpleDisk(disk=Parent.image0)
    sim_console = SimConsole(listener = ConsoleListener(port=3456))
    kernel = binary('vmunix')
    pal = binary('tl_osfpal')
    console = binary('console')
    binned_fns = ''

DetailedCPU.dcache.repl = NULL
def DetailedTlaserSystem():
    self = TlaserSystem()
    self.toMemBus = ToMemBus()
    self.tlaser.dma_engine.bus = Parent.toMemBus
    self.ram = SDRAM(in_bus=Parent.toMemBus)
    self.cpu = DetailedCPU()
    return self

SDRAM.latency = '500ns'
ToMemBus.clock = '200MHz'

BaseCPU.itb = AlphaITB()
BaseCPU.dtb = AlphaDTB()
BaseCPU.mem = Parent.memctrl
BaseCPU.system = Parent.any

class TlaserRoot(Root):
    clock = '200MHz'
    system = TlaserSystem()
