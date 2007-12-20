from m5 import *
import os.path
from os.path import join as joinpath
from shutil import copy

# Provide a processor count to avoid errors
if 'NP' not in env:
    print >>sys.stderr, "Warning: No processor count given on command line, using default with 2 processors\nUse -ENP=4 or similar to change."
    env['NP'] = 2

rootdir = os.getenv("BMROOT")
if rootdir == None:
  print >>sys.stderr, "Envirionment variable BMROOT not set. Quitting..."
  sys.exit(-1)
    
env.setdefault('ROOTDIR', rootdir+'/splash2/codes/')

benchmarkNames = ['Cholesky',
                  'FFT',
                  'LUContig',
                  'LUNoncontig',
                  'Radix',
                  'Barnes',
                  'FMM',
                  'OceanContig',
                  'OceanNoncontig',
                  'Raytrace',
                  'WaterNSquared',
                  'WaterSpatial']

# (fast forward cycles, detailed simulation cycles)
fastforward = {
'Cholesky':(100000000, 100000000),
'FFT':(0, 1000000000), 
'LUContig':(0, 500000000), 
'LUNoncontig':(0, 200000000),
'Radix':(0, 1000000000),
'Barnes':(40000000, 100000000),
'FMM':(0, 1000000000),
'OceanContig':(100000000, 100000000),
'OceanNoncontig':(100000000, 100000000),
'Raytrace':(0, 500000000),
'WaterNSquared':(150000000, 100000000),
'WaterSpatial':(0, 200000000)
}

instructions = {
2:{
'Barnes':200000000,
'Cholesky':200000000,
'FFT':150000000,
'LUContig':200000000,
'LUNoncontig':200000000,
'Radix':50000000,
'FMM':80000000,
'OceanContig':180000000,
'OceanNoncontig':170000000,
'Raytrace':180000000,
'WaterNSquared':200000000,
'WaterSpatial':200000000
},

4:{
'Barnes':200000000,
'Cholesky':100000000,
'FFT':125000000,
'LUContig':150000000,
'LUNoncontig':100000000,
'Radix':25000000,
'FMM':80000000,
'OceanContig':60000000,
'OceanNoncontig':50000000,
'Raytrace':200000000, # to completion
'WaterNSquared':150000000,
'WaterSpatial':125000000
},

8:{
'Barnes':200000000,
'Cholesky':200000000,
'FFT':100000000,
'LUContig':100000000,
'LUNoncontig':75000000,
'Radix':15000000,
'FMM':15000000,
'OceanContig':30000000,
'OceanNoncontig':30000000,
'Raytrace':200000000, # to completion
'WaterNSquared':75000000,
'WaterSpatial':75000000
}

}


class Cholesky(LiveProcess):
	cmd=env['ROOTDIR'] + 'kernels/cholesky/CHOLESKY -p' + env['NP'] + ' '\
	     + env['ROOTDIR'] + 'kernels/cholesky/inputs/tk23.O'

class FFT(LiveProcess):
	cmd=env['ROOTDIR'] + 'kernels/fft/FFT -p' + env['NP'] + ' -m18'

class LU_contig(LiveProcess):
	cmd=env['ROOTDIR'] + 'kernels/lu/contiguous_blocks/LU -p' + env['NP']

class LU_noncontig(LiveProcess):
	cmd=env['ROOTDIR'] + 'kernels/lu/non_contiguous_blocks/LU -n 512 -p' + \
	     env['NP']

class Radix(LiveProcess):
	cmd=env['ROOTDIR'] + 'kernels/radix/RADIX -n524288 -p' + env['NP']

class Barnes(LiveProcess):
	cmd=env['ROOTDIR'] + 'apps/barnes/BARNES'
	input=env['ROOTDIR'] + 'apps/barnes/input.p' + env['NP']

class FMM(LiveProcess):
	cmd=env['ROOTDIR'] + 'apps/fmm/FMM'
	input=env['ROOTDIR'] + 'apps/fmm/inputs/input.2048.p' + env['NP']

class Ocean_contig(LiveProcess):
	cmd=env['ROOTDIR'] + 'apps/ocean/contiguous_partitions/OCEAN -p' + \
	     env['NP']

class Ocean_noncontig(LiveProcess):
	cmd=env['ROOTDIR'] + 'apps/ocean/non_contiguous_partitions/OCEAN -p' \
	     + env['NP']

class Raytrace(LiveProcess):
	cmd=env['ROOTDIR'] + 'apps/raytrace/RAYTRACE -p' + env['NP'] + ' ' \
	     '/home/jahre/m5_hack/teapot.env' #+ env['ROOTDIR'] + 'apps/raytrace/inputs/car.env' #teapot.env'

class Water_nsquared(LiveProcess):
        copy(env['ROOTDIR'] + 'apps/water-nsquared/random.in', '.')
	cmd=env['ROOTDIR'] + 'apps/water-nsquared/WATER-NSQUARED'
	input=env['ROOTDIR'] + 'apps/water-nsquared/input.p' + env['NP']

class Water_spatial(LiveProcess):
	copy(env['ROOTDIR'] + 'apps/water-spatial/random.in', '.')
        cmd=env['ROOTDIR'] + 'apps/water-spatial/WATER-SPATIAL'
	input=env['ROOTDIR'] + 'apps/water-spatial/input.p' + env['NP']
	



