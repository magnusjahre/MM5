#!/bin/bash

mkdir bus_exp_data

../build/ALPHA_SE/m5.opt -ENP=4 -EBENCHMARK=fair40 -EINTERCONNECT=crossbar -EPROTOCOL=none -ESTATSFILE=shared_output.txt -ESIMULATETICKS=2000000 -EFASTFORWARDTICKS=1000000 -EMEMORY-BUS-SCHEDULER=RDFCFS -EPROGRESS=1000000 ../configs/CMP/run.py

cp estimation_access_trace.txt bus_exp_data

../build/ALPHA_SE/m5.opt -ENP=1 -EBENCHMARK=equake0 -EINTERCONNECT=crossbar -EPROTOCOL=none -ESTATSFILE=test_output.txt -ESIMINSTS=134224 -EFASTFORWARDTICKS=1000000 -EMEMORY-BUS-SCHEDULER=RDFCFS -EPROGRESS=1000000 -EMEMORY-ADDRESS-OFFSET=3 -EMEMORY-ADDRESS-PARTS=4  ../configs/CMP/run.py

cp dram_access_trace.txt bus_exp_data

cd bus_exp_data

python -c "import fairmha.getInterference as g; g.compareBusAccessTraces('estimation_access_trace.txt','dram_access_trace.txt',True)"

rm *txt
cd ..
rmdir bus_exp_data

