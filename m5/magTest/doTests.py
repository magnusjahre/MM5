
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
memSysArg = "-EMEMORY-SYSTEM="
mshrargs = ""
configFile = "../configs/CMP/run.py"

REPORTFILE = "testreport.txt"
report = open(REPORTFILE, 'w')

cpus = [4,8,16]
interconnect = 'crossbar'

memsys= ["CrossbarBased","RingBased"]
channels = [1,2,4]
configs = []
for m in memsys:
  for c in channels:
    configs.append([m,c])

def getCommandline(cpu, benchmark, conf):

  sim = 1 * 10**6
  fw = 10 * 10**6

  args = [binary]
  args.append(cpuArg+str(cpu))
  args.append(bmArg+str(benchmark))
  args.append("-ESIMULATETICKS="+str(sim))
  args.append("-EFASTFORWARDTICKS="+str(fw))
  args.append("-ESTATSFILE=test_output.txt")

  args.append("-EMEMORY-SYSTEM="+str(c[0]))
  args.append("-EMEMORY-BUS-CHANNELS="+str(c[1])) 
 
  args.append(configFile)

  cmd = ""
  for a in args:
    cmd += a+" "
  return cmd

def toString(conf):
  retstr = ""
  for c in conf:
    retstr += str(c)
  return retstr

def fillBms(cpus, length, bms):
  nums = range(1,length+1)

  for i in nums:
    if i < 10:
      bms[cpus].append("fair0"+str(i))
    else:
      bms[cpus].append("fair"+str(i))
  return bms

benchmarks = {}
for c in cpus:
  benchmarks[c] = []

benchmarks = fillBms(4,40,benchmarks)
benchmarks = fillBms(8,20,benchmarks)
benchmarks = fillBms(16,10,benchmarks)

correct_pattern = re.compile(successString)
lost_req_pattern = re.compile(lostString)
    
print
print "M5 Test Results:"
print
report.write("\nM5 Test Results:\n")
testnum = 1
correctCount = 0

for cpu in cpus:
    for c in configs:
        output = "Doing tests with "+str(cpu)+" cpus and "+toString(c)+":"
        print output
        report.write("\n"+output+"\n")
        for benchmark in benchmarks[cpu]:
            res = popen2.popen4("nice "+getCommandline(cpu, benchmark, c))
            
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
                
                file = open(str(benchmark)+str(cpu)+toString(c)+".output", "w");
                file.write("Program output\n\n")
                file.write(out)
                file.close()
            testnum = testnum + 1
        print

print
output = ""
if correctCount == (len(benchmarks)*len(cpus)*len(configs)):
    output = "All tests completed successfully!"
else:
    output = "One or more tests failed..."
print output
print
report.write("\n"+output+"\n\n")

report.flush()
report.close()
