#!/bin/bash

BINARY=m5.opt
CONFIGFILE=/home/jahre/workspace/m5sim-fairmha/m5/configs/CMP/run.py

echo ""
echo "Usage: SIMTICKS FWTICKS WORKLOAD NP MEMSYS RFLIMIT MEMORYCONTROLLER [ARGS ...]" 
echo ""

wl=$3
np=$4
bms=`python -c "import fairmha.interference.interferencemethods as g;g.getBenchmarks('$wl',True,$np)"`

memsys=$5
rflimit=$6
controller=$7
args=$8

dumpFreq=500

echo
echo "Running statistics test..."
echo
echo "Workload:   $wl"
echo "Benchmarks: $bms"
echo "Memsys:     $memsys"
echo "RF-limit    $rflimit"
echo "Scheduling  $controller"
echo "Extra args: $args"

rm stats_*txt
rm -Rf runtmp
mkdir runtmp

echo
echo "Running workload $wl..."

cd runtmp

$BINARY -ENP=$np -EBENCHMARK=$wl -EINTERCONNECT=crossbar -EPROTOCOL=none -ESTATSFILE=test_output.txt -ESIMULATETICKS=$1 -EMEMORY-BUS-SCHEDULER=$controller -EFASTFORWARDTICKS=$2 -EMEMORY-SYSTEM=$memsys -EMEMORY-BUS-CHANNELS=1 -EMEMORY-BUS-PAGE-POLICY=OpenPage -EWRITEBACK-OWNER-POLICY=shadow-tags -EREADY-FIRST-LIMIT-ALL-CPUS=$rflimit -EDUMP-INTERFERENCE=$dumpFreq $args $CONFIGFILE &> shared-trace.txt
cp test_output.txt ../stats_$wl.txt
cp cpuSwitchInsts.txt ../$(echo $wl)_cpuSwitchInsts.txt
cp CPU*InterferenceTrace.txt ../
cp shared-trace.txt ../
cp shared-errtrace.txt ../


COUNTER=0
for i in $bms
do
    rm -Rf *

    insts=`python -c "import fairmha.resultparse.getCommittedInsts as c; c.getCommittedInsts('../stats_$wl.txt', $COUNTER, True)"`
    
    echo "Running benchmark $i until it has committed $insts instructions..."

    $BINARY -ENP=1 -EBENCHMARK=$i -EINTERCONNECT=crossbar -EPROTOCOL=none -ESTATSFILE=test_output.txt -ESIMINSTS=$insts -EMEMORY-BUS-SCHEDULER=$controller -EPROGRESS=0 -EFASTFORWARDTICKS=$2 -EMEMORY-ADDRESS-OFFSET=$COUNTER -EMEMORY-ADDRESS-PARTS=$np -EMEMORY-SYSTEM=$memsys -EMEMORY-BUS-CHANNELS=1 -EMEMORY-BUS-PAGE-POLICY=OpenPage -EWRITEBACK-OWNER-POLICY=shadow-tags -EDUMP-INTERFERENCE=$dumpFreq $args $CONFIGFILE &> $i-trace.txt
    cp test_output.txt ../stats_$i.txt
    cp cpuSwitchInsts.txt ../$(echo $i)_cpuSwitchInsts.txt
    cp CPU0InterferenceTrace.txt ../$(echo $i)_priv_interferencetrace.txt

    COUNTER=$[$COUNTER+1]

done

cd ..

bmPythonArray=`python -c "print '$bms'.split()"`
fileArray=`python -c "print ['stats_'+i+'.txt' for i in $bmPythonArray]"`
echo
python -c "import fairmha.interference.interferencemethods as g; g.printCommitOnceErrors('stats_$wl.txt', $fileArray, '$memsys')"
echo
python -c "import fairmha.interference.interferencemethods as g; g.getInterferenceBreakdownError('stats_$wl.txt', $fileArray, True, '$memsys')"
echo
