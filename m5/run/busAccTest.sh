#!/bin/bash

mkdir bus_exp_data
cd bus_exp_data

wl=$1
simticks=$2
fwticks=$3
bmid=$4

strace=shared_accesses.txt
atrace=alone_accesses.txt

bms=`python -c "import fairmha.getInterference as g;g.getBenchmarks('$wl',True,4)"`
bmarray=($bms)

echo ""
echo "Running bus test..."
echo "Workload:       $wl"
echo "Benchmark:      ${bmarray[$bmid]}, id $bmid"
echo "Simulate ticks: $simticks"
echo "FW ticks:       $fwticks"
echo ""

../../build/ALPHA_SE/m5.opt -ENP=4 -EBENCHMARK=$wl -EINTERCONNECT=crossbar -EPROTOCOL=none -ESTATSFILE=shared_output.txt -ESIMULATETICKS=$simticks -EFASTFORWARDTICKS=$fwticks -EMEMORY-BUS-SCHEDULER=RDFCFS -EMEMORY-BUS-PAGE-POLICY=OpenPage  -EMEMORY-SYSTEM=CrossbarBased -EMEMORY-BUS-CHANNELS=1 -ECACHE-PARTITIONING=StaticUniform -EPROGRESS=1000000 ../../configs/CMP/run.py

cp estimation_access_trace_$bmid.txt $strace

insts=`python -c "import fairmha.getCommittedInsts as c; c.getCommittedInsts('shared_output.txt', $bmid, True)"`

../../build/ALPHA_SE/m5.opt -ENP=1 -EBENCHMARK=${bmarray[$bmid]} -EINTERCONNECT=crossbar -EPROTOCOL=none -ESTATSFILE=alone_output.txt -ESIMINSTS=$insts -EFASTFORWARDTICKS=$fwticks -EMEMORY-BUS-SCHEDULER=RDFCFS -EMEMORY-BUS-PAGE-POLICY=OpenPage -EPROGRESS=1000000 -EMEMORY-ADDRESS-OFFSET=$bmid -EMEMORY-ADDRESS-PARTS=4  -EMEMORY-SYSTEM=CrossbarBased -EMEMORY-BUS-CHANNELS=1 -ECACHE-PARTITIONING=StaticUniform ../../configs/CMP/run.py

cp dram_access_trace.txt $atrace

python -c "import fairmha.getInterference as g; g.compareBusAccessTraces('$strace','$atrace',True)"

cd ..
rm -Rf bus_exp_data

