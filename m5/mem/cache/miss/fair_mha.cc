
void
AdaptiveMHA::doFairAMHA(){

    Tick lowestAccStallTime = 1000;
    
    ofstream fairfile("fairAlgTrace.txt", ofstream::app);
    fairfile << "\nFAIR ADAPTIVE MHA RUNNING AT TICK " << curTick << "\n\n";
    
    // 1. Gather data
    
    // gathering stalled cycles, i.e. stall time shared estimate
    vector<int> stalledCycles(adaptiveMHAcpuCount, 0);
    for(int i=0;i<cpus.size();i++){
        stalledCycles[i] = cpus[i]->getStalledL1MissCycles();
        fairfile << "CPU" << i << " stall cycles: " << stalledCycles[i] << "\n";
    }
    fairfile << "\n";
    
    // gather t_interference_read
    vector<vector<Tick> > t_interference_read(adaptiveMHAcpuCount, vector<Tick>(adaptiveMHAcpuCount, 0));
    for(int i=0;i<t_interference_read.size();i++){
        for(int j=0;j<t_interference_read[i].size();j++){
            t_interference_read[i][j] = totalInterferenceDelayRead[i][j];
            totalInterferenceDelayRead[i][j] = 0;
        }
    }
    numInterferenceRequests = 0;
    
    printMatrix<Tick>(t_interference_read, fairfile, "Read interference matrix:");
    
    vector<vector<Tick> > t_interference_write(adaptiveMHAcpuCount, vector<Tick>(adaptiveMHAcpuCount, 0));
    for(int i=0;i<t_interference_write.size();i++){
        for(int j=0;j<t_interference_write[i].size();j++){
            t_interference_write[i][j] = totalInterferenceDelayWrite[i][j];
            totalInterferenceDelayWrite[i][j] = 0;
        }
    }
    
    printMatrix<Tick>(t_interference_write, fairfile, "Write interference matrix:");
    
    // gather t_shared
    vector<Tick> t_shared(adaptiveMHAcpuCount, 0);
    vector<Tick> t_writeback(adaptiveMHAcpuCount, 0);
    for(int i=0;i<t_shared.size();i++){
        t_shared[i] = totalSharedDelay[i];
        t_writeback[i] = totalSharedWritebackDelay[i];
        totalSharedDelay[i] = 0;
        totalSharedWritebackDelay[i] = 0;
    }
    numDelayRequests = 0;
    
    // gather current cache capacity measurements
    vector<int> sharedCacheCapacityOverusePoints(adaptiveMHAcpuCount, 0);
    vector<int> tmpOveruse(adaptiveMHAcpuCount, 0);
    for(int i=0;i<sharedCaches.size();i++){
        vector<int> bankUse = sharedCaches[i]->perCoreOccupancy();
        int totalBlocks = bankUse[adaptiveMHAcpuCount+1];
        int quota = totalBlocks / adaptiveMHAcpuCount;
        
        for(int i=0;i<adaptiveMHAcpuCount;i++){
            if(bankUse[i] > quota) tmpOveruse[i] += bankUse[i] - quota;
        }
    }
    
    fairfile << "Shared cache capacity interference points:\n";
    for(int i=0;i<tmpOveruse.size();i++){
        sharedCacheCapacityOverusePoints[i] = tmpOveruse[i]; // NOTE: might need adjustment by factor or function
        fairfile << i << ": " << sharedCacheCapacityOverusePoints[i] << ", actual block overuse " << tmpOveruse[i] << "\n";
    }
    fairfile << "\n";
    
    // gather number of read requests
    vector<Tick> numReads(adaptiveMHAcpuCount, 0);
    vector<Tick> numWrites(adaptiveMHAcpuCount, 0);
    fairfile << "Total number of reads and writes per CPU:\n";
    for(int i=0;i<numReads.size();i++){
        numReads[i] = delayReadRequestsPerCPU[i];
        numWrites[i] = delayWriteRequestsPerCPU[i];
        delayReadRequestsPerCPU[i] = 0;
        delayWriteRequestsPerCPU[i] = 0;
        fairfile << "CPU " << i << ": " << numReads[i] << " reads, " << numWrites[i] << " writes\n";
    }
    fairfile << "\n";

    // 2. Compute relative interference points to quantify unfairness
    vector<vector<double> > relativeInterferencePoints(adaptiveMHAcpuCount, vector<double>(adaptiveMHAcpuCount, 0.0));
    vector<double> overallRelativeInterference(adaptiveMHAcpuCount, 0.0);
    fairfile << "Interference points per CPU:\n";
    for(int i=0;i<adaptiveMHAcpuCount;i++){
        for(int j=0;j<adaptiveMHAcpuCount;j++){
            if(numReads[i] == 0){
                relativeInterferencePoints[i][j] = 0;
            }
            else{
                relativeInterferencePoints[i][j] = (double) ((double) t_interference_read[i][j] / (double) numReads[i]);
            }
            overallRelativeInterference[i] += (double) t_interference_read[i][j];
        }
        overallRelativeInterference[i] = overallRelativeInterference[i] / (double)  numReads[i];
        fairfile << "CPU" << i << ": " << overallRelativeInterference[i] << "\n";
    }
    fairfile << "\n";
    
    printMatrix<double>(relativeInterferencePoints, fairfile, "Relative Interference Point Matrix:");
    
    // 3. Measure fairness by computing interference point ratios
    // Idea: if the threads are impacted by fairness to the same extent, the memory system will appear to be fair
    vector<vector<double> > searchPoints(adaptiveMHAcpuCount, vector<double>(adaptiveMHAcpuCount, 0.0));
    double maxDifference = 1.0;
    for(int i=0;i<adaptiveMHAcpuCount;i++){
        for(int j=0;j<adaptiveMHAcpuCount;j++){
            if(overallRelativeInterference[i] != 0 && overallRelativeInterference[j] != 0){
                double diff = overallRelativeInterference[i] / overallRelativeInterference[j];
                if(stalledCycles[i] >= lowestAccStallTime){
                    searchPoints[i][j] = diff;
                    if(diff > maxDifference){
                        maxDifference = diff;
                    }
                }
                else{
                    searchPoints[i][j] = -1;
                }
            }
        }
    }
    
    printMatrix(searchPoints, fairfile, "Relative Interference Searchpoints:");
    
    fairfile << "Current fairness measure (Maxdiff) is " << maxDifference << "\n";
    
    // 4 Modify MSHRs of writeback queue based on measurements
    dampingAndCacheAnalysis(fairfile,
                            t_interference_read,
                            sharedCacheCapacityOverusePoints,
                            numReads,
                            stalledCycles,
                            maxDifference);
    
//     IPDirectWithRollback(fairfile,
//                          t_interference_read,
//                          sharedCacheCapacityOverusePoints,
//                          numReads,
//                          stalledCycles,
//                          maxDifference);
    
//     maxDiffRedWithRollback(fairfile,
//                            relativeInterferencePoints,
//                            numReads,
//                            stalledCycles,
//                            maxDifference);
    
//     fairAMHAFirstAlg(fairfile,
//                      relativeInterferencePoints,
//                      numReads,
//                      numWrites,
//                      stalledCycles,
//                      maxDifference,
//                      lowestAccStallTime);
    
    fairfile.flush();
    fairfile.close();
}

