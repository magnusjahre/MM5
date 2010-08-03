
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
  os.system("cp -r " + fra + " " + til +" 2> /dev/null")

def createWorkload(benchmarkStrings):
    returnArray = []
    
    for string in benchmarkStrings:
        if string == 's6-bzip2-source':
            returnArray.append(Bzip2Source())
        elif string == 's6-bzip2-chicken':
            returnArray.append(Bzip2Chicken())
        elif string == 's6-bzip2-liberty':
            returnArray.append(Bzip2Liberty())
        elif string == 's6-bzip2-text':
            returnArray.append(Bzip2Text())
        elif string == 's6-gcc-scilab':
            returnArray.append(GccScilab())
        elif string == 's6-mcf':
            returnArray.append(Mcf())
        elif string == 's6-gobmk-trevord':
            returnArray.append(GobmkTrevord())
        elif string == 's6-hmmer-nph':
            returnArray.append(HmmerNph())
        elif string == 's6-sjeng':
            returnArray.append(Sjeng())
        elif string == 's6-libquantum':
            returnArray.append(Libquantum())
        elif string == 's6-h264ref-sss':
            returnArray.append(H264refSss())
        elif string == 's6-omnetpp':
            returnArray.append(Omnetpp())
        elif string == 's6-astar-biglakes':
            returnArray.append(AstarBigLakes())
        elif string == 's6-specrand':
            returnArray.append(Specrand())
        elif string == 's6-milc':
            returnArray.append(Milc())
        elif string == 's6-namd':
            returnArray.append(Namd())
        elif string == 's6-dealII':
            returnArray.append(DealII())
        #elif string == 's6-soplex-ref':
        #    returnArray.append(SoplexRef())
        elif string == 's6-povray':
            returnArray.append(Povray())
        elif string == 's6-lbm':
            returnArray.append(Lbm())
        elif string == 's6-sphinx3':
            returnArray.append(Sphinx3())              
        else:
            panic("Unknown benchmark "+str(string)+" is part of workload")
    
    idcnt = 0
    for process in returnArray:
        process.cpuID = idcnt
        idcnt += 1 
    
    return returnArray

class Bzip2Source(LiveProcess):
    copysymtree(os.path.join(spec_root, 'input/401.bzip2/') , '.')
    executable = os.path.join(spec_bin, 'bzip2')
    cmd = 'bzip2 input.source 280'

class Bzip2Chicken(LiveProcess):
    copysymtree(os.path.join(spec_root, 'input/401.bzip2/') , '.')
    executable = os.path.join(spec_bin, 'bzip2')
    cmd = 'bzip2 chicken.jpg 30'
    
class Bzip2Liberty(LiveProcess):
    copysymtree(os.path.join(spec_root, 'input/401.bzip2/') , '.')
    executable = os.path.join(spec_bin, 'bzip2')
    cmd = 'bzip2 liberty.jpg 30'
    
class Bzip2Text(LiveProcess):
    copysymtree(os.path.join(spec_root, 'input/401.bzip2/') , '.')
    executable = os.path.join(spec_bin, 'bzip2')
    cmd = 'bzip2 text.html 280'
    
class GccScilab(LiveProcess):
    copysym(os.path.join(spec_root, '403.gcc/scilab.i') , '.')
    executable = os.path.join(spec_bin, 'gcc')
    cmd = 'gcc scilab.i -o scilab.s'
    
class Mcf(LiveProcess):
    copysym(os.path.join(spec_root, '429.mcf/inp.in') , '.')
    executable = os.path.join(spec_bin, 'mcf')
    cmd = 'mcf inp.in'

class GobmkTrevord(LiveProcess):
    copysym(os.path.join(spec_root, '445.gobmk/trevord.tst') , '.')
    executable = os.path.join(spec_bin, 'gobmk')
    input = "trevord.tst"
    cmd = 'gobmk --quiet --mode gtp'
    
class HmmerNph(LiveProcess):
    copysym(os.path.join(spec_root, '456.hmmer/nph3.hmm') , '.')
    copysym(os.path.join(spec_root, '456.hmmer/swiss41') , '.')
    executable = os.path.join(spec_bin, 'hmmer')
    cmd = 'hmmer nph3.hmm swiss41'    

class Sjeng(LiveProcess):
    copysym(os.path.join(spec_root, '458.sjeng/ref.txt') , '.')
    executable = os.path.join(spec_bin, 'sjeng')
    cmd = 'sjeng ref.txt' 
    
class Libquantum(LiveProcess):
    executable = os.path.join(spec_bin, 'libquantum')
    cmd = 'libquantum 1397 8'

class H264refSss(LiveProcess):
    copysym(os.path.join(spec_root, '464.h264ref/sss_encoder_main.cfg') , '.')
    copysym(os.path.join(spec_root, '464.h264ref/sss.yuv') , '.')
    executable = os.path.join(spec_bin, 'h264ref')
    cmd = 'h264ref -d sss_encoder_main.cfg'

class Omnetpp(LiveProcess):
    copysym(os.path.join(spec_root, '471.omnetpp/omnetpp.ini') , '.')
    executable = os.path.join(spec_bin, 'omnetpp')
    cmd = 'omnetpp omnetpp.ini'

class AstarBigLakes(LiveProcess):
    copysym(os.path.join(spec_root, '473.astar/BigLakes2048.cfg') , '.')
    copysym(os.path.join(spec_root, '473.astar/BigLakes2048.bin') , '.')
    executable = os.path.join(spec_bin, 'astar')
    cmd = 'astar BigLakes2048.cfg'
    
class Specrand(LiveProcess):
    executable = os.path.join(spec_bin, 'specrand')
    cmd = 'specrand 1255432124 234923'
    output = 'specrand.out'

class Milc(LiveProcess):
    copysym(os.path.join(spec_root, '433.milc/su3imp.in') , '.')
    executable = os.path.join(spec_bin, 'milc')
    input = "su3imp.in"
    cmd = 'milc'

class Namd(LiveProcess):
    copysym(os.path.join(spec_root, '444.namd/namd.input') , '.')
    executable = os.path.join(spec_bin, 'namd')
    cmd = 'namd --input namd.input --iterations 38 --output namd.out'

class DealII(LiveProcess):
    executable = os.path.join(spec_bin, 'dealII')
    cmd = 'dealII 23'
    
class SoplexRef(LiveProcess):
    copysym(os.path.join(spec_root, '450.soplex/ref.mps') , '.')
    #copysym(os.path.join(spec_root, '450.soplex/pds-50.mps') , '.')
    executable = os.path.join(spec_bin, 'soplex')
    cmd = 'soplex -m3500 ref.mps'
    #cmd = 'soplex -s1 -e -m45000 pds-50.mps'

class Povray(LiveProcess):
    copysym(os.path.join(spec_root, '453.povray/*') , '.')
    executable = os.path.join(spec_bin, 'povray')
    cmd = 'povray SPEC-benchmark-ref.ini'

class Lbm(LiveProcess):
    copysym(os.path.join(spec_root, '470.lbm/100_100_130_ldc.of') , '.')
    executable = os.path.join(spec_bin, 'lbm')
    cmd = 'lbm 3000 reference.dat 0 0 100_100_130_ldc.of'

class Sphinx3(LiveProcess):
    copysymtree(os.path.join(spec_root, '482.sphinx3/*') , '.')
    executable = os.path.join(spec_bin, 'sphinx_livepretend')
    cmd = 'sphinx_livepretend ctlfile . args.an4'
