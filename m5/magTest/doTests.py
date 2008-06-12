
import popen2
import re
import os
import sys

rootdir = os.getenv("SIMROOT")
if rootdir == None:
  print "Envirionment variable SIMROOT not set. Quitting..."
  sys.exit(-1)

successString = 'Simulation complete'
lostString = 'Sampler exit lost'
binary = rootdir+'/branch/fairMHA/m5/build/ALPHA_SE/m5.opt'
bmArg = "-EBENCHMARK="
cpuArg = "-ENP="
interconArg = "-EINTERCONNECT="
args = "-EPROTOCOL=none -ESTATSFILE=test_output.txt -ESIMULATETICKS=5000000 -EFASTFORWARDTICKS=10000000 -EUSE-FAIR-AMHA"
#mshrargs = "-EMSHRSL1D=16 -EMSHRSL1I=16 -EMSHRL1TARGETS=4 -EMSHRSL2=4 -EMSHRL2TARGETS=4 -EUSE-ADAPTIVE-MHA -EADAPTIVE-MHA-LOW-THRESHOLD=0.7 -EADAPTIVE-MHA-HIGH-THRESHOLD=0.9 -EADAPTIVE-REPEATS=1"
#mshrargs = "-EMEMORY-BUS=TimeMultiplexed"
#mshrargs = "-EMEMORY-BUS=NFQ"
mshrargs = ""
configFile = "../configs/CMP/run.py"

REPORTFILE = "testreport.txt"
report = open(REPORTFILE, 'w')

cpus = [4] #[2, 4, 8]
interconnect = 'crossbar'
#buses = ['TNFQ', 'FNFQ', 'RDFCFS', 'FCFS']
buses = ['RDFCFS']

#benchmarks = ['hello', 'gzip', 'vpr', 'gcc', 'mcf', 'crafty', 'parser', 'eon', 'perlbmk', 'gap', 'bzip', 'twolf', 'wupwise', 'swim', 'mgrid', 'applu', 'galgel', 'art', 'equake', 'facerec', 'ammp', 'lucas', 'fma3d', 'sixtrack' ,'apsi', 'mesa', 'vortex1']

#benchmarks = ['Cholesky', 'FFT', 'LUContig', 'LUNoncontig', 'Radix', 'Barnes', 'FMM', 'OceanContig', 'OceanNoncontig', 'Raytrace', 'WaterNSquared', 'WaterSpatial']

benchmarks = range(1,41)

correct_pattern = re.compile(successString)
lost_req_pattern = re.compile(lostString)
    
print
print "M5 Test Results:"
print
report.write("\nM5 Test Results:\n")
testnum = 1
correctCount = 0

for cpu in cpus:
    for bus in buses:
        output = "Doing tests with "+str(cpu)+" cpus and "+bus+" bus:"
        print output
        report.write("\n"+output+"\n")
        for benchmark in benchmarks:
            #print binary+" "+bmArg+str(benchmark)+" "+args+" "+configFile
            res = popen2.popen4("nice "+binary+" "+cpuArg+str(cpu)+" "+bmArg+str(benchmark)+" "+interconArg+interconnect+" "+args+" "+mshrargs+" "+"-EMEMORY-BUS-SCHEDULER="+bus+" "+configFile)
            
            out = ""
            for line in res[0]:
                out += line
            
            correct = correct_pattern.search(out)
            lostReq = lost_req_pattern.search(out)
            
            if correct and (not lostReq):
                output = (str(testnum)+": "+str(benchmark)).ljust(40)+"Test passed!".rjust(20)
                print output
                
                report.write(output+"\n")
                report.flush()
                
                correctCount = correctCount + 1
            else:
                output = (str(testnum)+": "+str(benchmark)).ljust(40)+"Test failed!".rjust(20)
               
                print output
                
                report.write(output+"\n")
                report.flush()
                
                file = open(str(benchmark)+str(cpu)+bus+".output", "w");
                file.write("Program output\n\n")
                file.write(out)
                file.close()
            testnum = testnum + 1
        print

print
output = ""
if correctCount == (len(benchmarks)*len(cpus)*len(buses)):
    output = "All tests completed successfully!"
else:
    output = "One or more tests failed..."
print output
print
report.write("\n"+output+"\n\n")

report.flush()
report.close()