void
AdaptiveMHA::maxDiffRedWithRollback(std::ofstream& fairfile,
                                    std::vector<std::vector<double> >& relativeInterferencePoints,
                                    std::vector<Tick>& numReads,
                                    std::vector<int>& stalledCycles,
                                    double maxDifference){
    
    assert(resetCounter > 0);
    localResetCounter--;
    
    printMatrix<bool>(interferenceBlacklist, fairfile, "Current blacklist:");
    
    if(localResetCounter <= 0){
        
        fairfile << "Resetting blacklist and increasing all caches to max MSHRs\n";
        
        // Reset blacklist
        for(int i=0;i<adaptiveMHAcpuCount;i++){
            for(int j=0;j<adaptiveMHAcpuCount;j++){
                interferenceBlacklist[i][j] = false;
            }
        }
        
        // Increase all caches to max MSHRs
        for(int i=0;i<dataCaches.size();i++){
            while(dataCaches[i]->getCurrentMSHRCount(true) < maxMshrs){
                dataCaches[i]->incrementNumMSHRs(true);
                
                if(dataCaches[i]->isBlockedNoMSHRs()){
                    assert(dataCaches[i]->isBlocked());
                    dataCaches[i]->clearBlocked(Blocked_NoMSHRs);
                }
            }
        }
        
        localResetCounter = resetCounter;
        
        // Need to collect new measurements before making a decision
        lastVictimID = -1;
        lastInterfererID = -1;
        lastInterferenceValue = 0.0;
        return;
    }
    
    // check last move
    if(lastVictimID >= 0 && lastInterfererID >= 0){
        
        fairfile << "Checking last decision, current interference is " 
                << relativeInterferencePoints[lastVictimID][lastInterfererID] 
                << " last was " << lastInterferenceValue 
                << ", threshold value was " 
                <<  lastInterferenceValue -(reductionThreshold * lastInterferenceValue) << "\n";
        
        if(relativeInterferencePoints[lastVictimID][lastInterfererID]
           > lastInterferenceValue -(reductionThreshold * lastInterferenceValue)){
            
            // blacklist change
            interferenceBlacklist[lastVictimID][lastInterfererID] = true;
            fairfile << "MSHR reduction did not impact stall time sufficiently, blacklisting victim "
                     << lastVictimID << " and interferer " << lastInterfererID << "\n";
            
            // roll back reduction decision, increase interferers MSHR count
            assert(dataCaches[lastInterfererID]->getCurrentMSHRCount(true) < maxMshrs);
            fairfile << "Increasing the number of MSHRs for cpu " << lastInterfererID << "\n";
            dataCaches[lastInterfererID]->incrementNumMSHRs(true);
                
            if(dataCaches[lastInterfererID]->isBlockedNoMSHRs()){
                assert(dataCaches[lastInterfererID]->isBlocked());
                dataCaches[lastInterfererID]->clearBlocked(Blocked_NoMSHRs);
            }
            
            // reset storage
            lastVictimID = -1;
            lastInterfererID = -1;
            lastInterferenceValue = 0.0;
        }
    }
    
    // last change accepted or no change, search for another reduction that can improve fairness
    double maxScore = 0.0;
    int victimID = -1;
    int responsibleID = -1;
    
    for(int i=0;i<adaptiveMHAcpuCount;i++){
        for(int j=0;j<adaptiveMHAcpuCount;j++){
            if(!interferenceBlacklist[i][j]){
                if(relativeInterferencePoints[i][j] > maxScore){
                    maxScore = relativeInterferencePoints[i][j];
                    victimID = i;
                    responsibleID = j;
                }
            }
        }
    }
    
    if(victimID < 0 || responsibleID < 0 || maxScore < interferencePointMinAllowed){
        fairfile << "No reduction opportunity found, reseting storage and quitting...\n";
        
        // reset storage
        lastVictimID = -1;
        lastInterfererID = -1;
        lastInterferenceValue = 0.0;
        
        return;
    }
    
    if(dataCaches[responsibleID]->getCurrentMSHRCount(true) > 1){
        fairfile << "CPU " << responsibleID << " interferes with CPU " << victimID << ", reducing MSHRs\n";
        dataCaches[responsibleID]->decrementNumMSHRs(true);
    }
    else{
        fairfile << "CPU " << responsibleID << " interferes with CPU "
                 << victimID << ", allready at blocking cache configuration, no action taken.\n";
    }
    
    // update storage
    lastVictimID = victimID;
    lastInterfererID = responsibleID;
    lastInterferenceValue = relativeInterferencePoints[lastVictimID][lastInterfererID];
}

