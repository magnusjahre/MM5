from m5 import *
if 'JOBNAME' in env:
    from jobfile import JobFile
    conf = JobFile(env.get('JOBFILE', 'test.py'))
    job = conf.find(env['JOBNAME'])
    env.update(job.env)
    if 'STATS_JOBNAME' not in env:
        env['STATS_JOBNAME'] = env['JOBNAME']
    if job.checkpoint:
        env['CKPT_JOB'] = job.checkpoint.name
        
if env.get('POOLJOB', False):
    env.setdefault('USE_DATABASE', True)

if not build_env['FULL_SYSTEM']:
    panic("M5 must be built with FULL_SYSTEM to use these configurations.")

from Config import *
from Benchmarks import *
from Memory import *
from System import *
from P4 import *
from Monet import *

DriveSystem = MakeSystem()
DriveSystem.cpu.width = 8

if env['NAT']:
    num_ethers = 2
else:
    num_ethers = 1

env.setdefault('SYSTEM', 'Simple')

if env['SYSTEM'] == 'Simple':
    CPU = SimpleCPU
    if env['SIMPLE_DEDICATED']:
        CPUs = [ CPU() for i in xrange(int(env['NUMCPUS'])+1) ]
        CPUs[1].clock = '1GHz'
    else :
        CPUs = [ CPU() for i in xrange(int(env['NUMCPUS'])) ]
    TestSystem = MakeSystem(CPUs, num_ethers)
    
elif env['SYSTEM'] == 'Cache':
    CPU = ProcessorCache(CacheCPU)
    CPUs = [ CPU() for i in xrange(int(env['NUMCPUS'])) ]
    TestSystem = MakeSystem(CPUs, num_ethers)
    TestSystem = MemConfig(TestSystem)

elif env['SYSTEM'] == 'P4Simple':
    TestSystem = TsunamiSystem(num_ethers)
    TestSystem.cpu = P4SimpleCache()
    TestSystem = P4Memory(TestSystem)
    
elif env['SYSTEM'] == 'P4Full':
    TestSystem = TsunamiSystem(num_ethers)
    TestSystem.cpu = P4FullCache()
    TestSystem = P4Memory(TestSystem)

elif env['SYSTEM'] == 'MonetSimple':
    TestSystem = TsunamiSystem(num_ethers)
    TestSystem.cpu = MonetSimpleCache(CacheCPU)
    TestSystem = MonetMemory(TestSystem)
    
elif env['SYSTEM'] == 'MonetFull':
    TestSystem = TsunamiSystem(num_ethers)
    TestSystem.cpu = MonetFullCache(Monet)
    TestSystem = MonetMemory(TestSystem)
     
elif env['SYSTEM'] == 'Detailed':
    CPU = ProcessorCache(DetailedCPU)
    CPUs = [ CPU() for i in xrange(int(env['NUMCPUS'])) ]
    TestSystem = MakeSystem(CPUs, num_ethers)
    TestSystem = MemConfig(TestSystem)

elif env['SYSTEM'] == 'Sampler':
    TestSystem = TsunamiSystem(num_ethers)

    ccpu = CacheCPU(defer_registration=True)
    dcpu = DetailedCPU(defer_registration=True)
    proxycpu = ProcessorCache(SimObject)
    sampler = Sampler(periods = [1e9, 200e6])

    if env['SIMPLE_DEDICATED']:
        cpus = []
        cpus.append(ccpu(icache=Parent.proxy[0].icache,
                     dcache=Parent.proxy[0].dcache))

        cpus.append(DedicatedCPU())

        if env['NUMCPUS'] != 1:
            for i in xrange(1,int(env['NUMCPUS'])):
                cpus.append(ccpu(icache=Parent.proxy[i].icache,
                                 dcache=Parent.proxy[i].dcache))

        TestSystem.cpu = cpus
        p0cpus = []
        p0cpus.append(Parent.cpu[0])
        for i in xrange(2,int(env['NUMCPUS'])+1):
            p0cpus.append(Parent.cpu[i])

    else:
        TestSystem.cpu = [ ccpu(icache=Parent.proxy[i].icache,
                                 dcache=Parent.proxy[i].dcache)\
                            for i in xrange(int(env['NUMCPUS']))]
        p0cpus = [ Parent.cpu[i] \
                   for i in xrange(int(env['NUMCPUS'])) ]
        
    TestSystem.full = [ dcpu(icache=Parent.proxy[i].icache,
                             dcache=Parent.proxy[i].dcache)\
                        for i in xrange(int(env['NUMCPUS']))]
    
    TestSystem.proxy = [ proxycpu() for i in xrange(int(env['NUMCPUS'])) ]

    p1cpus = [ Parent.full[i] for i in xrange(int(env['NUMCPUS'])) ]

    TestSystem.sampler = [ sampler(phase0_cpus=p0cpus, phase1_cpus=p1cpus) ]
    TestSystem.tsunami.console.cpu = Parent.cpu[0]
    TestSystem.intrctrl.cpu = Parent.cpu[0]

    TestSystem = MemConfig(TestSystem)
        
