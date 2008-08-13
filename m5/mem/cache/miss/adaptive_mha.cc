
#include "sim/builder.hh"
#include "adaptive_mha.hh"

#include <iomanip>

// #define FATAL_ZERO_NUM 100

using namespace std;

#include "fair_mha.cc"

AdaptiveMHA::AdaptiveMHA(const std::string &name,
                         double _lowThreshold,
                         double _highThreshold,
                         int cpu_count,
                         Tick _sampleFrequency,
                         Tick _startTick,
                         bool _onlyTraceBus,
                         int _neededRepeatDecisions,
                         vector<int> & _staticAsymmetricMHA,
                         bool _useFairAMHA,
                         int _resetCounter,
                         double _reductionThreshold,
                         double _interferencePointMinAllowed)
        : SimObject(name)
{
    adaptiveMHAcpuCount = cpu_count;
    sampleFrequency = _sampleFrequency;
    highThreshold = _highThreshold;
    lowThreshold = _lowThreshold;
    
    onlyTraceBus = _onlyTraceBus;
    
    resetCounter = _resetCounter;
    localResetCounter = _resetCounter;
    reductionThreshold = _reductionThreshold;
    interferencePointMinAllowed = _interferencePointMinAllowed;
    
    cpus.resize(cpu_count, 0);
    
    totalInterferenceDelayRead = vector<vector<Tick> >(cpu_count, vector<Tick>(cpu_count, 0));
    totalInterferenceDelayWrite = vector<vector<Tick> >(cpu_count, vector<Tick>(cpu_count, 0));
    totalSharedDelay.resize(cpu_count, 0);
    totalSharedWritebackDelay.resize(cpu_count, 0);
    delayReadRequestsPerCPU.resize(cpu_count, 0);
    delayWriteRequestsPerCPU.resize(cpu_count, 0);
    numInterferenceRequests = 0;
    numDelayRequests = 0;
    interferenceOverflow = 0;
    
    interferenceBlacklist = vector<vector<bool> >(cpu_count, vector<bool>(cpu_count, false));
    lastInterfererID = -1;
    lastVictimID = -1;
    lastInterferenceValue = 0.0;
    
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
    maxWB = -1;
    
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
        for(int i=0;i<cpu_count;i++) mhafile << ";D " << i << " MSHRs";
        for(int i=0;i<cpu_count;i++) mhafile << ";D " << i << " WB";
        for(int i=0;i<cpu_count;i++) mhafile << ";I" << i << " MSHRs";
        for(int i=0;i<cpu_count;i++) mhafile << ";I " << i << " WB";
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
AdaptiveMHA::registerCache(int cpu_id, bool isDataCache, BaseCache* cache, bool isShared){
    
    if(!isShared){
        if(isDataCache){
            assert(dataCaches[cpu_id] == NULL);
            dataCaches[cpu_id] = cache;
        }
        else{
            assert(instructionCaches[cpu_id] == NULL);
            instructionCaches[cpu_id] = cache;
        }
        
        if(!onlyTraceBus){
            if(maxMshrs == -1) maxMshrs = cache->getCurrentMSHRCount(true);
            else assert(maxMshrs == cache->getCurrentMSHRCount(true));
            
            if(isDataCache){
                if(maxWB == -1) maxWB = cache->getCurrentMSHRCount(false);
                else assert(maxWB == cache->getCurrentMSHRCount(false));
            }
        }
    }
    else{
        sharedCaches.push_back(cache);
    }
}

void
AdaptiveMHA::handleSampleEvent(Tick time){
    
    bool wasFirst = false;
    if(firstSample){
        wasFirst = true;
        bus->resetAdaptiveStats();
        for(int i=0;i<cpus.size();i++) cpus[i]->getStalledL1MissCycles();
        for(int i=0;i<totalInterferenceDelayRead.size();i++) for(int j=0;j<totalInterferenceDelayRead[i].size();j++) totalInterferenceDelayRead[i][j] = 0;
        for(int i=0;i<totalInterferenceDelayWrite.size();i++) for(int j=0;j<totalInterferenceDelayWrite[i].size();j++) totalInterferenceDelayWrite[i][j] = 0;
        for(int i=0;i<totalSharedDelay.size();i++) totalSharedDelay[i] = 0;
        for(int i=0;i<totalSharedWritebackDelay.size();i++) totalSharedWritebackDelay[i] = 0;
        for(int i=0;i<delayReadRequestsPerCPU.size();i++) delayReadRequestsPerCPU[i] = 0;
        for(int i=0;i<delayWriteRequestsPerCPU.size();i++) delayWriteRequestsPerCPU[i] = 0;
        numDelayRequests = 0;
        numInterferenceRequests = 0;
        oracleStorage.clear();
        firstSample = false;
        
        if(staticAsymmetricMHAs.size() != 0){
            assert(onlyTraceBus);
            for(int i=0;i<staticAsymmetricMHAs.size();i++){
                for(int j=0;j<staticAsymmetricMHAs[i];j++){
                    dataCaches[i]->decrementNumMSHRs(true);
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
    assert(IPCs.size() == cpus.size());
    assert(cpus.size() == adaptiveMHAcpuCount);
    for(int i=0;i<cpus.size();i++){
        IPCs[i] = cpus[i]->getCommittedInstructionSample(sampleFrequency);
        cpus[i]->resetCommittedInstructionSample();
    }
    
    // analyse data according to selected scheme
    if(!onlyTraceBus){
        if(useFairAMHA && ! wasFirst) doFairAMHA();
        else doThroughputAMHA(dataBusUtil, dataUsers);
        
        if(wasFirst){
            ofstream fairfile("fairAlgTrace.txt");
            fairfile << "";
            fairfile.flush();
            fairfile.close();
        }
        
        // write Adaptive MHA trace file
        ofstream mhafile(adaptiveMHATraceFileName.c_str(), ofstream::app);
        mhafile << time;
        for(int i=0;i<dataCaches.size();i++) mhafile << ";" << dataCaches[i]->getCurrentMSHRCount(true);
        for(int i=0;i<dataCaches.size();i++) mhafile << ";" << dataCaches[i]->getCurrentMSHRCount(false);
        for(int i=0;i<instructionCaches.size();i++) mhafile << ";" << instructionCaches[i]->getCurrentMSHRCount(true);
        for(int i=0;i<instructionCaches.size();i++) mhafile << ";" << instructionCaches[i]->getCurrentMSHRCount(false);
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
        if(currentVector[i] > largest && dataCaches[i]->getCurrentMSHRCount(true) > 1){
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
            dataCaches[index]->decrementNumMSHRs(true);
            numRepeatDecisions = 0;
        }
    }
    else{

        currentCandidate = index;
        numRepeatDecisions = 1;
        
        if(neededRepeatDecisions == 1){
            dataCaches[index]->decrementNumMSHRs(true);
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
        if(dataCaches[i]->getCurrentMSHRCount(true) < smallest){
            smallest = dataCaches[i]->getCurrentMSHRCount(true);
            index = i;
        }
    }
    
    assert(index > -1);
    if(smallest == maxMshrs) return;
    
    // reset the decrease stats
    currentCandidate = -1;
    numRepeatDecisions = 0;
    
    // no filtering here, we should increase quickly
    dataCaches[index]->incrementNumMSHRs(true);
    
    // Unblock the cache if it is blocked due to too few MSHRs
    if(dataCaches[index]->isBlockedNoMSHRs()){
        assert(dataCaches[index]->isBlocked());
        dataCaches[index]->clearBlocked(Blocked_NoMSHRs);
    }
}

// Storage convention: delay[victim][responsible]
void
AdaptiveMHA::addInterferenceDelay(vector<std::vector<Tick> > perCPUQueueTimes,
                                  Addr addr,
                                  MemCmd cmd,
                                  int fromCPU,
                                  InterferenceType type,
                                  vector<vector<bool> > nextIsRead){
    
    assert(cmd == Read || cmd == Writeback);
    
    //Addr cacheBlkAddr = addr & ~((Addr) dataCaches[0]->getBlockSize()-1);
    
    for(int i=0;i<perCPUQueueTimes.size();i++){
        for(int j=0;j<perCPUQueueTimes[i].size();j++){
            if(nextIsRead[i][j]) totalInterferenceDelayRead[i][j] += perCPUQueueTimes[i][j];
            else totalInterferenceDelayWrite[i][j] += perCPUQueueTimes[i][j];
        }
    }

    /*
    map<Addr, delayEntry>::iterator iter = oracleStorage.find(cacheBlkAddr);
    if(iter != oracleStorage.end()){
        oracleStorage[cacheBlkAddr].addDelays(perCPUQueueTimes, type);
        bool prevRead = (cmd == Read ? true : false);
        if(oracleStorage[cacheBlkAddr].isRead != prevRead){
            oracleStorage[cacheBlkAddr].isRead = !prevRead;
        }
    }
    else{
        oracleStorage[cacheBlkAddr] = delayEntry(perCPUQueueTimes, (cmd == Read ? true : false), type);
        numInterferenceRequests++;
    }
    */
}

void
AdaptiveMHA::addTotalDelay(int issuedCPU, Tick delay, Addr addr, bool isRead){
    
    assert(delay > 0);
    
    //Addr cacheBlkAddr = addr & ~((Addr) dataCaches[0]->getBlockSize()-1);
    
    assert(issuedCPU >= 0 && issuedCPU <= totalSharedDelay.size());
    assert(issuedCPU >= 0 && issuedCPU <= totalSharedWritebackDelay.size());
    if(isRead){
        totalSharedDelay[issuedCPU] += delay;
        delayReadRequestsPerCPU[issuedCPU]++;
    }
    else{
        totalSharedWritebackDelay[issuedCPU] += delay;
        delayWriteRequestsPerCPU[issuedCPU]++;
    }
    numDelayRequests++;
    
    /*
    map<Addr, delayEntry>::iterator iter = oracleStorage.find(cacheBlkAddr);
    if(iter != oracleStorage.end()){
        oracleStorage[cacheBlkAddr].totalDelay = delay;
    }
    */
}

AdaptiveMHA::delayEntry::delayEntry(std::vector<std::vector<Tick> > _delays, bool read, InterferenceType type)
{
    switch(type){
        case INTERCONNECT_INTERFERENCE:
            cbDelay = _delays;
            break;
        case L2_INTERFERENCE:
            l2Delay = _delays;
            break;
        case MEMORY_INTERFERENCE:
            memDelay = _delays;
            break;
        default:
            fatal("Unknown interference type");
    }
    totalDelay = 0;
    isRead = read;
}
            
void
AdaptiveMHA::delayEntry::addDelays(std::vector<std::vector<Tick> > newDelays, InterferenceType type){
    switch(type){
        case INTERCONNECT_INTERFERENCE:
            for(int i=0;i<cbDelay.size();i++) for(int j=0;j<cbDelay[i].size();j++) cbDelay[i][j] += newDelays[i][j];
            break;
        case L2_INTERFERENCE:
            for(int i=0;i<l2Delay.size();i++) for(int j=0;j<l2Delay[i].size();j++) l2Delay[i][j] += newDelays[i][j];
            break;
        case MEMORY_INTERFERENCE:
            for(int i=0;i<memDelay.size();i++) for(int j=0;j<memDelay[i].size();j++) memDelay[i][j] += newDelays[i][j];
            break;
        default:
            fatal("Unknown interference type");
    }
    
}

template <class T>
void
AdaptiveMHA::printMatrix(std::vector<std::vector<T> >& matrix, ofstream &file, std::string header){
    file << header << "\n";
    for(int i=0;i<matrix.size();i++){
        file << i << ":";
        for(int j=0;j<matrix[i].size();j++){
            file << setw(10) << matrix[i][j]; 
        }
        file << "\n";
    }
    file << "\n";
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
    Param<int> resetCounter;
    Param<double> reductionThreshold;
    Param<double> minInterferencePointAllowed;
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
    INIT_PARAM(useFairMHA, "True if the fair AMHA implementation should be used"),
    INIT_PARAM_DFLT(resetCounter,"The number of events that should be processed before F-AMHA is reset", -1),
    INIT_PARAM_DFLT(reductionThreshold, "The percentage reduction in interference points needed to accept a reduction", 0.1),
    INIT_PARAM_DFLT(minInterferencePointAllowed, "Lowest relative interference point that will count as interference", 1.0)
            
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
                           useFairMHA,
                           resetCounter,
                           reductionThreshold,
                           minInterferencePointAllowed);
}

REGISTER_SIM_OBJECT("AdaptiveMHA", AdaptiveMHA)

#endif //DOXYGEN_SHOULD_SKIP_THIS