void
AdaptiveMHA::IPDirectWithRollback(std::ofstream& fairfile,
                                  std::vector<std::vector<Tick> >& readInterference,
                                  std::vector<int>& sharedCacheCapacityIPs,
                                  std::vector<Tick>& numReads,
                                  std::vector<int>& stalledCycles,
                                  double maxDifference){
    
                                      
    assert(resetCounter > 0);
    localResetCounter--;
    
    printMatrix<bool>(interferenceBlacklist, fairfile, "Current blacklist:");
    
    if(localResetCounter <= 0){
        
        fairfile << "Resetting blacklist and increasing all caches to max MSHRs\n";
        
        // Reset blacklist
        for(int i=0;i<adaptiveMHAcpuCount;i++){
            for(int j=0;j<adaptiveMHAcpuCount;j++){
                interferenceBlacklist[i][j] = false;
            }
        }
        
        // Increase all caches to max MSHRs
        for(int i=0;i<dataCaches.size();i++){
            while(dataCaches[i]->getCurrentMSHRCount(true) < maxMshrs){
                dataCaches[i]->incrementNumMSHRs(true);
                
                if(dataCaches[i]->isBlockedNoMSHRs()){
                    assert(dataCaches[i]->isBlocked());
                    dataCaches[i]->clearBlocked(Blocked_NoMSHRs);
                }
            }
        }
        
        localResetCounter = resetCounter;
        
        // Need to collect new measurements before making a decision
        lastVictimID = -1;
        lastInterfererID = -1;
        lastInterferenceValueTick = 0;
        return;
    }
    
    // check last move
    if(lastVictimID >= 0 && lastInterfererID >= 0){
        
        fairfile << "Checking last decision, current interference is " 
                << readInterference[lastVictimID][lastInterfererID] 
                << " last was " << lastInterferenceValueTick 
                << ", threshold value was " 
                <<  lastInterferenceValueTick -(reductionThreshold * lastInterferenceValueTick) << "\n";
        
        if(readInterference[lastVictimID][lastInterfererID]
           > lastInterferenceValueTick -(reductionThreshold * lastInterferenceValueTick)){
            
            // blacklist change
            interferenceBlacklist[lastVictimID][lastInterfererID] = true;
            fairfile << "MSHR reduction did not impact stall time sufficiently, blacklisting victim "
                     << lastVictimID << " and interferer " << lastInterfererID << "\n";
            
            // roll back reduction decision, increase interferers MSHR count
            assert(dataCaches[lastInterfererID]->getCurrentMSHRCount(true) < maxMshrs);
            fairfile << "Increasing the number of MSHRs for cpu " << lastInterfererID << "\n";
            dataCaches[lastInterfererID]->incrementNumMSHRs(true);
                
            if(dataCaches[lastInterfererID]->isBlockedNoMSHRs()){
                assert(dataCaches[lastInterfererID]->isBlocked());
                dataCaches[lastInterfererID]->clearBlocked(Blocked_NoMSHRs);
            }
            
            // reset storage
            lastVictimID = -1;
            lastInterfererID = -1;
            lastInterferenceValueTick = 0;
        }
    }
    
    // last change accepted or no change, search for another reduction that can improve fairness
    Tick maxScore = 0;
    int victimID = -1;
    int responsibleID = -1;
    
    for(int i=0;i<adaptiveMHAcpuCount;i++){
        for(int j=0;j<adaptiveMHAcpuCount;j++){
            if(!interferenceBlacklist[i][j]){
                if(readInterference[i][j] > maxScore){ //FIXME: Bug here, unclear if it influences results!!!
                    maxScore = readInterference[i][j] + sharedCacheCapacityIPs[j];
                    victimID = i;
                    responsibleID = j;
                }
            }
        }
    }
    
    if(victimID < 0 || responsibleID < 0){ //NOTE: no max score check for now || maxScore < interferencePointMinAllowed){
        fairfile << "No reduction opportunity found, reseting storage and quitting...\n";
        
        // reset storage
        lastVictimID = -1;
        lastInterfererID = -1;
        lastInterferenceValueTick = 0;
        
        return;
    }
    
    if(dataCaches[responsibleID]->getCurrentMSHRCount(true) > 1){
        fairfile << "CPU " << responsibleID << " interferes with CPU " << victimID << ", reducing MSHRs\n";
        dataCaches[responsibleID]->decrementNumMSHRs(true);
    }
    else{
        fairfile << "CPU " << responsibleID << " interferes with CPU "
                 << victimID << ", allready at blocking cache configuration, no action taken.\n";
    }
    
    // update storage
    lastVictimID = victimID;
    lastInterfererID = responsibleID;
    lastInterferenceValueTick = readInterference[lastVictimID][lastInterfererID];
}

