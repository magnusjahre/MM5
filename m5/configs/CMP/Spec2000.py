from m5 import *
import os
import os.path
from shutil import copy, copytree
import glob

# Originally written by James Srinivasan
# Further modified by Magnus Jahre <jahre @ idi.ntnu.no>

if 'NP' not in env:
    print >>sys.stderr, "Warning: No processor count given on command line, using default with 2 processors\nUse -ENP=4 or similar to change."
    env['NP'] = 2

rootdir = os.getenv("BMROOT")
if rootdir == None:
  print "Envirionment variable BMROOT not set. Quitting..."
  sys.exit(-1)

# Assumes current working directory is where we ought to run the benchmarks from, copysym datasets to etc.

# Root of where SPEC2000 install lives

spec_root = rootdir+'/spec2000/SPEC_2000_REDUCED'

# Location of SPEC binaries
spec_bin  = rootdir+'/spec2000/'

# Create symlink to input files
# NOTE: suppresses error messages to avoid flooding output with messages if the link exists
def copysym(fra, til):
  os.system("ln -s " + fra + " " + til +" 2> /dev/null")
#  os.system("cp " + fra + " " + til +" 2> /dev/null")

def copysymtree(fra, til):
  os.system("ln -s " + fra + " " + til +" 2> /dev/null")
#  os.system("cp -r " + fra + " " + til +" 2> /dev/null")


benchmarknames= ['gzip', 'vpr', 'gcc', 'mcf', 'crafty', 'parser', 'eon', 'perlbmk', 'gap', 'bzip', 'twolf', 'wupwise', 'swim', 'mgrid', 'applu', 'galgel', 'art', 'equake', 'facerec', 'ammp', 'lucas', 'fma3d', 'sixtrack' ,'apsi', 'mesa', 'vortex1']

# String to benchmark mappings

def createWorkload(benchmarkStrings):
    returnArray = []
    
    for string in benchmarkStrings:
        if string == 'gzip':
            returnArray.append(GzipSource())
        elif string == 'vpr':
            returnArray.append(VprPlace())
        elif string == 'gcc':
            returnArray.append(Gcc166())
        elif string == 'mcf':
            returnArray.append(Mcf())
        elif string == 'crafty':
            returnArray.append(Crafty())
        elif string == 'parser':
            returnArray.append(Parser())
        elif string == 'eon':
            returnArray.append(Eon1())
        elif string == 'perlbmk':
            returnArray.append(Perlbmk1())
        elif string == 'gap':
            returnArray.append(Gap())
        elif string == 'vortex1':
            returnArray.append(Vortex1())
        elif string == 'bzip':
            returnArray.append(Bzip2Source())
        elif string == 'twolf':
            returnArray.append(Twolf())
        elif string == 'wupwise':
            returnArray.append(Wupwise())
        elif string == 'swim':
            returnArray.append(Swim())
        elif string == 'mgrid':
            returnArray.append(Mgrid())
        elif string == 'applu':
            returnArray.append(Applu())
        elif string == 'mesa':
            returnArray.append(Mesa())
        elif string == 'galgel':
            returnArray.append(Galgel())
        elif string == 'art':
            returnArray.append(Art1())
        elif string == 'equake':
            returnArray.append(Equake())
        elif string == 'facerec':
            returnArray.append(Facerec())
        elif string == 'ammp':
            returnArray.append(Ammp())
        elif string == 'lucas':
            returnArray.append(Lucas())
        elif string == 'fma3d':
            returnArray.append(Fma3d())
        elif string == 'sixtrack':
            returnArray.append(Sixtrack())
        elif string == 'apsi':
            returnArray.append(Apsi())
        else:
            panic("Unknown benchmark is part of workload")
    
    idcnt = 0
    for process in returnArray:
        process.cpuID = idcnt
        idcnt += 1 
    
    return returnArray

###############################################################################
###############################################################################

class GzipSource(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '164.gzip/input/ref.source') , '.')
	
    executable = os.path.join(spec_bin, 'gzip00.peak.ev6')
    cmd = 'gzip00.peak.ev6 ref.source 60'
    
class GzipLog(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '164.gzip/input/ref.log') , '.')
	
    executable = os.path.join(spec_bin, 'gzip00.peak.ev6')
    cmd = 'gzip00.peak.ev6 ref.log 60'

class GzipGraphic(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '164.gzip/input/ref.graphic') , '.')
	
    executable = os.path.join(spec_bin, 'gzip00.peak.ev6')
    cmd = 'gzip00.peak.ev6 ref.graphic 60'

