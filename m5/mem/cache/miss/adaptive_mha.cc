
#include "sim/builder.hh"
#include "adaptive_mha.hh"

#include <fstream>

// #define FATAL_ZERO_NUM 100

using namespace std;

AdaptiveMHA::AdaptiveMHA(const std::string &name,
                         double _lowThreshold,
                         double _highThreshold,
                         int cpu_count,
                         Tick _sampleFrequency,
                         Tick _startTick,
                         bool _onlyTraceBus,
                         int _neededRepeatDecisions,
                         vector<int> & _staticAsymmetricMHA,
                         bool _useFairAMHA)
        : SimObject(name)
{
    adaptiveMHAcpuCount = cpu_count;
    sampleFrequency = _sampleFrequency;
    highThreshold = _highThreshold;
    lowThreshold = _lowThreshold;
    
    onlyTraceBus = _onlyTraceBus;
    
    cpus.resize(cpu_count, 0);
    
    totalInterferenceDelay.resize(cpu_count, 0);
    totalSharedDelay.resize(cpu_count, 0);
    
    neededRepeatDecisions = _neededRepeatDecisions;
    
    currentCandidate = -1;
    numRepeatDecisions = 0;
    
    Tick start = _startTick;
    if(start % sampleFrequency != 0){
        start = start + (sampleFrequency - (start % sampleFrequency));
    }
    assert(start % sampleFrequency == 0);
    
    firstSample = true;
            
    maxMshrs = -1;
    
    assert(_staticAsymmetricMHA.size() == cpu_count);
    bool allStaticM1 = true;
    for(int i=0;i<_staticAsymmetricMHA.size();i++) if(_staticAsymmetricMHA[i] != -1) allStaticM1 = false;
    
    if(!allStaticM1) staticAsymmetricMHAs = _staticAsymmetricMHA;

    useFairAMHA = _useFairAMHA;
    
    dataCaches.resize(adaptiveMHAcpuCount, NULL);
    instructionCaches.resize(adaptiveMHAcpuCount, NULL);
            
    sampleEvent = new AdaptiveMHASampleEvent(this);
    sampleEvent->schedule(start);
    
    if(!onlyTraceBus){
        adaptiveMHATraceFileName = "adaptiveMHATrace.txt";
        ofstream mhafile(adaptiveMHATraceFileName.c_str());
        mhafile << "Tick";
        for(int i=0;i<cpu_count;i++) mhafile << ";DCache " << i << " MSHRs";
        for(int i=0;i<cpu_count;i++) mhafile << ";ICache " << i << " MSHRs";
        mhafile << "\n";
        mhafile.flush();
        mhafile.close();
    }
    
    memTraceFileName = "memoryBusTrace.txt";
    ofstream memfile(memTraceFileName.c_str());
    memfile << "Tick;DataUtil";
    for(int i=0;i<cpu_count;i++) memfile << ";Cache " << i << " Data";
    memfile << ";Avg Queue";
    for(int i=0;i<cpu_count;i++) memfile << ";Cache " << i << " Avg Queue";
    memfile << "\n";
    memfile.flush();
    memfile.close();
    
    ipcTraceFileName = "ipcTrace.txt";
    ofstream ipcfile(ipcTraceFileName.c_str());
    ipcfile << "Tick";
    for(int i=0;i<cpu_count;i++) ipcfile << ";CPU " << i;
    ipcfile << "\n";
    ipcfile.flush();
    ipcfile.close();
    
}
        
AdaptiveMHA::~AdaptiveMHA(){
    delete sampleEvent;
}

void
AdaptiveMHA::regStats(){
    
    dataSampleTooLarge
            .name(name() + ".data_sample_too_large")
            .desc("Number of samples where there data sample was to large")
            ;
    
    addrSampleTooLarge
            .name(name() + ".addr_sample_too_large")
            .desc("Number of samples where there address sample was to large")
            ;
}

void
AdaptiveMHA::registerCache(int cpu_id, bool isDataCache, BaseCache* cache){
    
    if(isDataCache){
        assert(dataCaches[cpu_id] == NULL);
        dataCaches[cpu_id] = cache;
    }
    else{
        assert(instructionCaches[cpu_id] == NULL);
        instructionCaches[cpu_id] = cache;
    }
    
    if(!onlyTraceBus){
        if(maxMshrs == -1) maxMshrs = cache->getCurrentMSHRCount();
        else assert(maxMshrs == cache->getCurrentMSHRCount());
    }
}