void
AdaptiveMHA::dampingAndCacheAnalysis(std::ofstream& fairfile,
                                     std::vector<std::vector<Tick> >& readInterference,
                                     std::vector<int>& sharedCacheCapacityIPs,
                                     std::vector<Tick>& numReads,
                                     std::vector<int>& stalledCycles,
                                     double maxDifference){
    
//     assert(interferencePointMinAllowed == 0.0);
    assert(reductionThreshold == 0.0);
    assert(resetCounter > 0);
    localResetCounter--;
    
    if(localResetCounter <= 0){
        
        fairfile << "Increasing all caches to max MSHRs\n";
        
        // Increase all caches to max MSHRs
        for(int i=0;i<dataCaches.size();i++){
            while(dataCaches[i]->getCurrentMSHRCount(true) < maxMshrs){
                dataCaches[i]->incrementNumMSHRs(true);
                
                if(dataCaches[i]->isBlockedNoMSHRs()){
                    assert(dataCaches[i]->isBlocked());
                    dataCaches[i]->clearBlocked(Blocked_NoMSHRs);
                }
            }
        }
        
        localResetCounter = resetCounter;
        lastVictimID = -1;
        lastInterfererID = -1;
        numRepeatDecisions = 0;
        return;
    }
    
    Tick maxScore = (int) interferencePointMinAllowed;
    int victimID = -1;
    int responsibleID = -1;
    
    vector<vector<Tick> > resultingIPs(adaptiveMHAcpuCount, vector<Tick>(adaptiveMHAcpuCount, 0));
    for(int i=0;i<adaptiveMHAcpuCount;i++){
        for(int j=0;j<adaptiveMHAcpuCount;j++){
            //NOTE: a new computation of cache IPs which is graded with overuse might be appropriate
            if(i != j) resultingIPs[i][j] = readInterference[i][j] + sharedCacheCapacityIPs[j];
        }
    }
    printMatrix(resultingIPs, fairfile, "IPs used for analysis");
    
    for(int i=0;i<adaptiveMHAcpuCount;i++){
        for(int j=0;j<adaptiveMHAcpuCount;j++){
            // skip caches that are allready at the blocking configuration
            if(dataCaches[j]->getCurrentMSHRCount(true) > 1){
                if(resultingIPs[i][j] > maxScore){
                    maxScore = resultingIPs[i][j];
                    victimID = i;
                    responsibleID = j;
                }
            }
        }
    }
    
    int blockingCount = 0;
    for(int i=0;i<adaptiveMHAcpuCount;i++){
        if(dataCaches[i]->getCurrentMSHRCount(true) == 1){
            fairfile << "CPU " << i << " has been reduced to a blocking cache, has been skipped\n"; 
            blockingCount++;
        }
    }
    
    if(blockingCount == adaptiveMHAcpuCount){
        fairfile << "All caches have been reduced to blocking caches, quitting...\n"; 
        lastVictimID = -1;
        lastInterfererID = -1;
        numRepeatDecisions = 0;
        return;
    }
    
    if(victimID == -1 && responsibleID == -1){
        fairfile << "All remaining caches have to few iterference points, quitting...\n"; 
        lastVictimID = -1;
        lastInterfererID = -1;
        numRepeatDecisions = 0;
        return;
    }
    
    
    if(victimID == lastVictimID && responsibleID == lastInterfererID){
        numRepeatDecisions++;
        fairfile << "Responsible is still " << responsibleID << " and " << victimID << " is still the victim, repeates increased to " << numRepeatDecisions << ", " << neededRepeatDecisions << " needed\n";
    }
    else{
        lastVictimID = victimID;
        lastInterfererID = responsibleID;
        numRepeatDecisions = 1;
        
        fairfile << "New responsible and victim (r=" << responsibleID << ", v=" << victimID << "). Repeats reset to " << numRepeatDecisions << "\n";
    }
    
    assert(numRepeatDecisions <= neededRepeatDecisions);
    if(numRepeatDecisions == neededRepeatDecisions){
        if(dataCaches[responsibleID]->getCurrentMSHRCount(true) > 1){
            fairfile << "CPU " << responsibleID << " interferes with CPU " << victimID << ", reducing MSHRs\n";
            dataCaches[responsibleID]->decrementNumMSHRs(true);
        }
        else{
            fairfile << "CPU " << responsibleID << " interferes with CPU "
                    << victimID << ", allready at blocking cache configuration, no action taken.\n";
        }
        
        lastVictimID = -1;
        lastInterfererID = -1;
        numRepeatDecisions = 0;
    }
}