class GzipRandom(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '164.gzip/input/ref.random') , '.')
	
    executable = os.path.join(spec_bin, 'gzip00.peak.ev6')
    cmd = 'gzip00.peak.ev6 ref.random 60'

class GzipProgram(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '164.gzip/input/ref.program') , '.')
	
    executable = os.path.join(spec_bin, 'gzip00.peak.ev6')
    cmd = 'gzip00.peak.ev6 ref.program 60'

###############################################################################

class VprPlace(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '175.vpr/input/ref.net') ,  'refVpr.net')
	copysym(os.path.join(spec_root, '175.vpr/input/ref.arch.in') , 'refVpr.arch.in')
	
    executable = os.path.join(spec_bin, 'vpr00.peak.ev6')
    cmd = 'vpr00.peak.ev6 ' + 							\
		  'refVpr.net refVpr.arch.in place.out dum.out ' +				\
		  '-nodisp -place_only -init_t 5 -exit_t 0.005 -alpha_t 0.9412 -inner_num 2'


###############################################################################

class Gcc166(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '176.gcc/input/ref.166.i') , ".")
	
    executable = os.path.join(spec_bin, 'gcc00.peak.ev6')
    cmd = 'gcc00.peak.ev6 ref.166.i -o ref.166.s'

class Gcc200(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '176.gcc/input/ref.200.i') , ".")
	
    #executable = os.path.join(spec_bin, 'cc100.peak.ev6')
    #cmd = 'cc100.peak.ev6 200.i -o 200.s'
    executable = os.path.join(spec_bin, 'gcc00.peak.ev6')
    cmd = 'gcc00.peak.ev6 ref.200.i -o ref.200.s'
    

class GccExpr(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '176.gcc/input/ref.expr.i') , ".")
	
    executable = os.path.join(spec_bin, 'gcc00.peak.ev6')
    cmd = 'gcc00.peak.ev6 ref.expr.i -o ref.expr.s'

class GccIntegrate(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '176.gcc/input/ref.integrate.i') , ".")
	
    executable = os.path.join(spec_bin, 'gcc00.peak.ev6')
    cmd = 'gcc00.peak.ev6 ref.integrate.i -o ref.integrate.s'

class GccScilab(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '176.gcc/input/ref.scilab.i') , ".")
	
    executable = os.path.join(spec_bin, 'gcc00.peak.ev6')
    cmd = 'gcc00.peak.ev6 ref.scilab.i -o ref.scilab.s'

###############################################################################

class Mcf(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '181.mcf/input/ref.in') , "refMcf.in")
	
    executable = os.path.join(spec_bin, 'mcf00.peak.ev6')
    cmd = 'mcf00.peak.ev6 refMcf.in'

###############################################################################

class Crafty(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '186.crafty/input/ref/ref.in') , "./craftyref.in")
	
    executable = os.path.join(spec_bin, 'crafty00.peak.ev6')
    cmd = 'crafty00.peak.ev6 ' 
    input = 'craftyref.in'		# source for stderr

###############################################################################

class Parser(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '197.parser/input/ref.in') ,   "refParser.in")
	copysym(os.path.join(spec_root, '197.parser/input/2.1.dict') , ".")
	
	# for some reason this constructor gets called twice but if the target already exists copysymtree will fail so check first
	if not os.path.exists("words"):
	    copysymtree(os.path.join(spec_root, '197.parser/input/words') , "words")
	
    executable = os.path.join(spec_bin, 'parser00.peak.ev6')
    cmd = 'parser00.peak.ev6 2.1.dict -batch' 
    input = 'refParser.in' 		# source for stderr

###############################################################################

class Eon1(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '252.eon/input/ref/eon.dat') ,             ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/materials') ,           ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/spectra.dat') ,         ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/chair.control.cook') ,  ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/chair.camera') ,        ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/chair.surfaces') ,      ".")
	
    executable = os.path.join(spec_bin, 'eon00.peak.ev6')
    cmd = 'eon00.peak.ev6 chair.control.cook chair.camera chair.surfaces chair.cook.ppm ppm pixels_out.cook' 

class Eon2(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '252.eon/input/ref/eon.dat') ,             ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/materials') ,           ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/spectra.dat') ,         ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/chair.control.rushmeier') ,  ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/chair.camera') ,        ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/chair.surfaces') ,       ".")
	
    executable = os.path.join(spec_bin, 'eon00.peak.ev6')
    cmd = 'eon00.peak.ev6 chair.control.rushmeier chair.camera chair.surfaces chair.rushmeier.ppm ppm pixels_out.rushmeier' 