void
AdaptiveMHA::handleSampleEvent(Tick time){
    
    
    if(firstSample){
        bus->resetAdaptiveStats();
        for(int i=0;i<cpus.size();i++) cpus[i]->getStalledL1MissCycles();
        firstSample = false;
        
        if(staticAsymmetricMHAs.size() != 0){
            assert(onlyTraceBus);
            for(int i=0;i<staticAsymmetricMHAs.size();i++){
                for(int j=0;j<staticAsymmetricMHAs[i];j++){
                    dataCaches[i]->decrementNumMSHRs();
                }
            }
        }
    }
    
    // sanity check
    assert(cpus.size() == dataCaches.size());
    for(int i=0;i<adaptiveMHAcpuCount;i++){
        assert(cpus[i]->CPUParamsCpuID == dataCaches[i]->cacheCpuID);
    }
    
    // gather information
    double dataBusUtil = bus->getDataBusUtilisation(sampleFrequency);
    vector<int> dataUsers = bus->getDataUsePerCPUId();
    double avgQueueDelay = bus->getAverageQueue(sampleFrequency);
    vector<double> avgQueueDelayPerUser = bus->getAverageQueuePerCPU();
    bus->resetAdaptiveStats();
    assert(dataUsers.size() == adaptiveMHAcpuCount);
    
    int dataSum=0;
    for(int i=0;i<dataUsers.size();i++) dataSum += dataUsers[i];
    if((double) ((double) dataSum / (double) sampleFrequency) > dataBusUtil){
        dataSampleTooLarge++;
    }
    
    vector<double> IPCs(adaptiveMHAcpuCount, 0);
    for(int i=0;i<cpus.size();i++){
        IPCs[i] = cpus[i]->getCommittedInstructionSample(sampleFrequency);
        cpus[i]->resetCommittedInstructionSample();
    }
    
    // analyse data according to selected scheme
    if(!onlyTraceBus){
        if(useFairAMHA) doFairAMHA();
        else doThroughputAMHA(dataBusUtil, dataUsers);
        
        // write Adaptive MHA trace file
        ofstream mhafile(adaptiveMHATraceFileName.c_str(), ofstream::app);
        mhafile << time;
        for(int i=0;i<dataCaches.size();i++) mhafile << ";" << dataCaches[i]->getCurrentMSHRCount();
        for(int i=0;i<instructionCaches.size();i++) mhafile << ";" << instructionCaches[i]->getCurrentMSHRCount();
        mhafile << "\n";
        mhafile.flush();
        mhafile.close();
    }
    
    
    //write bus use trace
    ofstream memTraceFile(memTraceFileName.c_str(), ofstream::app);
    memTraceFile << time;
    memTraceFile << ";" << dataBusUtil;
    for(int i=0;i<dataUsers.size();i++) memTraceFile << ";" << dataUsers[i];
    memTraceFile << ";" << avgQueueDelay;
    for(int i=0;i<avgQueueDelayPerUser.size();i++) memTraceFile << ";" << avgQueueDelayPerUser[i];
    memTraceFile << "\n";
    
    memTraceFile.flush();
    memTraceFile.close();
    
    // write IPC trace
    ipcTraceFileName = "ipcTrace.txt";
    ofstream ipcfile(ipcTraceFileName.c_str(), ofstream::app);
    ipcfile << curTick;
    for(int i=0;i<IPCs.size();i++) ipcfile << ";" << IPCs[i];
    ipcfile << "\n";
    ipcfile.flush();
    ipcfile.close();

        
    assert(!sampleEvent->scheduled());
    sampleEvent->schedule(time + sampleFrequency);
}

void
AdaptiveMHA::doFairAMHA(){
    
    cout << "fair amha running at tick " << curTick << "\n";
    
    // 1. Gather data
    
    // gathering stalled cycles, i.e. stall time shared estimate
    vector<int> stalledCycles(0, adaptiveMHAcpuCount);
    for(int i=0;i<cpus.size();i++) stalledCycles[i] = cpus[i]->getStalledL1MissCycles();
    
    // gather the estimated cycles used for self-interference and transfers/accesses
    
    // 2. Compute maximum difference in stall time
    
    // 3. Reduce/Increase usable MSHRs to reduce the stall time difference
    
    
}

void
AdaptiveMHA::doThroughputAMHA(double dataBusUtil, std::vector<int> dataUsers){
    
    bool decreaseCalled = false;
    if(dataBusUtil >= highThreshold){
        throughputDecreaseNumMSHRs(dataUsers);
        decreaseCalled = true;
    }
    if(dataBusUtil <= lowThreshold){
        throughputIncreaseNumMSHRs();
    }
        
    if(!decreaseCalled){
        numRepeatDecisions = 0;
        currentCandidate = -1;
    }
}