void
AdaptiveMHA::fairAMHAFirstAlg(std::ofstream& fairfile,
                              vector<vector<double> >& relativeInterferencePoints,
                              vector<Tick>& numReads,
                              vector<Tick>& numWrites,
                              vector<int>& stalledCycles,
                              double maxDifference,
                              Tick lowestAccStallTime){
    
    double highThres = 1.20;
    double lowThres = 1.10;
    
    if(maxDifference > highThres){
        
        // Reduce miss para of worst processor
        
        double maxScore = 0.0;
        double minScore = 1000000000.0;
        int victimID = -1;
        int responsibleID = -1;
        for(int i=0;i<adaptiveMHAcpuCount;i++){
            for(int j=0;j<adaptiveMHAcpuCount;j++){
                // find max score
                if(stalledCycles[i] >= lowestAccStallTime){
                    if(relativeInterferencePoints[i][j] > maxScore){
                        maxScore = relativeInterferencePoints[i][j];
                        victimID = i;
                        responsibleID = j;
                    }
                }
                if(relativeInterferencePoints[i][j] > 0 && relativeInterferencePoints[i][j] < minScore){
                    minScore = relativeInterferencePoints[i][j];
                }
            }
        }
        assert(victimID >= 0 && responsibleID >= 0);
        
        fairfile << "CPU " << responsibleID 
                << " has " << numReads[responsibleID] 
                << " reads and " << numWrites[responsibleID] << " writes\n";
        
        bool blameReads = numReads[responsibleID] >= numWrites[responsibleID];
        
        fairfile << "The main victim is cpu " << victimID 
                << " which suffers from interference with CPU " << responsibleID 
                << ", value is " << maxScore 
                << ", " << (blameReads ? "blame reads" : "blame writes") 
                << ", minScore " << minScore 
                << ", ratio " << minScore / maxScore << "\n";
        
        if(dataCaches[responsibleID]->getCurrentMSHRCount(blameReads) > 1){
            dataCaches[responsibleID]->decrementNumMSHRs(blameReads);
        }
    }
    else if(maxDifference < lowThres){
        
        vector<int> tmpStall = stalledCycles;
        
        while(!tmpStall.empty()){
            Tick maxval = lowestAccStallTime;
            int maxID = -1;
            for(int i=0;i<tmpStall.size();i++){
                if(maxval < tmpStall[i]){
                    maxval = tmpStall[i];
                    maxID = i;
                }
            }
            if(maxID == -1){
                fairfile << "No CPU with large enough stall time could be found, quitting\n";
                break;
            }
            
            fairfile << "CPU " << maxID << " has a large stall time, can we increase its resources?\n";
            if(dataCaches[maxID]->getCurrentMSHRCount(true) < maxMshrs){
                fairfile << "Increasing the number of MSHRs for cpu " << maxID << "\n";
                dataCaches[maxID]->incrementNumMSHRs(true);
                
                if(dataCaches[maxID]->isBlockedNoMSHRs()){
                    assert(dataCaches[maxID]->isBlocked());
                    dataCaches[maxID]->clearBlocked(Blocked_NoMSHRs);
                }
                
                break;
            }
            else if(dataCaches[maxID]->getCurrentMSHRCount(false) < maxWB){
                fairfile << "Increasing the size of the writeback queue for cpu " << maxID << "\n";
                dataCaches[maxID]->incrementNumMSHRs(false);
                
                if(dataCaches[maxID]->isBlockedNoWBBuffers()){
                    assert(dataCaches[maxID]->isBlocked());
                    dataCaches[maxID]->clearBlocked(Blocked_NoWBBuffers);
                }
                
                break;
            }
            tmpStall.erase(tmpStall.begin()+maxID);
        }
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
