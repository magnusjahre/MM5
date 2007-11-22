from m5 import *
from Config import *

#if env['NUMCPUS'] != '1':
#    env.setdefault('WRITE_BUFFERS', 32)

if 'WRITE_BUFFERS' in env:
    BaseCache.write_buffers = env['WRITE_BUFFERS']

class BaseL1Cache(BaseCache):
    in_bus = NULL
    size = '128kB'
    assoc = 2
    block_size = 64
    tgts_per_mshr = 16
    if env['NUMCPUS'] != '1':
        protocol = CoherenceProtocol(protocol = 'moesi')

class IL1(BaseL1Cache):
    latency = 1 * Parent.clock.period
    size = '64kB'
    mshrs = 8

class DL1(BaseL1Cache):
    latency = 3 * Parent.clock.period
    size = '32kB'
    mshrs = 32

class L2Bus(Bus):
    width = 64
    clock = Parent.clock

class L2(BaseCache):
    size = env.get('L2SIZE', '2MB')
    assoc = 8
    block_size = 64
    latency = 25 * Parent.clock.period
    mshrs = 40
    tgts_per_mshr = 16

class MemoryBus(Bus):
    width = 16
    clock = '400MHz'

class FSBus(MemoryBus):
    width = 16
    clock = '400MHz'

class HTBus(MemoryBus):
    width = 16
    clock = '400MHz'

class PCIEBus(Bus):
    width = 4
    clock = '400MHz'

class PCIXBus(Bus):
    width = 8
    clock = '133.333333MHz'

class FreeBridge(BusBridge):
    max_buffer = 16

class ChipBridge(BusBridge):
    max_buffer = 16
    latency = '75ns'

class ChipsetBridge(BusBridge):
    max_buffer = 16
    latency = '90ns'

class IoBridge(BusBridge):
    max_buffer = 16
    latency = '180ns'

class BaseMainMem(BaseMemory):
    in_bus = Parent.membus
    addr_range = [ Parent.physmem.range ]

class OffChipMem(BaseMainMem):
    latency = '40ns'

class OnChipMem(BaseMainMem):
    latency = '25ns'

def MemoryBase(System, **kwargs):
    self = System(**kwargs)
    self.membus = MemoryBus()
    self.tsunami.console.io_bus = Parent.pcibus
    self.tsunami.cchip.io_bus = Parent.membus
    self.tsunami.pchip.io_bus = Parent.membus
    self.tsunami.pciconfig.io_bus = Parent.pcibus
    self.tsunami.fake_smchip.io_bus = Parent.pcibus
    self.tsunami.fake_uart1.io_bus = Parent.pcibus
    self.tsunami.fake_uart2.io_bus = Parent.pcibus
    self.tsunami.fake_uart3.io_bus = Parent.pcibus
    self.tsunami.fake_uart4.io_bus = Parent.pcibus
    self.tsunami.fb.io_bus = Parent.pcibus
    self.tsunami.io.io_bus = Parent.membus
    self.tsunami.uart.io_bus = Parent.pcibus
    self.tsunami.ide.io_bus = Parent.pcibus
    return self

def MemoryL2Base(System, **kwargs):
    self = MemoryBase(System, **kwargs)
    self.l2bus = L2Bus()
    self.l2 = L2(in_bus=Parent.l2bus, out_bus=Parent.membus)
    if 'SPLIT' in env and (env['MEMORY'] == 'OCP' or env['MEMORY'] == 'OCC'):
	self.l2.split = True
        self.l2.split_size = env['SPLIT']

        if env.get('LIFO', False):
            self.l2.lifo = True

            if env.get('TWO_Q', False):
                self.l2.two_queue = True
    return self

def MemoryStandard(System, **kwargs):
    self = MemoryL2Base(System, **kwargs)
    self.ram = OffChipMem(in_bus=Parent.membus)
    self.nsbus = PCIEBus()
    self.iobus = PCIEBus()
    self.pcibus = PCIXBus()
    self.northbridge = ChipBridge(in_bus=Parent.membus, out_bus=Parent.nsbus)
    self.southbridge = ChipsetBridge(in_bus=Parent.nsbus, out_bus=Parent.iobus)
    self.nibridge = [ IoBridge(in_bus=Parent.iobus, out_bus=Parent.nibus[i]) \
                      for i in xrange(len(self.tsunami.etherdev)) ]
    self.pcibridge = IoBridge(in_bus=Parent.iobus, out_bus=Parent.pcibus)
    return self

def MemoryHyperTransport(System, **kwargs):
    self = MemoryL2Base(System, **kwargs)
    self.ram = OnChipMem(in_bus=Parent.membus)
    self.htbus = HTBus()
    self.pcibus = PCIXBus()
    self.htbridge = ChipBridge(in_bus=Parent.membus, out_bus=Parent.htbus)
    self.pcibridge = IoBridge(in_bus=Parent.htbus, out_bus=Parent.pcibus)
    return self

