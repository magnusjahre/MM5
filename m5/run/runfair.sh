#!/bin/bash

#bms="facerec0 art0 applu0 equake0"
#wl="fair06"

bms="perlbmk0 ammp0 parser0 mgrid0"
wl="fair01"

#args="-EINFINITE-MEM-BW"
args="-ECACHE-PARTITIONING=StaticUniform"
#args=""

rm stats_*txt
rm -Rf runtmp
mkdir runtmp

echo
echo "Running workload $wl:"
echo

cd runtmp

../../build/ALPHA_SE/m5.opt -ENP=4 -EBENCHMARK=$wl -EINTERCONNECT=crossbar -EPROTOCOL=none -ESTATSFILE=test_output.txt -ESIMULATETICKS=$1 -EMEMORY-BUS-SCHEDULER=RDFCFS -EFASTFORWARDTICKS=$2 $args ../../configs/CMP/run.py > /dev/null 2> /dev/null
cp test_output.txt ../stats_$wl.txt
cp cpuSwitchInsts.txt ../$(echo $wl)_cpuSwitchInsts.txt
python -c "import fairmha.getInterference as g; g.getInterference('test_output.txt', 4, True)"
cat ../$(echo $wl)_cpuSwitchInsts.txt

COUNTER=0
for i in $bms
do
    rm -Rf *

    insts=`python -c "import fairmha.getCommittedInsts as c; c.getCommittedInsts('../stats_$wl.txt', $COUNTER, True)"`
    COUNTER=$[$COUNTER+1]

    echo
    echo "Running benchmark $i until it has committed $insts instructions:"
    echo

    ../../build/ALPHA_SE/m5.opt -ENP=1 -EBENCHMARK=$i -EINTERCONNECT=crossbar -EPROTOCOL=none -ESTATSFILE=test_output.txt -ESIMINSTS=$insts -EMEMORY-BUS-SCHEDULER=RDFCFS -EPROGRESS=0 -EFASTFORWARDTICKS=$2 -EMEMORY-ADDRESS-OFFSET=$COUNTER -EMEMORY-ADDRESS-PARTS=4  $args ../../configs/CMP/run.py > /dev/null 2> /dev/null
    cp test_output.txt ../stats_$i.txt
    cp cpuSwitchInsts.txt ../$(echo $i)_cpuSwitchInsts.txt
    python -c "import fairmha.getInterference as g; g.getInterference('test_output.txt', 1, True)"
    cat ../$(echo $i)_cpuSwitchInsts.txt
done

cd ..

bmPythonArray=`python -c "print '$bms'.split()"`
fileArray=`python -c "print ['stats_'+i+'.txt' for i in $bmPythonArray]"`
echo
echo "Error Summary:"
python -c "import fairmha.getInterference as g; g.printError('stats_$wl.txt', $fileArray, 4)"
echo