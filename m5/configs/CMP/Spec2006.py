
from m5 import *

rootdir = os.getenv("BMROOT")
if rootdir == None:
  print "Envirionment variable BMROOT not set. Quitting..."
  sys.exit(-1)

spec_root = rootdir+'/spec2006/input'
spec_bin  = rootdir+'/spec2006/'

def copyInputFiles(dirname):
    fra = os.path.join(spec_root, dirname+'/*') 
    til = '.'
    os.system("cp -r " + fra + " " + til)# +" 2> /dev/null")

def createWorkload(benchmarkStrings):
    returnArray = []
    
    for string in benchmarkStrings:
        if string == 's6-bzip2':
            returnArray.append(Bzip2Source())
        elif string == 's6-perlbench':
            panic("SPEC2006 perlbench did not compile")
        elif string == 's6-gcc':
            returnArray.append(GccScilab())
        elif string == 's6-mcf':
            returnArray.append(Mcf())
        elif string == 's6-gobmk':
            returnArray.append(GobmkTrevord())
        elif string == 's6-hmmer':
            returnArray.append(HmmerNph())
        elif string == 's6-sjeng':
            returnArray.append(Sjeng())
        elif string == 's6-libquantum':
            returnArray.append(Libquantum())
        elif string == 's6-h264ref':
            returnArray.append(H264refSss())
        elif string == 's6-omnetpp':
            returnArray.append(Omnetpp())
        elif string == 's6-astar':
            returnArray.append(AstarBigLakes())
        elif string == 's6-specrand':
            # benchmark is too short to be useful
            returnArray.append(Specrand())
        elif string == 's6-milc':
            returnArray.append(Milc())
        elif string == 's6-namd':
            returnArray.append(Namd())
        elif string == 's6-dealII':
            returnArray.append(DealII())
        elif string == 's6-soplex':
            returnArray.append(SoplexRef())
        elif string == 's6-povray':
            returnArray.append(Povray())
        elif string == 's6-lbm':
            returnArray.append(Lbm())
        elif string == 's6-sphinx3':
            returnArray.append(Sphinx3())
        elif string == 's6-bwaves':
            returnArray.append(Bwaves())
        elif string == 's6-gamess':
            returnArray.append(GamessCytosine())
        elif string == 's6-zeusmp':
            returnArray.append(Zeusmp())
        elif string == 's6-gromacs':
            returnArray.append(Gromacs())
        elif string == 's6-cactusADM':
            returnArray.append(CactusADM()) 
        elif string == 's6-leslie3d':
            returnArray.append(Leslie3d())                  
        elif string == 's6-calculix':
            returnArray.append(Calculix())
        elif string == 's6-gemsFDTD':
            returnArray.append(GemsFDTD())
        elif string == 's6-tonto':
            returnArray.append(Tonto())
        elif string == 's6-wrf':
            panic("SPEC2006 wrf fails while reading input files, no need to start it")
            returnArray.append(Wrf())
        elif string == 's6-xalancbmk':
            panic('SPEC2006 xalancbmk did not compile')               
        else:
            panic("Unknown benchmark "+str(string)+" is part of workload")
    
    idcnt = 0
    for process in returnArray:
        process.cpuID = idcnt
        idcnt += 1 
    
    return returnArray

class Bzip2Source(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('401.bzip2')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'bzip2')
    cmd = 'bzip2 input.source 280'
    output = 'bzip2-bmout.txt'
    
class GccScilab(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles("403.gcc")
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'gcc')
    cmd = 'gcc scilab.i -o scilab.s'
    output = 'gcc-bmout.txt'
    
class Mcf(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('429.mcf')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'mcf')
    cmd = 'mcf inp.in'
    output = 'mcf-bmout.txt'

class GobmkTrevord(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('445.gobmk')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'gobmk')
    input = "trevord.tst"
    cmd = 'gobmk --quiet --mode gtp'
    output = 'gobmk-bmout.txt'
    