void
AdaptiveMHA::throughputDecreaseNumMSHRs(vector<int> currentVector){
    
    int index = -1;
    int largest = 0;
    for(int i=0;i<currentVector.size();i++){
        if(currentVector[i] > largest && dataCaches[i]->getCurrentMSHRCount() > 1){
            largest = currentVector[i];
            index = i;
        }
    }
    
    if(index == -1){
        // we have reduced all bus users to blocking cache configurations
        // the others do not use the bus, return
        return;
    }
    
    assert(index > -1);

    if(index == currentCandidate){
        
        numRepeatDecisions++;
        if(numRepeatDecisions >= neededRepeatDecisions){
            dataCaches[index]->decrementNumMSHRs();
            numRepeatDecisions = 0;
        }
    }
    else{

        currentCandidate = index;
        numRepeatDecisions = 1;
        
        if(neededRepeatDecisions == 1){
            dataCaches[index]->decrementNumMSHRs();
            numRepeatDecisions = 0;
        }

    }
    return;
}

void
AdaptiveMHA::throughputIncreaseNumMSHRs(){
    
    int smallest = 100;
    int index = -1;
    
    for(int i=0;i<adaptiveMHAcpuCount;i++){
        if(dataCaches[i]->getCurrentMSHRCount() < smallest){
            smallest = dataCaches[i]->getCurrentMSHRCount();
            index = i;
        }
    }
    
    assert(index > -1);
    if(smallest == maxMshrs) return;
    
    // reset the decrease stats
    currentCandidate = -1;
    numRepeatDecisions = 0;
    
    // no filtering here, we should increase quickly
    dataCaches[index]->incrementNumMSHRs();
    
    // Unblock the cache if it is blocked due to too few MSHRs
    if(dataCaches[index]->isBlockedNoMSHRs()){
        assert(dataCaches[index]->isBlocked());
        dataCaches[index]->clearBlocked(Blocked_NoMSHRs);
    }
}

void
AdaptiveMHA::addInterferenceDelay(vector<Tick> perCPUQueueTimes){
    for(int i=0;i<perCPUQueueTimes.size();i++){
        totalInterferenceDelay[i] += perCPUQueueTimes[i];
    }
}

void
AdaptiveMHA::addTotalDelay(int issuedCPU, Tick delay){
    fatal("add total delay not impl");
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(AdaptiveMHA)
    Param<double> lowThreshold;
    Param<double> highThreshold;
    Param<int> cpuCount;
    Param<Tick> sampleFrequency;
    Param<Tick> startTick;
    Param<bool> onlyTraceBus;
    Param<int> neededRepeats;
    VectorParam<int> staticAsymmetricMHA;
    Param<bool> useFairMHA;
END_DECLARE_SIM_OBJECT_PARAMS(AdaptiveMHA)

BEGIN_INIT_SIM_OBJECT_PARAMS(AdaptiveMHA)
    INIT_PARAM(lowThreshold, "Threshold for increasing the number of MSHRs"),
    INIT_PARAM(highThreshold, "Threshold for reducing the number of MSHRs"),
    INIT_PARAM(cpuCount, "Number of cores in the CMP"),
    INIT_PARAM(sampleFrequency, "The number of clock cycles between each sample"),
    INIT_PARAM(startTick, "The tick where the scheme is started"),
    INIT_PARAM(onlyTraceBus, "Only create the bus trace, adaptiveMHA is turned off"),
    INIT_PARAM(neededRepeats, "Number of repeated desicions to change config"),
    INIT_PARAM(staticAsymmetricMHA, "The number of times each caches mshrcount should be reduced"),
    INIT_PARAM(useFairMHA, "True if the fair AMHA implementation should be used")
END_INIT_SIM_OBJECT_PARAMS(AdaptiveMHA)

CREATE_SIM_OBJECT(AdaptiveMHA)
{
    
    return new AdaptiveMHA(getInstanceName(),
                           lowThreshold,
                           highThreshold,
                           cpuCount,
                           sampleFrequency,
                           startTick,
                           onlyTraceBus,
                           neededRepeats,
                           staticAsymmetricMHA,
                           useFairMHA);
}

REGISTER_SIM_OBJECT("AdaptiveMHA", AdaptiveMHA)

#endif //DOXYGEN_SHOULD_SKIP_THIS

