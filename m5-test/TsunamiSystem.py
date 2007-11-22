from m5 import *
import os
from SysPaths import *
from DetailedUniConfig import *

# Base for tests is directory containing this file.
test_base = os.path.dirname(__file__)

class IdeControllerPciData(PciConfigData):
    VendorID = 0x8086
    DeviceID = 0x7111
    Command = 0x0
    Status = 0x280
    Revision = 0x0
    ClassCode = 0x01
    SubClassCode = 0x01
    ProgIF = 0x85
    BAR0 = 0x00018101
    BAR1 = 0x00018109
    BAR2 = 0x00000001
    BAR3 = 0x00000001
    BAR4 = 0x00018119
    BAR5 = 0x00000000
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
    BAR0 = 0x00018001
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

class LinuxRootDisk(IdeDisk):
    raw_image = RawDiskImage(image_file=disk('linux.img'),
                             read_only=True)
    image = CowDiskImage(child=Parent.raw_image, read_only=False)

class LinuxSwapDisk(IdeDisk):
    raw_image = RawDiskImage(image_file = disk('swap.img'),
                                  read_only=True)
    image = CowDiskImage(child = Parent.raw_image, read_only=False)

class FreeBSDRootDisk(IdeDisk):
    raw_image = RawDiskImage(image_file=disk('freebsd.img'),
                             read_only=True)
    image = CowDiskImage(child=Parent.raw_image, read_only=False)

class BridgePciData(PciConfigData):
    VendorID = 0x1106
    DeviceID = 0x0586
    Command = 0x0
    Status = 0x280
    Revision = 0x0
    ClassCode = 0x06
    SubClassCode = 0x01
    ProgIF = 0x00

class BaseTsunami(Tsunami):
    cchip = TsunamiCChip(addr=0x801a0000000)
    pchip = TsunamiPChip(addr=0x80180000000)
    pciconfig = PciConfigAll(addr=0x801fe000000)
    fake_sm_chip = IsaFake(addr=0x801fc000370)
    
    fake_uart1 = IsaFake(addr=0x801fc0002f8)
    fake_uart2 = IsaFake(addr=0x801fc0003e8)
    fake_uart3 = IsaFake(addr=0x801fc0002e8)
    fake_uart4 = IsaFake(addr=0x801fc0003f0)
   
    fake_ppc = IsaFake(addr=0x801fc0003bc)
  
    fake_OROM = IsaFake(addr=0x800000a0000, size=0x60000)

    fake_pnp_addr = IsaFake(addr=0x801fc000279)
    fake_pnp_write = IsaFake(addr=0x801fc000a79)
    fake_pnp_read0 = IsaFake(addr=0x801fc000203)
    fake_pnp_read1 = IsaFake(addr=0x801fc000243)
    fake_pnp_read2 = IsaFake(addr=0x801fc000283)
    fake_pnp_read3 = IsaFake(addr=0x801fc0002c3)
    fake_pnp_read4 = IsaFake(addr=0x801fc000303)
    fake_pnp_read5 = IsaFake(addr=0x801fc000343)
    fake_pnp_read6 = IsaFake(addr=0x801fc000383)
    fake_pnp_read7 = IsaFake(addr=0x801fc0003c3)

    fake_ata0 = IsaFake(addr=0x801fc0001f0)
    fake_ata1 = IsaFake(addr=0x801fc000170)
   
    fb = BadDevice(addr=0x801fc0003d0, devicename='FrameBuffer')
    io = TsunamiIO(addr=0x801fc000000)
    uart = Uart8250(addr=0x801fc0003f8)
    ethernet = NSGigE(configdata=NSGigEPciData(),
                      pci_bus=0, pci_dev=1, pci_func=0)
    etherint = NSGigEInt(device=Parent.ethernet)
    console = AlphaConsole(addr=0x80200000000, disk=Parent.simple_disk)
    bridge = PciFake(configdata=BridgePciData(), pci_bus=0, pci_dev=2, pci_func=0)

class FreeBSDTsunami(BaseTsunami):
    disk0 = FreeBSDRootDisk(delay='0us', driveID='master')
    ide = IdeController(disks=[Parent.disk0],
                        configdata=IdeControllerPciData(),
                        pci_func=0, pci_dev=0, pci_bus=0)

class LinuxTsunami(BaseTsunami):
    disk0 = LinuxRootDisk(delay='0us', driveID='master')
    ide = IdeController(disks=[Parent.disk0],
                        configdata=IdeControllerPciData(),
                        pci_func=0, pci_dev=0, pci_bus=0)

