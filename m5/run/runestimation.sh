#!/bin/bash


wl=$3
bms=`python -c "import fairmha.getInterference as g;g.getBenchmarks('$wl',True,4)"`
checkCPU=$4

echo
echo "Running statistics test..."
echo
echo "Workload:   $wl"
echo "Benchmarks: $bms"
echo "Check cpu: $checkCPU"

rm stats_*txt
rm -Rf runtmp
mkdir runtmp

echo
echo "Running workload $wl:"
echo

cd runtmp

../../build/ALPHA_SE/m5.opt -ENP=4 -EBENCHMARK=$wl -EINTERCONNECT=crossbar -EPROTOCOL=none -ESTATSFILE=test_output.txt -ESIMULATETICKS=$1 -EMEMORY-BUS-SCHEDULER=RDFCFS -EFASTFORWARDTICKS=$2 -ECACHE-PARTITIONING=StaticUniform -EMEMORY-BUS-PAGE-POLICY=OpenPage -EMEMORY-SYSTEM=CrossbarBased -EMEMORY-BUS-CHANNELS=1 ../../configs/CMP/run.py 

cp test_output.txt ../stats_$wl.txt
cp L1dcaches$(echo $checkCPU)InterferenceTrace.txt ../shared_dcache_int.txt
cp L1icaches$(echo $checkCPU)InterferenceTrace.txt ../shared_icache_int.txt
cp L1dcaches$(echo $checkCPU)LatencyTrace.txt ../shared_dcache_lat.txt
cp L1icaches$(echo $checkCPU)LatencyTrace.txt ../shared_icache_lat.txt
cp cpuSwitchInsts.txt ../$(echo $wl)_cpuSwitchInsts.txt

cat ../$(echo $wl)_cpuSwitchInsts.txt


rm -Rf *

insts=`python -c "import fairmha.getCommittedInsts as c; c.getCommittedInsts('../stats_$wl.txt', $checkCPU, True)"`

bmarray=($bms)

echo
echo "Running benchmark ${bmarray[$checkCPU]} until it has committed $insts instructions:"
echo

../../build/ALPHA_SE/m5.opt -ENP=1 -EBENCHMARK=${bmarray[$checkCPU]} -EINTERCONNECT=crossbar -EPROTOCOL=none -ESTATSFILE=test_output.txt -ESIMINSTS=$insts -EMEMORY-BUS-SCHEDULER=RDFCFS -EPROGRESS=0 -EFASTFORWARDTICKS=$2 -EMEMORY-ADDRESS-OFFSET=$checkCPU -EMEMORY-ADDRESS-PARTS=4 -ECACHE-PARTITIONING=StaticUniform  -EMEMORY-BUS-PAGE-POLICY=OpenPage -EMEMORY-SYSTEM=CrossbarBased -EMEMORY-BUS-CHANNELS=1 ../../configs/CMP/run.py 

cp test_output.txt ../stats_${bmarray[$checkCPU]}.txt
cp cpuSwitchInsts.txt ../$(echo $i)_cpuSwitchInsts.txt

cp L1dcaches0LatencyTrace.txt ../alone_dcache_lat.txt
cp L1icaches0LatencyTrace.txt ../alone_icache_lat.txt

cd ..

echo
echo "Instruction cache evaluation"
echo

s=shared_icache_lat.txt
i=shared_icache_int.txt
a=alone_icache_lat.txt

python -c "import fairmha.getInterference as g;g.evaluateRequestEstimates('$s','$i','$a', True)"

echo
echo "Data cache evaluation"
echo


s=shared_dcache_lat.txt
i=shared_dcache_int.txt
a=alone_dcache_lat.txt

python -c "import fairmha.getInterference as g;g.evaluateRequestEstimates('$s','$i','$a', True)"