class Eon3(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '252.eon/input/ref/eon.dat') ,             ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/materials') ,           ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/spectra.dat') ,         ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/chair.control.kajiya') ,  ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/chair.camera') ,        ".")
	copysym(os.path.join(spec_root, '252.eon/input/ref/chair.surfaces') ,       ".")
	
    executable = os.path.join(spec_bin, 'eon00.peak.ev6')
    cmd = 'eon00.peak.ev6 chair.control.kajiya chair.camera chair.surfaces chair.kajiya.ppm ppm pixels_out.kajiya' 
    

###############################################################################


class Perlbmk1(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/lenums') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/diffmail.pl') , ".")

	# for some reason this constructor gets called twice but if the target already exists copysymtree will fail so check first
	if not os.path.exists("lib"):
	    copysymtree(os.path.join(spec_root, '253.perlbmk/input/ref/lib') , "lib")

	
    executable = os.path.join(spec_bin, 'perlbmk00.peak.ev6')
    cmd = 'perlbmk00.peak.ev6 -I./lib diffmail.pl 2 550 15 24 23 100' 

class Perlbmk2(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/lenums') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/cpu2000_mhonarc.rc') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/makerand.pl') , ".")

	# for some reason this constructor gets called twice but if the target already exists copysymtree will fail so check first
	if not os.path.exists("lib"):
	    copysymtree(os.path.join(spec_root, '253.perlbmk/input/ref/lib') , "lib")

	
    executable = os.path.join(spec_bin, 'perlbmk00.peak.ev6')
    cmd = 'perlbmk00.peak.ev6 -I./lib makerand.pl' 

class Perlbmk3(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/lenums') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/cpu2000_mhonarc.rc') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/perfect.pl') , ".")

	# for some reason this constructor gets called twice but if the target already exists copysymtree will fail so check first
	if not os.path.exists("lib"):
	    copysymtree(os.path.join(spec_root, '253.perlbmk/input/ref/lib') , "lib")

	
    executable = os.path.join(spec_bin, 'perlbmk00.peak.ev6')
    cmd = 'perlbmk00.peak.ev6 -I./lib perfect.pl b 3 m 4' 
    
class Perlbmk4(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/lenums') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/cpu2000_mhonarc.rc') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/splitmail.pl') , ".")

	# for some reason this constructor gets called twice but if the target already exists copysymtree will fail so check first
	if not os.path.exists("lib"):
	    copysymtree(os.path.join(spec_root, '253.perlbmk/input/ref/lib') , "lib")

	
    executable = os.path.join(spec_bin, 'perlbmk00.peak.ev6')
    cmd = 'perlbmk00.peak.ev6 -I./lib splitmail.pl 850 5 19 18 1500'     
    
class Perlbmk5(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/lenums') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/cpu2000_mhonarc.rc') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/splitmail.pl') , ".")

	# for some reason this constructor gets called twice but if the target already exists copysymtree will fail so check first
	if not os.path.exists("lib"):
	    copysymtree(os.path.join(spec_root, '253.perlbmk/input/ref/lib') , "lib")

	
    executable = os.path.join(spec_bin, 'perlbmk00.peak.ev6')
    cmd = 'perlbmk00.peak.ev6 -I./lib splitmail.pl 704 12 26 16 836'

class Perlbmk6(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/lenums') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/cpu2000_mhonarc.rc') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/splitmail.pl') , ".")

	# for some reason this constructor gets called twice but if the target already exists copysymtree will fail so check first
	if not os.path.exists("lib"):
	    copysymtree(os.path.join(spec_root, '253.perlbmk/input/ref/lib') , "lib")

	
    executable = os.path.join(spec_bin, 'perlbmk00.peak.ev6')
    cmd = 'perlbmk00.peak.ev6 -I./lib splitmail.pl 535 13 25 24 1091'
    
class Perlbmk7(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/lenums') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/cpu2000_mhonarc.rc') , ".")
	copysym(os.path.join(spec_root, '253.perlbmk/input/ref/splitmail.pl') , ".")

	# for some reason this constructor gets called twice but if the target already exists copysymtree will fail so check first
	if not os.path.exists("lib"):
	    copysymtree(os.path.join(spec_root, '253.perlbmk/input/ref/lib') , "lib")

	
    executable = os.path.join(spec_bin, 'perlbmk00.peak.ev6')
    cmd = 'perlbmk00.peak.ev6 -I./lib splitmail.pl 957 12 23 26 1014'
    
###############################################################################