def MemoryOnChip(System, **kwargs):
    self = MemoryL2Base(System, **kwargs)
    self.ram = OnChipMem(in_bus=Parent.membus)
    self.htbus = HTBus()
    self.pcibus = PCIXBus()
    self.htbridge = ChipBridge(in_bus=Parent.membus, out_bus=Parent.htbus)
    self.pcibridge = IoBridge(in_bus=Parent.htbus, out_bus=Parent.pcibus)
    return self

def MemorySTX(System, **kwargs):
    self = MemoryStandard(System, **kwargs)
    self.nibus = [ PCIXBus() for i in xrange(len(self.tsunami.etherdev)) ]
    for i in xrange(len(self.tsunami.etherdev)):
        self.tsunami.etherdev[i].io_bus = Parent.nibus[i]
    return self

def MemorySTE(System, **kwargs):
    self = MemoryStandard(System, **kwargs)
    self.nibus = [ PCIEBus() for i in xrange(len(self.tsunami.etherdev)) ]
    for i in xrange(len(self.tsunami.etherdev)):
        self.tsunami.etherdev[i].io_bus = Parent.nibus[i]
    return self

def MemoryHTX(System, **kwargs):
    self = MemoryHyperTransport(System, **kwargs)
    self.nibus = [ PCIXBus() for i in xrange(len(self.tsunami.etherdev)) ]
    self.nibridge = [ ChipsetBridge(in_bus=Parent.htbus,
                                    out_bus=Parent.nibus[i]) ]
    for i in xrange(len(self.tsunami.etherdev)):
        self.tsunami.etherdev[i].io_bus = Parent.nibus[i]
    return self

def MemoryHTE(System, **kwargs):
    self = MemoryHyperTransport(System, **kwargs)
    self.nibus = [ PCIEBus() for i in xrange(len(self.tsunami.etherdev)) ]
    self.nibridge = [ ChipsetBridge(in_bus=Parent.htbus,
                                    out_bus=Parent.nibus[i]) \
                      for i in xrange(len(self.tsunami.etherdev)) ]
    for i in xrange(len(self.tsunami.etherdev)):
        self.tsunami.etherdev[i].io_bus = Parent.nibus[i]
    return self

def MemoryHTD(System, **kwargs):
    self = MemoryHyperTransport(System, **kwargs)
    self.nibus = [ HTBus() for i in xrange(len(self.tsunami.etherdev)) ] 
    self.nibridge = [ ChipBridge(in_bus=Parent.membus, out_bus=Parent.nibus[i]) \
                      for i in xrange(len(self.tsunami.etherdev)) ]
    for i in xrange(len(self.tsunami.etherdev)):
        self.tsunami.etherdev[i].io_bus = Parent.nibus[i]
    return self

def MemoryOCM(System, **kwargs):
    self = MemoryOnChip(System, **kwargs)
    for i in xrange(len(self.tsunami.etherdev)):
        self.tsunami.etherdev[i].io_bus = Parent.membus
    return self

def MemoryOCC(System, **kwargs):
    self = MemoryOnChip(System, **kwargs)
    for i in xrange(len(self.tsunami.etherdev)):
        self.tsunami.etherdev[i].io_bus = Parent.l2bus
        if env.get('SPLIT_PAYLOAD', False):
            self.tsunami.etherdev[i].payload_bus = Parent.membus
    self.l2.addr_range = [ AddrRange(0x0, 0x80008ffffff),
                           AddrRange(0x8000a000000, 0x803ffffffff) ]
    return self

def MemConfig(System, **kwargs):
    if env['MEMORY'] == 'STX':
        return MemorySTX(System, **kwargs)
    elif env['MEMORY'] == 'STE':
        return MemorySTE(System, **kwargs)
    elif env['MEMORY'] == 'HTX':
        return MemoryHTX(System, **kwargs)
    elif env['MEMORY'] == 'HTE':
        return MemoryHTE(System, **kwargs)
    elif env['MEMORY'] == 'HTD':
        return MemoryHTD(System, **kwargs)
    elif env['MEMORY'] == 'OCM':
        return MemoryOCM(System, **kwargs)
    elif env['MEMORY'] == 'OCC' or env['MEMORY'] == 'OCP':
        return MemoryOCC(System, **kwargs)
    else:
        panic('Invalid configuration for MEMORY') 

def ProcessorCache(CPUType, **kwargs):
    self = CPUType(**kwargs)
    self.dcache = DL1(out_bus=Parent.l2bus)
    self.icache = IL1(out_bus=Parent.l2bus)
    return self
