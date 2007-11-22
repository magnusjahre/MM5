from m5 import *
from Config import *
from Processor import *

class IdeControllerPciData(PciConfigData):
    VendorID = 0x8086
    DeviceID = 0x7111
    Command = 0x0
    Status = 0x280
    Revision = 0x0
    ClassCode = 0x01
    SubClassCode = 0x01
    ProgIF = 0x85
    BAR0 = 0x00000001
    BAR1 = 0x00000001
    BAR2 = 0x00000001
    BAR3 = 0x00000001
    BAR4 = 0x00000001
    BAR5 = 0x00000001
    InterruptLine = 0x1f
    InterruptPin = 0x01
    BAR0Size = 8
    BAR1Size = 4
    BAR2Size = 8
    BAR3Size = 4
    BAR4Size = 16

class NSGigEPciData(PciConfigData):
    VendorID = 0x100B
    DeviceID = 0x0022
    Status = 0x0290
    SubClassCode = 0x00
    ClassCode = 0x02
    ProgIF = 0x00
    BAR0 = 0x00000001
    BAR1 = 0x00000000
    BAR2 = 0x00000000
    BAR3 = 0x00000000
    BAR4 = 0x00000000
    BAR5 = 0x00000000
    MaximumLatency = 0x34
    MinimumGrant = 0xb0
    InterruptLine = 0x1e
    InterruptPin = 0x01
    BAR0Size = 256
    BAR1Size = 4096

class SinicPciData(PciConfigData):
    VendorID = 0x1291
    DeviceID = 0x1293
    Status = 0x0290
    SubClassCode = 0x00
    ClassCode = 0x02
    ProgIF = 0x00
    BAR0 = 0x00000000
    BAR1 = 0x00000000
    BAR2 = 0x00000000
    BAR3 = 0x00000000
    BAR4 = 0x00000000
    BAR5 = 0x00000000
    MaximumLatency = 0x34
    MinimumGrant = 0xb0
    InterruptLine = 0x1e
    InterruptPin = 0x01
    BAR0Size = 256

if env['SINIC']:
    EtherDev = Sinic
    EtherPci = SinicPciData
    EtherInt = SinicInt
else:
    EtherDev = NSGigE
    EtherPci = NSGigEPciData
    EtherInt = NSGigEInt

class LinuxRootDisk(IdeDisk):
    raw_image = RawDiskImage(image_file=disk('linux.img'),
                             read_only=True)
    image = CowDiskImage(child=Parent.raw_image, read_only=False)

class LinuxSwapDisk(IdeDisk):
    raw_image = RawDiskImage(image_file = disk('swap.img'),
                                  read_only=True)
    image = CowDiskImage(child = Parent.raw_image, read_only=False)

class SurgeFilesetDisk(IdeDisk):
    raw_image = RawDiskImage(image_file = disk('linux.img'), #surge-fileset.img'),
                                  read_only=True)
    image = CowDiskImage(child = Parent.raw_image, read_only=False)

class SpecwebFilesetDisk(IdeDisk):
    raw_image = RawDiskImage(image_file = disk('linux.img'), #specweb-fileset.img'),
                                  read_only=True)
    image = CowDiskImage(child = Parent.raw_image, read_only=False)

def MyTsunami(num_etherdevs=1, **kwargs):
    self = Tsunami(**kwargs)
    self.cchip = TsunamiCChip(addr=0x801a0000000)
    self.pchip = TsunamiPChip(addr=0x80180000000)
    self.pciconfig = PciConfigAll(addr=0x801fe000000)
    self.fake_smchip = IsaFake(addr=0x801fc000370)
    self.fake_uart1 = IsaFake(addr=0x801fc0002f8)
    self.fake_uart2 = IsaFake(addr=0x801fc0003e8)
    self.fake_uart3 = IsaFake(addr=0x801fc0002e8)
    self.fake_uart4 = IsaFake(addr=0x801fc0003f0)
    self.fb = BadDevice(addr=0x801fc0003d0, devicename='FrameBuffer')
    self.io = TsunamiIO(addr=0x801fc000000)
    self.uart = Uart8250(addr=0x801fc0003f8)
    self.console = AlphaConsole(addr=0x80200000000, disk=Parent.simple_disk)
    self.disk0 = LinuxRootDisk(driveID='master')
    self.disk1 = SpecwebFilesetDisk(driveID='slave')
    self.disk2 = LinuxSwapDisk(driveID='master')
    self.ide = IdeController(disks=[Parent.disk0, Parent.disk1, Parent.disk2],
                             configdata=IdeControllerPciData(),
                             pci_func=0, pci_dev=0, pci_bus=0)
    self.etherdev = [ EtherDev(configdata=EtherPci(), 
                               pci_bus=0, pci_dev=i+1, pci_func=0,
                               dma_no_allocate=env['NO_ALLOCATE']) \
                      for i in xrange(num_etherdevs) ]
    self.etherint = [ EtherInt(device=Parent.etherdev[i]) \
                      for i in xrange(num_etherdevs) ]

    return self

#
# System level configuration
#
BaseCPU.itb = AlphaITB()
BaseCPU.dtb = AlphaDTB()
BaseCPU.mem = Parent.memctrl
BaseCPU.system = Parent.any

def TsunamiSystem(num_etherdevs=1):
    self = LinuxSystem()
    self.physmem = PhysicalMemory(range = AddrRange(env['MEMSIZE']))
    self.tsunami = MyTsunami(num_etherdevs)
    self.simple_disk = SimpleDisk(disk=Parent.tsunami.disk0.image)
    self.intrctrl = IntrControl()
    self.memctrl = MemoryController()
    self.sim_console = SimConsole(listener=ConsoleListener(port=3456))
    self.kernel = env.get('KERNEL', binary('vmlinux'))
    self.pal = env.get('PALCODE',  binary('ts_osfpal'))
    self.console = env.get('CONSOLE', binary('console'))
    self.boot_osflags = 'root=/dev/hda1 console=ttyS0'

    return self

def MakeSystem(cpus = SimpleCPU, num_etherdevs=1):
    self = TsunamiSystem(num_etherdevs)
    
    if isinstance(cpus, (list, tuple)):
        self.cpu = [ cpu() for cpu in cpus ]
        self.tsunami.console.cpu = Parent.cpu[0]
        self.intrctrl.cpu = Parent.cpu[0]
    else:
        self.cpu = cpus()

    return self