class Gap(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '254.gap/input/ref/ref.in') , "./gapref.in")
	
	# copysym input file by file
	for file in glob.glob(os.path.join(spec_root, '254.gap/input/ref/*')):
            if os.path.basename(file) != "ref.in":
                copysym(file, ".")
	
    executable = os.path.join(spec_bin, 'gap00.peak.ev6')
    cmd = 'gap00.peak.ev6 -l ./ -q -m 192M'
    input = 'gapref.in'
    
###############################################################################

class Vortex1(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
        # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '255.vortex/input/persons.1k') , "./persons.1k")
	copysym(os.path.join(spec_root, '255.vortex/input/lendian.rnv') , "./lendian.rnv")
	copysym(os.path.join(spec_root, '255.vortex/input/lendian.wnv') , "./lendian.wnv")
	copysym(os.path.join(spec_root, '255.vortex/input/lendian1.raw') , "./lendian1.raw")
		
    executable = os.path.join(spec_bin, 'vortex00.peak.ev6')
    cmd = 'vortex00.peak.ev6 lendian1.raw'

class Vortex2(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '255.vortex/input/persons.1k') , "./persons.1k")
	copysym(os.path.join(spec_root, '255.vortex/input/lendian.rnv') , "./lendian.rnv")
	copysym(os.path.join(spec_root, '255.vortex/input/lendian.wnv') , "./lendian.wnv")
	copysym(os.path.join(spec_root, '255.vortex/input/lendian2.raw') , "./lendian2.raw")

		
    executable = os.path.join(spec_bin, 'vortex00.peak.ev6')
    cmd = 'vortex00.peak.ev6 lendian2.raw'

class Vortex3(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '255.vortex/input/persons.1k') , "./persons.1k")
	copysym(os.path.join(spec_root, '255.vortex/input/lendian.rnv') , "./lendian.rnv")
	copysym(os.path.join(spec_root, '255.vortex/input/lendian.wnv') , "./lendian.wnv")
	copysym(os.path.join(spec_root, '255.vortex/input/lendian3.raw') , "./lendian3.raw")
	
		
    executable = os.path.join(spec_bin, 'vortex00.peak.ev6')
    cmd = 'vortex00.peak.ev6 lendian3.raw'

###############################################################################

class Bzip2Source(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '256.bzip2/input/ref.source') , ".")
		
    executable = os.path.join(spec_bin, 'bzip200.peak.ev6')
    cmd = 'bzip200.peak.ev6 ref.source 58'

class Bzip2Graphic(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '256.bzip2/input/ref.graphic') , ".")
		
    executable = os.path.join(spec_bin, 'bzip200.peak.ev6')
    cmd = 'bzip200.peak.ev6 ref.graphic 58'
    
class Bzip2Program(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '256.bzip2/input/ref.program') , ".")
		
    executable = os.path.join(spec_bin, 'bzip200.peak.ev6')
    cmd = 'bzip200.peak.ev6 ref.program 58' 

###############################################################################

class Twolf(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):

    	LiveProcess.__init__(self)	# call parent constructor
	
	# copysym input file by file
	for file in glob.glob(os.path.join(spec_root, '300.twolf/input/ref/*')):
	 	copysym(file, ".")
		
    executable = os.path.join(spec_bin, 'twolf00.peak.ev6')
    cmd = 'twolf00.peak.ev6 ref' 


###############################################################################