elif env['SYSTEM'] == 'P4Sampler':
    TestSystem = TsunamiSystem(num_ethers)
    ccpu = CacheCPU(defer_registration=True)
    dcpu = Pentium4(defer_registration=True)
    scpu = P4SimpleCache(Sampler, periods = [1e9, 200e6])

    TestSystem.cpu = ccpu(icache=Parent.sampler.icache,
                          dcache=Parent.sampler.dcache)
    TestSystem.full = dcpu(icache=Parent.sampler.icache,
                           dcache=Parent.sampler.dcache)
    TestSystem.sampler = scpu(phase0_cpus = [ Parent.cpu ],
                              phase1_cpus = [ Parent.full ])
    TestSystem.tsunami.console.cpu = Parent.cpu
    TestSystem.intrctrl.cpu = Parent.cpu

    TestSystem = P4Memory(TestSystem)
    
elif env['SYSTEM'] == 'MonetSampler':
    TestSystem = TsunamiSystem(num_ethers)
    ccpu = CacheCPU(defer_registration=True)
    dcpu = Monet(defer_registration=True)
    proxycpu = MonetSimpleCache(SimObject)
    sampler = Sampler( periods = [650e6, 5e9])

    TestSystem.cpu = ccpu(icache=Parent.proxy.icache,
                          dcache=Parent.proxy.dcache)
    TestSystem.full = dcpu(icache=Parent.proxy.icache,
                           dcache=Parent.proxy.dcache)
    TestSystem.proxy = proxycpu()
    TestSystem.sampler = sampler(phase0_cpus = [ Parent.cpu ],
                                 phase1_cpus = [ Parent.full ])
    TestSystem.tsunami.console.cpu = Parent.cpu
    TestSystem.intrctrl.cpu = Parent.cpu
    TestSystem.physmem.range = AddrRange(env['SERVER_MEMSIZE'])

    TestSystem = MonetMemory(TestSystem)
else:
    panic("SYSTEM='%s' is not valid" % env['SYSTEM'])

if env.get('BINNING', False):
    TestSystem.bin = True

if 'SAMPLE_INTERVAL' in env:
    DetailedCPU.pc_sample_interval = env['SAMPLE_INTERVAL']

#
# Top level configuration
#
class NetRoot(Root):
    if 'DUMPFILE' in env:
        dump = EtherDump(file = env['DUMPFILE'])
    clock = env['FREQUENCY']
    #clock = '1THz'

#BaseCPU.clock = env['FREQUENCY']

NSGigE.rx_fifo_size = env.get('RX_FIFO_SIZE', '1MB')
NSGigE.tx_fifo_size = env.get('TX_FIFO_SIZE', '1MB')

class EtherLink(EtherLink):
    speed = env.get('LINK_SPEED', '10Gbps')
    delay = env.get('LINK_DELAY', '0ns')
    if 'DUMPFILE' in env:
        dump = Parent.dump
    
def DualRoot(ClientSystem, ServerSystem):
    self = NetRoot()
    self.client = ClientSystem()
    self.server = ServerSystem()
    self.client.physmem.range = AddrRange(env['CLIENT_MEMSIZE'])
    self.server.physmem.range = AddrRange(env['SERVER_MEMSIZE'])
    self.etherlink = EtherLink(int1 = Parent.server.tsunami.etherint[0],
                               int2 = Parent.client.tsunami.etherint[0])
    return self

