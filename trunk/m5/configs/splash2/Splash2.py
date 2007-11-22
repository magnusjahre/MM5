from m5 import *
import os.path
from os.path import join as joinpath

if 'NP' not in env:
	panic("No number of processors was defined.\ne.g. -ENP=4\n")

env.setdefault('ROOTDIR', 'splash2/codes/')

class Cholesky(LiveProcess):
	cmd=env['ROOTDIR'] + 'kernels/cholesky/CHOLESKY -p' + env['NP'] + ' '\
	     + env['ROOTDIR'] + 'kernels/cholesky/inputs/tk23.O'

class FFT(LiveProcess):
	cmd=env['ROOTDIR'] + 'kernels/fft/FFT -p' + env['NP'] + ' -m18'

class LU_contig(LiveProcess):
	cmd=env['ROOTDIR'] + 'kernels/lu/contiguous_blocks/LU -p' + env['NP']

class LU_noncontig(LiveProcess):
	cmd=env['ROOTDIR'] + 'kernels/lu/non_contiguous_blocks/LU -p' + \
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
	     + env['ROOTDIR'] + 'apps/raytrace/inputs/teapot.env'

class Water_nsquared(LiveProcess):
	cmd=env['ROOTDIR'] + 'apps/water-nsquared/WATER-NSQUARED'
	input=env['ROOTDIR'] + 'apps/water-nsquared/input.p' + env['NP']

class Water_spatial(LiveProcess):
	cmd=env['ROOTDIR'] + 'apps/water-spatial/WATER-SPATIAL'
	input=env['ROOTDIR'] + 'apps/water-spatial/input.p' + env['NP']
	



