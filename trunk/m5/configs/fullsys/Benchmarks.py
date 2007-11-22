from m5 import *
from Config import *

TestBox = None

# default to no test
env.setdefault('TEST', 'NONE')
env.setdefault('NAT', 'False')
env.setdefault('SINIC', 'False')

#
# no test
#
if env['TEST'] == 'NONE':
    pass

#
# netperf stream benchmark configuration
#
elif env['TEST'] == 'NETPERF_STREAM':
    TestBox = 'Client'

    if env['NAT']:
        env['SERVER_SCRIPT'] = script('nat-netperf-server.rcS')
        env['CLIENT_SCRIPT'] = script('nat-netperf-stream-client.rcS')
        env['NATBOX_SCRIPT'] = script('natbox-netperf.rcS')
    else:
        env['SERVER_SCRIPT'] = script('netperf-server.rcS')
        env['CLIENT_SCRIPT'] = script('netperf-stream-client.rcS')

#
# netperf stream no-touch benchmark configuration
#
elif env['TEST'] == 'NETPERF_STREAM_NT':
    TestBox = 'Client'

    if env['NAT']:
        panic('no NAT')
    else:
        env['SERVER_SCRIPT'] = script('netperf-server.rcS')
        env['CLIENT_SCRIPT'] = script('netperf-stream-nt-client.rcS')

#
# netperf maerts benchmark configuration
#
elif env['TEST'] == 'NETPERF_MAERTS':
    TestBox = 'Client'

    if env['NAT']:
        env['SERVER_SCRIPT'] = script('nat-netperf-server.rcS')
        env['CLIENT_SCRIPT'] = script('nat-netperf-maerts-client.rcS')
        env['NATBOX_SCRIPT'] = script('natbox-netperf.rcS')
    else:
        env['SERVER_SCRIPT'] = script('netperf-server.rcS')
        env['CLIENT_SCRIPT'] = script('netperf-maerts-client.rcS')

#
# netperf request/response benchmark configuration
#
elif env['TEST'] == 'NETPERF_RR':
    TestBox = 'Client'
    if env['NAT']:
        panic('do not have netperf_rr on nat yet - make yer own rcS!')
    else:
        env['SERVER_SCRIPT'] = script('netperf-server.rcS')
        env['CLIENT_SCRIPT'] = script('netperf-rr.rcS')

#
# standard surge benchmark configuration
#
elif env['TEST'] == 'SURGE_STANDARD':
    TestBox = 'Server'

    env['CLIENT_MEMSIZE'] = '256MB'
    env['SERVER_MEMSIZE'] = '512MB'
    MyTsunami.disk2.raw_image.image_file = disk('surge-fileset.img')
    if env['NAT']:
        panic('do not have standard surge on nat yet - make yer own rcS!')
    else:
        env['SERVER_SCRIPT'] = script('surge-server.rcS')
        env['CLIENT_SCRIPT'] = script('surge-client.rcS')

#
# specweb99-like surge benchmark configuration
#
elif env['TEST'] == 'SURGE_SPECWEB':
    TestBox = 'Server'
    
    env['CLIENT_MEMSIZE'] = '256MB'
    env['SERVER_MEMSIZE'] = '512MB'
    if env['NAT']:
        env['SERVER_SCRIPT'] = script('nat-spec-surge-server.rcS')
        env['CLIENT_SCRIPT'] = script('nat-spec-surge-client.rcS')
        env['NATBOX_SCRIPT'] = script('natbox-spec-surge.rcS')
    else:
        env['SERVER_SCRIPT'] = script('spec-surge-server.rcS')
        env['CLIENT_SCRIPT'] = script('spec-surge-client.rcS')

#
# NFS/nhfsstone benchmark configuration
#
elif env['TEST'] == 'NHFS':
    TestBox = 'Server'

    env['SERVER_MEMSIZE'] = '512MB'
    if env['NAT']:
        panic('do not have nfs/bonnie on NAT yet!  make yer own rcS!')
    else:
        env['SERVER_SCRIPT'] = script('nfs-server-nhfsstone.rcS')
        env['CLIENT_SCRIPT'] = script('nfs-client-nhfsstone.rcS')
#
# NFS/bonnie++ benchmark configuration
#
elif env['TEST'] == 'NFS':
    TestBox = 'Server'

    env['SERVER_MEMSIZE'] = '900MB'
    if env['NAT']:
        panic('do not have nfs/bonnie on NAT yet!  make yer own rcS!')
    else:
        env['SERVER_SCRIPT'] = script('nfs-server.rcS')
        env['CLIENT_SCRIPT'] = script('nfs-client.rcS')

#
# NFS/bonnie++ small block size benchmark configuration
#
elif env['TEST'] == 'NFS_SB':
    TestBox = 'Server'

    env['SERVER_MEMSIZE'] = '1024MB'
    if env['NAT']:
        panic('No NFS/NAT config file')
    else:
        env['SERVER_SCRIPT'] = script('nfs-server.rcS')
        env['CLIENT_SCRIPT'] = script('nfs-client-smallb.rcS')

#
# NFS/bonnie++ benchmark configuration with TCP
#
elif env['TEST'] == 'NFS_TCP':
    TestBox = 'Server'

    env['SERVER_MEMSIZE'] = '1024MB'
    if env['NAT']:
        panic('do not have nfs/bonnie on NAT yet!  make yer own rcS!')
    else:
        env['SERVER_SCRIPT'] = script('nfs-server.rcS')
        env['CLIENT_SCRIPT'] = script('nfs-client-tcp.rcS')

