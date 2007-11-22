from m5 import *
# binary/disk image stuff
env.setdefault('SYSTEMDIR', '/home/jahre/skole/diplom/m5_1.1/fullsim_files')
env.setdefault('BINDIR', env['SYSTEMDIR'] + '/binaries')
env.setdefault('DISKDIR', env['SYSTEMDIR'] + '/disks')
env.setdefault('BOOTDIR', env['SYSTEMDIR'] + '/boot')
env.setdefault('NUMCPUS', '1')
env.setdefault('NO_ALLOCATE', 'True')
env.setdefault('SIMPLE_DEDICATED', 'False')

def disk(file):
    return '%s/%s' % (env['DISKDIR'], file)

def binary(file):
    return '%s/%s' % (env['BINDIR'], file)

def script(file):
    return '%s/%s' % (env['BOOTDIR'], file)

env.setdefault('FREQUENCY', '2GHz')
