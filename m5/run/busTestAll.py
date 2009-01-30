
import subprocess
import re


SIM = 1000000
FW = 10000000
NPS = 4
FILENAME = "busTestResults.txt"
w = 20

def getCommand(wl, bmid):
    args = []
    args.append(wl)
    args.append(str(SIM))
    args.append(str(FW))
    args.append(str(bmid))

    cmd = "./busAccTest.sh "
    for a in args:
        cmd += a+" "

    return cmd

of = open(FILENAME,'w')
of.write("Workload".ljust(w))
for i in range(NPS):
    of.write(("CPU "+str(i)).rjust(w))
of.write("\n")

correctPat = re.compile("Correct:.*")
wrongPat = re.compile("Wrong:.*")

ids = range(1,41)
workloads = []

for i in ids:
    if i < 10:
        workloads.append("fair0"+str(i))
    else:
        workloads.append("fair"+str(i))

for wl in workloads:
    percs = []

    for pid in range(NPS):
        print "Testing "+wl+" "+str(pid)+"..."

        cmd = getCommand(wl,pid)
        p = subprocess.Popen([cmd], 
                             shell=True,
                             stdin=subprocess.PIPE, 
                             stdout=subprocess.PIPE, 
                             stderr = subprocess.PIPE,
                             close_fds=True)
        (outpipe, inpipe, errpipe) = (p.stdout, p.stdin, p.stderr)
        
        print "Forked process with cmd "+str(cmd)+", pid "+str(p.pid)
        
        output = outpipe.read()

        correct = correctPat.findall(output)
        wrong = wrongPat.findall(output)

        percs.append( (correct[0].split()[2],wrong[0].split()[2]) )

        print correct
        print wrong


    of.write(wl.ljust(w))
    for cor,wrong in percs:
        res = cor+"/"+wrong
        of.write(res.rjust(w))
    of.write("\n")

of.flush()
of.close()