#
# NFS/bonnie++ small block size benchmark configuration
#
elif env['TEST'] == 'NFS_TCP_SB':
    TestBox = 'Server'

    env['SERVER_MEMSIZE'] = '1024MB'
    if env['NAT']:
        panic('No NFS/NAT config file')
    else:
        env['SERVER_SCRIPT'] = script('nfs-server.rcS')
        env['CLIENT_SCRIPT'] = script('nfs-client-tcp-smallb.rcS')

#
# NFS/dbench benchmark configuration
#
elif env['TEST'] == 'NFS_DBENCH':
    TestBox = 'Server'

    env['SERVER_MEMSIZE'] = '900MB'
    if env['NAT']:
        panic('do not have nfs/dbench on NAT yet!  make yer own rcS!')
    else:
        env['SERVER_SCRIPT'] = script('nfs-server.rcS')
        env['CLIENT_SCRIPT'] = script('nfs-client-dbench.rcS')

#
# ISCSI/dbench benchmark configuration
#
elif env['TEST'] == 'ISCSI_INIT_DBENCH':
    TestBox = 'Client'

    env['SERVER_MEMSIZE'] = '900MB'
    if env['NAT']:
        panic('do not have iscsi/dbench on NAT yet!  make yer own rcS!')
    else:
        env['SERVER_SCRIPT'] = script('iscsi-server.rcS')
        env['CLIENT_SCRIPT'] = script('iscsi-client.rcS')

#
# ISCSI/dbench benchmark configuration
#
elif env['TEST'] == 'ISCSI_TARGET_DBENCH':
    TestBox = 'Server'

    env['SERVER_MEMSIZE'] = '900MB'
    if env['NAT']:
        panic('do not have iscsi/dbench on NAT yet!  make yer own rcS!')
    else:
        env['SERVER_SCRIPT'] = script('iscsi-server.rcS')
        env['CLIENT_SCRIPT'] = script('iscsi-client.rcS')

#
# Tests for validation
#
elif env['TEST'] == 'VAL_ACC_DELAY':
    env['SERVER_MEMSIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('devtime.rcS')
elif env['TEST'] == 'VAL_ACC_DELAY2':
    env['SERVER_MEMSIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('devtimewmr.rcS')
elif env['TEST'] == 'VAL_MEM_LAT':
    env['SERVER_MEMSIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('micro_memlat.rcS')
elif env['TEST'] == 'VAL_MEM_LAT2MB':
    env['SERVER_MEMORY_SIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('micro_memlat2mb.rcS')
elif env['TEST'] == 'VAL_MEM_LAT8MB':
    env['SERVER_MEMORY_SIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('micro_memlat8mb.rcS')
elif env['TEST'] == 'VAL_MEM_LAT8':
    env['SERVER_MEMORY_SIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('micro_memlat8.rcS')
elif env['TEST'] == 'VAL_TLB_LAT':
    env['SERVER_MEMORY_SIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('micro_tlblat.rcS')
elif env['TEST'] == 'VAL_SYS_LAT':
    env['SERVER_MEMSIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('micro_syscall.rcS')
elif env['TEST'] == 'VAL_CTX_LAT':
    env['SERVER_MEMORY_SIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('micro_ctx.rcS')
elif env['TEST'] == 'VAL_STREAM':
    env['SERVER_MEMORY_SIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('micro_stream.rcS')
elif env['TEST'] == 'VAL_STREAMSCALE':
    env['SERVER_MEMORY_SIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('micro_streamscale.rcS')
elif env['TEST'] == 'VAL_STREAMCOPY':
    env['SERVER_MEMORY_SIZE'] = '512MB'
    env['SERVER_SCRIPT'] = script('micro_streamcopy.rcS')
elif env['TEST'] == 'PING':
    TestBox = 'Server'
    env['SERVER_MEMORY_SIZE'] = '512MB'
    if env['NAT']:
        panic('No NFS/NAT config file')
    else:
        env['SERVER_SCRIPT'] = script('ping-server.rcS')
        env['CLIENT_SCRIPT'] = script('ping-client.rcS')
#
# No configuration defined!
#

elif env['TEST'] == 'BOOTME':  # ADDED BY MAGNUS FOR TESTING
  TestBox = 'Server'
  env['SERVER_MEMORY_SIZE'] = '512MB'
  env['CLIENT_SCRIPT'] = script('boot.rcS'  )
  env['SERVER_SCRIPT'] = script('boot.rcS')
  

else:
    panic('must define a valid test!')

env.setdefault('MEMSIZE', '128MB')
env.setdefault('CLIENT_MEMSIZE', env['MEMSIZE'])
env.setdefault('SERVER_MEMSIZE', env['MEMSIZE'])
env.setdefault('NATBOX_MEMSIZE', env['MEMSIZE'])

if 'CKPT_FILE' not in env and 'CKPT_NUM' in env \
    and 'CKPT_DIR' in env and 'CKPT_JOB' in env:

    import re
    from os import listdir
    from os.path import isdir, join as joinpath

    if 'CKPT_DIR' not in env:
        panic("CKPT_DIR not set!")

    if 'CKPT_JOB' not in env:
        panic("CKPT_JOB not set!")

    cptdir = joinpath(env['CKPT_DIR'], env['CKPT_JOB'])
    if not isdir(cptdir):
        panic('checkpoint dir %s does not exist' % cptdir)

    dirs = listdir(cptdir)
    expr = re.compile('cpt.([0-9]*)')
    cpts = []
    for dir in dirs:
        match = expr.match(dir)
        if match:
            cpts.append(match.group(1))

    cpts.sort(lambda a,b: cmp(long(a), long(b)))

    number = int(env['CKPT_NUM'])
    if number > len(cpts):
        panic('Checkpoint %s not found' % number)

    env['CKPT_FILE'] = joinpath(cptdir, 'cpt.%s' % cpts[number - 1])