def NatRoot(NatSystem, ClientSystem, ServerSystem):
    self = NetRoot()
    self.client = ClientSystem()
    self.server = ServerSystem()
    self.natbox = NatSystem()
    self.client.physmem.range = AddrRange(env['CLIENT_MEMSIZE'])
    self.server.physmem.range = AddrRange(env['SERVER_MEMSIZE'])
    self.natbox.physmem.range = AddrRange(env['NATBOX_MEMSIZE'])
    
    self.natbox.tsunami.etherdev[1].configdata.InterruptLine = 0x1d
    self.etherlink1 = EtherLink(int1 = Parent.server.tsunami.etherint[0],
                                int2 = Parent.natbox.tsunami.etherint[0])
    self.etherlink2 = EtherLink(int1 = Parent.natbox.tsunami.etherint[1],
                                int2 = Parent.client.tsunami.etherint[0])
    return self

if env['TEST'] == 'NONE':
    if env['TEST_NONE'] == 'DUAL':
        if env['SYSTEM'] == 'Simple':
            DriveSystem.cpu.width = 1
        root = DualRoot(ClientSystem = TestSystem(),
                        ServerSystem = DriveSystem())
        #ServerSystem.physmem.range = AddrRange('512MB')
    elif env['TEST_NONE'] == 'TAP':
        class TapSystem(TestSystem):
            tap = EtherTap(peer = Parent.tsunami.etherint[0])
        root = NetRoot(system = TapSystem())
    else:
        root = NetRoot(system = TestSystem())
elif env['NAT']:
    root = NatRoot(ClientSystem = DriveSystem(readfile=env['CLIENT_SCRIPT']),
                   ServerSystem = DriveSystem(readfile=env['SERVER_SCRIPT']),
                   NatSystem = TestSystem(readfile=env['NATBOX_SCRIPT']))
elif env['TEST'].find('VAL_') != -1:
    root = NetRoot(system = TestSystem(readfile=env['SERVER_SCRIPT']))
elif TestBox == 'Server':
    root = DualRoot(ClientSystem = DriveSystem(readfile=env['CLIENT_SCRIPT']),
                    ServerSystem = TestSystem(readfile=env['SERVER_SCRIPT']))
elif TestBox == 'Client':
    root = DualRoot(ClientSystem = TestSystem(readfile=env['CLIENT_SCRIPT']),
                    ServerSystem = DriveSystem(readfile=env['SERVER_SCRIPT']))
else:
    panic('System type not properly defined!')

if env['DEDICATED'] and not env['SINIC']:
    TestSystem.boot_osflags = 'root=/dev/hda1 console=ttyS0 app_cpu_mask=cpu0'
    for i in xrange(len(TestSystem.tsunami.etherdev)):
        TestSystem.tsunami.etherdev[i].m5reg = 0x1

LinuxSystem.init_param = env.get('INITPARAM', '0')

root.pseudo_inst = PseudoInst()
if env.get('NO_STATISTICS_INST', False):
    root.pseudo_inst.statistics = False
if env.get('NO_CHECKPOINT_INST', False):
    root.pseudo_inst.checkpoint = False

root.stats.descriptions = 'no'
if 'STATS_PROJECT' in env:
    root.stats.project_name = env['STATS_PROJECT']
if 'STATS_TEXT' in env:
    root.stats.text_file = env['STATS_TEXT']
if 'STATS_JOBNAME' in env:
    root.stats.simulation_name = env['STATS_JOBNAME']
if env.get('USE_DATABASE', False) and 'STATS_MYSQL_DB' in env:
    root.stats.mysql_db = env.get('STATS_MYSQL_DB')
    root.stats.mysql_host = env.get('STATS_MYSQL_HOST', 'poolfs.pool')

if env.get('SAMPLING', False):
    root.stats.dump_cycle = 1e9
    root.stats.dump_period = 20e6
    root.stats.dump_reset = True

if 'MAX_CHECKPOINTS' in env:
    Serialize.count = env['MAX_CHECKPOINTS']

if 'CKPT_FILE' in env:
    from os.path import isdir
    if not isdir(env['CKPT_FILE']):
        panic("%s isn't a valid checkpoint!" % env['CKPT_FILE'])
    root.checkpoint = env['CKPT_FILE']
