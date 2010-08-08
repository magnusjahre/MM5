
from m5 import *

rootdir = os.getenv("BMROOT")
if rootdir == None:
  print "Envirionment variable BMROOT not set. Quitting..."
  sys.exit(-1)

spec_root = rootdir+'/spec2006/input'
spec_bin  = rootdir+'/spec2006/'

def copysym(fra, til):
  os.system("cp " + fra + " " + til) #+" 2> /dev/null")

def copysymtree(fra, til):
  os.system("cp -r " + fra + " " + til)# +" 2> /dev/null")

def createWorkload(benchmarkStrings):
    returnArray = []
    
    for string in benchmarkStrings:
        if string == 's6-bzip2':
            returnArray.append(Bzip2Source())
#        elif string == 's6-bzip2-chicken':
#            returnArray.append(Bzip2Chicken())
#        elif string == 's6-bzip2-liberty':
#            returnArray.append(Bzip2Liberty())
#        elif string == 's6-bzip2-text':
#            returnArray.append(Bzip2Text())
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
            returnArray.append(Wrf())               
        else:
            panic("Unknown benchmark "+str(string)+" is part of workload")
    
    idcnt = 0
    for process in returnArray:
        process.cpuID = idcnt
        idcnt += 1 
    
    return returnArray

class Bzip2Source(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '401.bzip2/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'bzip2')
    cmd = 'bzip2 input.source 280'

class Bzip2Chicken(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '401.bzip2/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'bzip2')
    cmd = 'bzip2 chicken.jpg 30'
    
class Bzip2Liberty(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '401.bzip2/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'bzip2')
    cmd = 'bzip2 liberty.jpg 30'
    
class Bzip2Text(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '401.bzip2/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'bzip2')
    cmd = 'bzip2 text.html 280'
    
class GccScilab(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '403.gcc/scilab.i') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'gcc')
    cmd = 'gcc scilab.i -o scilab.s'
    
class Mcf(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '429.mcf/inp.in') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'mcf')
    cmd = 'mcf inp.in'

class GobmkTrevord(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '445.gobmk/trevord.tst') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'gobmk')
    input = "trevord.tst"
    cmd = 'gobmk --quiet --mode gtp'
    
class HmmerNph(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '456.hmmer/nph3.hmm') , '.')
        copysym(os.path.join(spec_root, '456.hmmer/swiss41') , '.')
        LiveProcess.__init__(self)
        
    executable = os.path.join(spec_bin, 'hmmer')
    cmd = 'hmmer nph3.hmm swiss41'    

class Sjeng(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '458.sjeng/ref.txt') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'sjeng')
    cmd = 'sjeng ref.txt' 
    
class Libquantum(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        executable = os.path.join(spec_bin, 'libquantum')
    
    cmd = 'libquantum 1397 8'

class H264refSss(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '464.h264ref/sss_encoder_main.cfg') , '.')
        copysym(os.path.join(spec_root, '464.h264ref/sss.yuv') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'h264ref')
    cmd = 'h264ref -d sss_encoder_main.cfg'

class Omnetpp(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '471.omnetpp/omnetpp.ini') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'omnetpp')
    cmd = 'omnetpp omnetpp.ini'

class AstarBigLakes(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '473.astar/BigLakes2048.cfg') , '.')
        copysym(os.path.join(spec_root, '473.astar/BigLakes2048.bin') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'astar')
    cmd = 'astar BigLakes2048.cfg'
    
class Specrand(LiveProcess):    
    executable = os.path.join(spec_bin, 'specrand')
    cmd = 'specrand 1255432124 234923'
    output = 'specrand.out'

class Namd(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '444.namd/namd.input') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'namd')
    cmd = 'namd --input namd.input --iterations 38 --output namd.out'

class DealII(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '447.dealII/*') , '.')
        LiveProcess.__init__(self)
        
    executable = os.path.join(spec_bin, 'dealII')
    cmd = 'dealII 23'
    
class SoplexRef(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '450.soplex/ref.mps') , '.')
        #copysym(os.path.join(spec_root, '450.soplex/pds-50.mps') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'soplex')
    cmd = 'soplex -m3500 ref.mps'
    #cmd = 'soplex -s1 -e -m45000 pds-50.mps'

class Povray(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '453.povray/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'povray')
    cmd = 'povray SPEC-benchmark-ref.ini'

class Lbm(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysym(os.path.join(spec_root, '470.lbm/100_100_130_ldc.of') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'lbm')
    cmd = 'lbm 3000 reference.dat 0 0 100_100_130_ldc.of'

class Sphinx3(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '482.sphinx3/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'sphinx_livepretend')
    cmd = 'sphinx_livepretend ctlfile . args.an4'
    
class Bwaves(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '410.bwaves/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'bwaves')
    cmd = 'bwaves'
    
class GamessCytosine(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '416.gamess/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'gamess')
    cmd = 'gamess'
    input = 'cytosine.2.config'

class Milc(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '433.milc/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'milc')
    cmd = 'milc'
    input = 'su3imp.in'

class Zeusmp(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '434.zeusmp/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'zeusmp')
    cmd = 'zeusmp'

class Gromacs(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '435.gromacs/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'gromacs')
    cmd = 'gromacs -silent -deffnm gromacs -nice 0'
    
class CactusADM(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '436.cactusADM/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'cactusADM')
    cmd = 'cactusADM benchADM.par'    

class Leslie3d(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '437.leslie3d/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'leslie3d')
    cmd = 'leslie3d'
    input = 'leslie3d.in'     

class Calculix(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '454.calculix/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'calculix')
    cmd = 'calculix -i  hyperviscoplastic'
    
class GemsFDTD(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '459.GemsFDTD/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'GemsFDTD')
    cmd = 'GemsFDTD'

class Tonto(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '465.tonto/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'tonto')
    cmd = 'tonto'
    
class Wrf(LiveProcess):
    def __init__(self, _value_parent = None, **kwargs):
        copysymtree(os.path.join(spec_root, '481.wrf/*') , '.')
        LiveProcess.__init__(self)
    
    executable = os.path.join(spec_bin, 'wrf')
    cmd = 'wrf'
    