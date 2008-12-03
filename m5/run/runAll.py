
import popen2
import re

fw = 10000000
sim = 1000000
np = 4
nums = range(1,41)
#errorpat = re.compile("CPU[0-9].*%")
errorpat = re.compile("L1.caches.*")


reportfile = open("testreport.txt", "w")
reportfile.write("Testreport for fairness test\n\n")
reportfile.flush()

wls = []
for i in nums:
    if i < 10:
        wls.append("fair0"+str(i))
    else:
        wls.append("fair"+str(i))


print
print "Running fairness test (all errors in %)"
print

print "".ljust(10),
for i in range(np):
    print ("D"+str(i)).rjust(7),
for i in range(np):
    print ("I"+str(i)).rjust(7),
print "Max".rjust(7),
print "Min".rjust(7)

for wl in wls:

    res = popen2.popen3("./runfair.sh "+str(sim)+" "+str(fw)+" "+wl)
    output = res[0].read()
    searchres = errorpat.findall(output)

    errstr = (wl+":").ljust(10)
    foundvals = []
    for r in searchres:
        tmpstr = r.split()[3]
        try:
            errval = int(tmpstr.split()[0])
            foundvals.append(errval)
        except:
            errval = "N/A"
        errstr += str(errval).rjust(8)

    errstr += str(max(foundvals)).rjust(8)
    errstr += str(min(foundvals)).rjust(8)

    print errstr

    reportfile.write(errstr+"\n")
    reportfile.flush()


reportfile.flush()
reportfile.close()

print "All tests finished"