class HmmerNph(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('456.hmmer')
        LiveProcess.__init__(self)
        
    executable = os.path.join(spec_bin, 'hmmer')
    cmd = 'hmmer nph3.hmm swiss41'
    output = 'hmmer-bmout.txt'    

class Sjeng(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('458.sjeng')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'sjeng')
    cmd = 'sjeng ref.txt' 
    output = 'sjeng-bmout.txt'
    
class Libquantum(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'libquantum')
    cmd = 'libquantum 1397 8'
    output = 'libquantum-bmout.txt'

class H264refSss(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('464.h264ref')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'h264ref')
    cmd = 'h264ref -d sss_encoder_main.cfg'
    output = 'h264ref-bmout.txt'

class Omnetpp(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('471.omnetpp')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'omnetpp')
    cmd = 'omnetpp omnetpp.ini'
    output = 'omnetpp-bmout.txt'

class AstarBigLakes(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('473.astar')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'astar')
    cmd = 'astar BigLakes2048.cfg'
    output = 'astar-bmout.txt'
    
class Specrand(LiveProcess):    
    executable = os.path.join(spec_bin, 'specrand')
    cmd = 'specrand 1255432124 234923'
    output = 'specrand-bmout.txt'

class Namd(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('444.namd')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'namd')
    cmd = 'namd --input namd.input --iterations 38 --output namd-bmout.txt'
    output = 'namdbm-bmout.txt'

class DealII(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('447.dealII')
        LiveProcess.__init__(self)
        
    executable = os.path.join(spec_bin, 'dealII')
    cmd = 'dealII 23'
    output = 'dealII-bmout.txt'
    
class SoplexRef(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('450.soplex')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'soplex')
    cmd = 'soplex -m3500 ref.mps'
    output = 'soplex-bmout.txt'

class Povray(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('453.povray')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'povray')
    cmd = 'povray SPEC-benchmark-ref.ini'
    output = 'povray-bmout.txt'

class Lbm(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('470.lbm')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'lbm')
    cmd = 'lbm 3000 reference.dat 0 0 100_100_130_ldc.of'
    output = 'lbm-bmout.txt'

class Sphinx3(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('482.sphinx3')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'sphinx_livepretend')
    cmd = 'sphinx_livepretend ctlfile . args.an4'
    output = 'sphinx-bmout.txt'
    
class Bwaves(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('410.bwaves')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'bwaves')
    cmd = 'bwaves'
    output = 'bwaves-bmout.txt'
    
class GamessCytosine(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('416.gamess')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'gamess')
    cmd = 'gamess'
    input = 'cytosine.2.config'
    output = 'gamess-bmout.txt'

class Milc(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('433.milc')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'milc')
    cmd = 'milc'
    input = 'su3imp.in'
    output = 'milc-bmout.txt'

class Zeusmp(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('434.zeusmp')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'zeusmp')
    cmd = 'zeusmp'
    output = 'zeusmp-bmout.txt'

class Gromacs(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('435.gromacs')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'gromacs')
    cmd = 'gromacs -silent -deffnm gromacs -nice 0'
    output = 'gromacs-bmout.txt'
    
class CactusADM(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('436.cactusADM')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'cactusADM')
    cmd = 'cactusADM benchADM.par'
    output = 'cactusADM-bmout.txt'    

class Leslie3d(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('437.leslie3d')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'leslie3d')
    cmd = 'leslie3d'
    input = 'leslie3d.in'  
    output = 'leslie3d-bmout.txt'   

class Calculix(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('454.calculix')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'calculix')
    cmd = 'calculix -i  hyperviscoplastic'
    output = 'calculix-bmout.txt'
    
class GemsFDTD(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('459.GemsFDTD')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'GemsFDTD')
    cmd = 'GemsFDTD'
    output = 'GemsFDTD-bmout.txt'

class Tonto(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('465.tonto')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'tonto')
    cmd = 'tonto'
    output = 'tonto-bmout.txt'
    
class Wrf(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copyInputFiles('481.wrf')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'wrf')
    cmd = 'wrf'
    output = 'wrf-bmout.txt'
