

void
AdaptiveMHA::addAloneInterference(int extraDelay, int victimCPU, InterferenceType type){
    
    if(curTick > startRunning){
        
        switch(type){
            case MEMORY_SERIALIZATION_INTERFERENCE:
                busInterference[victimCPU] += extraDelay;
                break;
            case MEMORY_BLOCKED_INTERFERENCE:
                busBlockedInterference[victimCPU] += extraDelay;
                break;
            case MEMORY_PRIVATE_BLOCKED_INTERFERENCE:
                busPrivateBlockedInterference[victimCPU] -= extraDelay;
                break;
            case L2_CAPACITY_INTERFERENCE:
                l2CapInterference[victimCPU] += extraDelay;
                break;
            case L2_INTERFERENCE:
                fatal("this interference is now handled in the crossbar");
                l2BwInterference[victimCPU] += extraDelay;
                break;
            case INTERCONNECT_INTERFERENCE:
                interconnectInterference[victimCPU] += extraDelay;
                break;
            default:
                fatal("unknown interference type");
        }
    }
}

void 
AdaptiveMHA::addRequestLatency(Tick latency, int cpuID){
    
    if(curTick > startRunning){
        
        totalAloneDelay[cpuID] += latency;
        numberOfAloneRequests[cpuID]++;
        
        if(numberOfAloneRequests[cpuID] == dumpAtNumReqs) dumpAloneInterference(cpuID);
    }
}

void
AdaptiveMHA::dumpAloneInterference(int cpuID){
    
    // compute values
    Tick totalInterference = 0;
    totalInterference += busInterference[cpuID];
    totalInterference += busBlockedInterference[cpuID];
    totalInterference += busPrivateBlockedInterference[cpuID];
    totalInterference += l2CapInterference[cpuID];
    totalInterference += l2BwInterference[cpuID];
    totalInterference += interconnectInterference[cpuID];
    
    double avgSharedLat = (double) ((double) totalAloneDelay[cpuID] / (double) numberOfAloneRequests[cpuID]);
    double avgInterferenceLat = (double) ((double) totalInterference / (double) numberOfAloneRequests[cpuID]);
    
//     cout << curTick << ": CPU" << cpuID << ", interference " << totalInterference << " total delay " << totalAloneDelay[cpuID] << ", reqs " << numberOfAloneRequests[cpuID] << "\n";
//     
//     cout << curTick << ": CPU" << cpuID << " avg shared latency " << avgSharedLat << "\n";
//     cout << curTick << ": CPU" << cpuID << " avg interference " << avgInterferenceLat << "\n";
    
    // write to file
    stringstream filename;
    filename << "CPU" << cpuID << aloneInterferenceFileName;
    ofstream interferencefile(filename.str().c_str(), ofstream::app);
    interferencefile << (dumpAtNumReqs * dumpCount[cpuID]) << ";";
    interferencefile << avgSharedLat << ";";
    interferencefile << avgInterferenceLat << ";";
    interferencefile << busInterference[cpuID] << ";";
    interferencefile << busBlockedInterference[cpuID] << ";";
    interferencefile << busPrivateBlockedInterference[cpuID] << ";";
    interferencefile << l2CapInterference[cpuID] << ";";
    interferencefile << l2BwInterference[cpuID] << ";";
    interferencefile << interconnectInterference[cpuID] << "\n";
    interferencefile.flush();
    interferencefile.close();
    
    // reset storage
    busInterference[cpuID] = 0;
    busBlockedInterference[cpuID] = 0;
    busPrivateBlockedInterference[cpuID] = 0;
    l2CapInterference[cpuID] = 0;
    l2BwInterference[cpuID] = 0;
    interconnectInterference[cpuID] = 0;
    
    totalAloneDelay[cpuID] = 0;
    numberOfAloneRequests[cpuID] = 0;
    
    dumpCount[cpuID]++;
}