class Wupwise(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
    # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '168.wupwise/input/ref/wupwise.in') , ".")
			
    executable = os.path.join(spec_bin, 'wupwise00.peak.ev6')
    cmd = 'wupwise00.peak.ev6' 

###############################################################################

class Swim(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
    # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '171.swim/input/ref/swim.in') , ".")
			
    executable = os.path.join(spec_bin, 'swim00.peak.ev6')
    cmd = 'swim00.peak.ev6' 
    input = 'swim.in'

###############################################################################

class Mgrid(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
    # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '172.mgrid/input/ref/mgrid.in') , ".")
			
    executable = os.path.join(spec_bin, 'mgrid00.peak.ev6')
    cmd = 'mgrid00.peak.ev6' 
    input = 'mgrid.in'

###############################################################################

class Applu(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
    # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '173.applu/input/ref/applu.in') , ".")
			
    executable = os.path.join(spec_bin, 'applu00.peak.ev6')
    cmd = 'applu00.peak.ev6' 
    input = 'applu.in'

###############################################################################

class Mesa(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
    # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '177.mesa/input/ref.in') , "./refMesa.in")
	
        # Can't find this file
        #copysym(os.path.join(spec_root, 'benchspec/CFP2000/177.mesa/data/ref/input/numbers') , ".")
			
    executable = os.path.join(spec_bin, 'mesa00.peak.ev6')
    cmd = 'mesa00.peak.ev6 -frames 1000 -meshfile refMesa.in -ppmfile mesa.ppm' 

###############################################################################

class Galgel(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
    # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '178.galgel/input/ref/ref.in') , "refGalgel.in")
				
    executable = os.path.join(spec_bin, 'galgel00.peak.ev6')
    cmd = 'galgel00.peak.ev6'
    input = 'refGalgel.in'

###############################################################################

class Art1(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
        # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '179.art/input/a10.img')    , ".")
	copysym(os.path.join(spec_root, '179.art/input/c756hel.in') , ".")
	copysym(os.path.join(spec_root, '179.art/input/hc.img')     , ".")
				
    executable = os.path.join(spec_bin, 'art00.peak.ev6')
    cmd = 'art00.peak.ev6 -scanfile c756hel.in -trainfile1 a10.img -trainfile2 hc.img -stride 2 -startx 110 -starty 200 -endx 160 -endy 240 -objects 10'

###############################################################################

class Art2(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
        # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '179.art/input/a10.img')    , ".")
	copysym(os.path.join(spec_root, '179.art/input/c756hel.in') , ".")
	copysym(os.path.join(spec_root, '179.art/input/hc.img')     , ".")

    executable = os.path.join(spec_bin, 'art00.peak.ev6')
    cmd = 'art00.peak.ev6 -scanfile c756hel.in -trainfile1 a10.img -trainfile2 hc.img -stride 2 -startx 470 -starty 140 -endx 520 -endy 180 -objects 10'

###############################################################################

class Equake(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
        # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '183.equake/input/ref/inp.in') , "inpEquake.in")
				
    executable = os.path.join(spec_bin, 'equake00.peak.ev6')
    cmd = 'equake00.peak.ev6'
    input = 'inpEquake.in'

###############################################################################

class Facerec(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
        # copysym input file by file
        for file in glob.glob(os.path.join(spec_root, '187.facerec/input/ref/*')):
            if os.path.basename(file) == "ref.in":
                copysym(file, "./refFacerec.in")
            else:
                copysym(file, ".")
        for file in glob.glob(os.path.join(spec_root, '187.facerec/input/all/input/*')):
            copysym(file, ".")
    
    executable = os.path.join(spec_bin, 'facerec00.peak.ev6')
    cmd = 'facerec00.peak.ev6'
    input = 'refFacerec.in'

###############################################################################

class Ammp(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
    # copysym input file by file
	#for file in glob.glob(os.path.join(spec_root, '188.ammp/input/*')):
	# 	copysym(file, ".")
        if not os.path.exists("input"):
	    copysymtree(os.path.join(spec_root, '188.ammp/input') , "input")
	 				
    executable = os.path.join(spec_bin, 'ammp00.peak.ev6')
    cmd = 'ammp00.peak.ev6'
    input = 'input/ref.in'

###############################################################################

class Lucas(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
        # copysym input file(s) to run directory
	copysym(os.path.join(spec_root, '189.lucas/input/ref/ref.in') , "./lucasref.in")
				
    executable = os.path.join(spec_bin, 'lucas00.peak.ev6')
    cmd = 'lucas00.peak.ev6'
    input = 'lucasref.in'

###############################################################################

class Fma3d(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
        # copysym input file(s) to run directory
        copysym(os.path.join(spec_root, '191.fma3d/input/ref/fma3d.in') , ".")

    executable = os.path.join(spec_bin, 'fma3d00.peak.ev6')
    cmd = 'fma3d00.peak.ev6'

###############################################################################

class Sixtrack(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
        # copysym input file by file
        for file in glob.glob(os.path.join(spec_root, '200.sixtrack/input/all/input/*')):
            copysym(file, ".")
        for file in glob.glob(os.path.join(spec_root, '200.sixtrack/input/ref/*')):
            copysym(file, ".")	 	
				
    executable = os.path.join(spec_bin, 'sixtrack00.peak.ev6')
    cmd = 'sixtrack00.peak.ev6'
    input = 'inp.in'
    
###############################################################################

class Apsi(LiveProcess):

    def __init__(self, _value_parent = None, **kwargs):
        
        LiveProcess.__init__(self)	# call parent constructor  
        
    # copysym input file(s) to run directory	
	copysym(os.path.join(spec_root, '301.apsi/input/ref/apsi.in') , ".")
				
    executable = os.path.join(spec_bin, 'apsi00.peak.ev6')
    cmd = 'apsi00.peak.ev6'
    input = 'apsi.in'
        
###############################################################################