class FreebsdSystem(FreebsdSystem):
    physmem = PhysicalMemory(range = AddrRange('128MB'))
    tsunami = FreeBSDTsunami()
    simple_disk = SimpleDisk(disk=Parent.tsunami.disk0.raw_image)
    intrctrl = IntrControl()
    memctrl = MemoryController()
    cpu = SimpleCPU()
    sim_console = SimConsole(listener=ConsoleListener(port=3456))
    kernel = binary('freebsd')
    pal = binary('ts_osfpal')
    console = binary('console')
    boot_osflags = ''
    readfile = os.path.join(test_base, 'halt.sh')

class LinuxSystem(LinuxSystem):
    physmem = PhysicalMemory(range = AddrRange('128MB'))
    tsunami = LinuxTsunami()
    simple_disk = SimpleDisk(disk=Parent.tsunami.disk0.raw_image)
    intrctrl = IntrControl()
    memctrl = MemoryController()
    cpu = SimpleCPU()
    sim_console = SimConsole(listener=ConsoleListener(port=3456))
    kernel = binary('vmlinux')
    pal = binary('ts_osfpal')
    console = binary('console')
    boot_osflags = 'root=/dev/hda1 console=ttyS0'
    readfile = os.path.join(test_base, 'halt.sh')

class BaseL1Cache(BaseCache):
    size = '32kB'
    assoc = 2
    block_size = 64
    tgts_per_mshr = 16

class IL1Cache(BaseL1Cache):
    latency = Parent.clock.period
    mshrs = 8

class DL1Cache(BaseL1Cache):
    latency = 3 * Parent.clock.period
    mshrs = 32

class L2Cache(BaseCache):
    size = '256kB'
    assoc = 8
    block_size = 64
    latency = 12 * Parent.clock.period
    mshrs = 40
    tgts_per_mshr = 16

class L3Cache(BaseCache):
    size = '2MB'
    assoc = 8
    block_size = 64
    latency = 25 * Parent.clock.period
    mshrs = 40
    tgts_per_mshr = 16

class ToL2Bus(Bus):
    width = 64
    clock = Parent.clock.period

class ToL3Bus(Bus):
    width = 64
    clock = Parent.clock.period

class SDRAM(BaseMemory):
    latency = 40 * Parent.clock.period
    uncacheable_latency = 1000 * Parent.clock.period
    addr_range = [Parent.physmem.range]

class DetailedCPU(DetailedCPU):
    toL3Bus = ToL3Bus()
    l3 = L3Cache(in_bus=Parent.toL3Bus, out_bus=Parent.toMemBus)
    toL2Bus = ToL2Bus()
    l2 = L2Cache(in_bus=Parent.toL2Bus, out_bus=Parent.toL3Bus)
    dcache = DL1Cache(out_bus=Parent.toL2Bus)
    icache = IL1Cache(out_bus=Parent.toL2Bus)

def DetailedSystem(System):
    self = System()
    self.cpu = DetailedCPU()
    self.toMemBus = ToMemBus()
    self.ram = SDRAM(in_bus=Parent.toMemBus)
    tsunami = self.tsunami
    tsunami.cchip.io_bus = Parent.toMemBus
    tsunami.pchip.io_bus = Parent.toMemBus
    tsunami.pciconfig.io_bus = Parent.toMemBus
    tsunami.fake_sm_chip.io_bus = Parent.toMemBus
    tsunami.fake_uart1.io_bus = Parent.toMemBus
    tsunami.fake_uart2.io_bus = Parent.toMemBus
    tsunami.fake_uart3.io_bus = Parent.toMemBus
    tsunami.fake_uart4.io_bus = Parent.toMemBus
    tsunami.fb.io_bus = Parent.toMemBus
    tsunami.io.io_bus = Parent.toMemBus
    tsunami.uart.io_bus = Parent.toMemBus
    tsunami.console.io_bus = Parent.toMemBus
    tsunami.ethernet.io_bus = Parent.toMemBus
    tsunami.ide.io_bus = Parent.toMemBus
    #tsunami.ethernet.configdata.io_bus = Parent.toMemBus
    #tsunami.ide.configdata.io_bus = Parent.toMemBus
    return self

DetailedLinuxSystem = DetailedSystem(LinuxSystem)
DetailedFreebsdSystem = DetailedSystem(FreebsdSystem)

BaseCPU.itb = AlphaITB()
BaseCPU.dtb = AlphaDTB()
BaseCPU.mem = Parent.memctrl
BaseCPU.system = Parent.any

class TsunamiRoot(Root):
    pass
