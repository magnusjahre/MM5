
import popen2
import re
import os
import sys

rootdir = os.getenv("SIMROOT")
if rootdir == None:
  print "Envirionment variable SIMROOT not set. Quitting..."
  sys.exit(-1)

def buildOutput(bm, success, verified, cover):
  output = (str(testnum)+": "+str(benchmark)).ljust(10)
  
  if verified:
    output = output + "Verified".ljust(15)
  else:
    output = outpu + "Not verified".ljust(15)

  if cover:
    output = output + "Full coverage".ljust(20)
  else:
    output = output + "Not full coverage".ljust(20)

  if success:
    output = output+"Test passed!".ljust(20)
  else:
    output = output+"Test failed!".ljust(20)

  return output


successString = 'Simulation complete'
lostString = 'Sampler exit lost'
verifySuccess = 'Verify finished successfully'
fullCoverage = 'All state machine edges have been covered'

binary = rootdir+'/branch/fairMHA/m5/build/ALPHA_SE/m5.opt'
fwticks = 25000000
simticks = 5000000
bmArg = "-EBENCHMARK=" 
args  = "-ENP=4 -EPROTOCOL=none -EINTERCONNECT=crossbar -EFASTFORWARDTICKS="+str(fwticks)+" -ESIMULATETICKS="+str(simticks)+"  -ESTATSFILE=test.txt -EMEMORY-BUS-SCHEDULER=FCFS"
configFile = "../configs/CMP/run.py"

REPORTFILE = "testreport.txt"
report = open(REPORTFILE, 'w')

benchmarks = range(1,41)

correct_pattern = re.compile(successString)
lost_req_pattern = re.compile(lostString)

ver_pattern = re.compile(verifySuccess)
coverage_pattern = re.compile(fullCoverage)
    
print
print "M5 Test Results:"
print
report.write("\nM5 Test Results:\n")
testnum = 1
correctCount = 0

for benchmark in benchmarks:

  res = popen2.popen4("nice "+binary+" "+bmArg+str(benchmark)+" "+args+" "+configFile)
            
  simOutput = res[0].read()
    
  correct = correct_pattern.search(simOutput)
  lostReq = lost_req_pattern.search(simOutput)

  res = popen2.popen4('python -c "import fairmha.memVerify"')

  verOutput = res[0].read()

  verified = ver_pattern.search(verOutput)
  fullCoverage = coverage_pattern.search(verOutput)

  output = buildOutput(benchmark, correct, verified, fullCoverage)
  print output
  report.write(output+"\n")
  report.flush()
  
            
  if correct and verified and (not lostReq):
    correctCount = correctCount + 1
  else:
    file = open(str(benchmark)+".output", "w");
    file.write("Program output\n\n")
    file.write(simOutput)
    file.close()

    file = open(str(benchmark)+".verify.output", "w");
    file.write("Verification output\n\n")
    file.write(verOutput)
    file.close()
  testnum = testnum + 1

print
output = ""
if correctCount == len(benchmarks):
    output = "All tests completed successfully!"
else:
    output = "One or more tests failed..."
print output
print
report.write("\n"+output+"\n\n")

report.flush()
report.close()
