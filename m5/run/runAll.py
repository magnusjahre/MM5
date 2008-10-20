
import popen2
import re

fw = 10000000
sim = 1000000
nums = range(1,41)
errorpat = re.compile("CPU[0-9].*%")


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
print "Running fairness test"
print

for wl in wls:
    prolog = "Testing workload "+wl+": "
    print prolog,
    res = popen2.popen3("./runfair.sh "+str(sim)+" "+str(fw)+" "+wl)
    output = res[0].read()
    searchres = errorpat.findall(output)

    errstr = "Errors: "
    for r in searchres:
        errstr += r.split()[1]+"% "

    print errstr

    reportfile.write(prolog+errstr+"\n")
    reportfile.flush()


reportfile.flush()
reportfile.close()

print "All tests finished"
