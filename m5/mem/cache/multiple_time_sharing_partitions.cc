
#include "mem/cache/multiple_time_sharing_partitions.hh"

using namespace std;

MultipleTimeSharingParititions::MultipleTimeSharingParititions(BaseCache* _cache,
                                                               int _associativity,
                                                               Tick _epochSize,
                                                               std::vector<LRU*> _shadowTags,
                                                               Tick _uniformPartitioningStart){

    cache = _cache;
    associativity = _associativity;
    shadowTags = _shadowTags;

    misscurves.resize(_cache->cpuCount, vector<double>(_associativity, 0));
    mtpPhase = -1;
    mtpEpochSize = _epochSize;
    assert(mtpEpochSize > 0);
    assert(!shadowTags.empty());

    repartEvent = new CacheRepartitioningEvent(_cache);
    repartEvent->schedule(_uniformPartitioningStart);
}

MultipleTimeSharingParititions::~MultipleTimeSharingParititions(){
	assert(!repartEvent->scheduled());
	delete repartEvent;
}

void
MultipleTimeSharingParititions::handleRepartitioningEvent(){

	cout << curTick << " " << cache->name() << ": handleRepartEvent called\n";

    switch(mtpPhase){

        case -1:{
        	cout << curTick << " " << cache->name() << ": phase -1, entering phase 0\n";
            mtpPhase = 0; //enter measuring phase
            break;
        }
        case 0:{

        	cout << curTick << " " << cache->name() << ": in phase 0\n";
            string filename = cache->name()+"MTPTrace.txt";
            ofstream mtptracefile(filename.c_str());

            // measurement phase finished
            for(int i=0;i<shadowTags.size();i++){
                vector<double> missRateProfile = shadowTags[i]->getMissRates();
                misscurves[i] = missRateProfile;
                mtptracefile << "Warmup stats for CPU" << i << " shadow tags: " << shadowTags[i]->getTouchedRatio() << "\n";
            }

            mtptracefile << "Measured miss rate curves\n";
            DPRINTF(MTP, "Miss rate curves:\n");
            for(int i=0;i<cache->cpuCount;i++){
                mtptracefile << "CPU" << i << ":";
                DPRINTFR(MTP, "CPU %d:", i);
                for(int j=0;j<misscurves[i].size();j++){
                    DPRINTFR(MTP, " %d:%f", j, misscurves[i][j]);
                    mtptracefile << " " << misscurves[i][j];
                }
                DPRINTFR(MTP, "\n");
                mtptracefile << "\n";
            }

            // exit measurement phase
            mtpPhase = 1;

            // Calculate partitions
            bool partitioningNeeded = calculatePartitions();

            // If all threads are suppliers, no partitioning is needed
            // Use static uniform partitioning
            if(!partitioningNeeded){
                DPRINTF(MTP, "No partitioning found\n");
                mtptracefile << "No partitioning found, reverting to static uniform partitioning\n";
                mtptracefile.flush();
                mtptracefile.close();

                // enforce static partitioning
                int equalQuota = associativity / cache->cpuCount;
                vector<int> staticPartition(cache->cpuCount, equalQuota);

                cache->setMTPPartition(staticPartition);
                curMTPPartition = -1;

                return;
            }

            for(int i=0;i<mtpPartitions.size();i++){
                mtptracefile << "Partition " << i << ":";
                for(int j=0;j<mtpPartitions[i].size();j++){
                    mtptracefile << " CPU" << j << "=" << mtpPartitions[i][j];
                }
                mtptracefile << "\n";
            }

            mtptracefile.flush();
            mtptracefile.close();

            // enforce the first MTP partition
            curMTPPartition = 0;
            cache->setMTPPartition(mtpPartitions[curMTPPartition]);

            // reset hitprofiles and curCPU
            for(int i=0;i<cache->cpuCount;i++){
                for(int j=0;j<associativity;j++){
                    misscurves[i][j] = 0;
                }
            }
            break;
        }
        case 1:{

        	cout << curTick << " " << cache->name() << ": in phase 1\n";

            // switch to next partitioning
            curMTPPartition = (curMTPPartition + 1) % mtpPartitions.size();
            assert(curMTPPartition >= 0 && curMTPPartition < mtpPartitions.size());
            cache->setMTPPartition(mtpPartitions[curMTPPartition]);
            break;
        }
        default:{
            fatal("MTP: unknown selection value encountered");
        }
    }

    // reset counters and reschedule event
    for(int i=0;i<shadowTags.size();i++) shadowTags[i]->resetHitCounters();
    assert(repartEvent != NULL);
    repartEvent->schedule(curTick + mtpEpochSize);
}

/**
* This method implements Chang and Sohi's MTP allocation algorithm
*/
bool
MultipleTimeSharingParititions::calculatePartitions(){

    // Initialization
    vector<vector<int> > partitions;
    int baseSetSize = associativity / cache->cpuCount;
    int minSafeSetIndex = (associativity / cache->cpuCount) -1;
    int capacity = associativity;

    vector<int> c_expand = vector<int>(cache->cpuCount, 0);
    for(int i=0;i<cache->cpuCount;i++){
        //NOTE: this implementation interpretes Chang and Sohi to mean the largest speedup relative to the guaranteed partition
        double tmpMin = misscurves[i][minSafeSetIndex];
        int minIndex = minSafeSetIndex;
        for(int j=minSafeSetIndex+1;j<associativity;j++){
            if(misscurves[i][j] < tmpMin){
                tmpMin = misscurves[i][j];
                minIndex = j;
            }
        }
        c_expand[i] = minIndex;
    }

    DPRINTF(MTP, "Initial c_expand:");
    for(int i=0;i<cache->cpuCount;i++){
        DPRINTFR(MTP, " P%d=%d", i, c_expand[i]);
    }
    DPRINTFR(MTP, "\n");

    vector<int> c_shrink = vector<int>(cache->cpuCount, 0);
    for(int i=0;i<cache->cpuCount;i++){
        c_shrink[i] = minSafeSetIndex;
        double base = misscurves[i][minSafeSetIndex];
        double expandSpeedup = base - misscurves[i][c_expand[i]];

        for(int j=minSafeSetIndex-1;j>=0;j--){
            if(expandSpeedup >= (cache->cpuCount-1)*(misscurves[i][j] - base)) c_shrink[i] = j;
            else break;
        }
    }

    DPRINTF(MTP, "Initial c_shrink:");
    for(int i=0;i<cache->cpuCount;i++){
        DPRINTFR(MTP, " P%d=%d", i, c_shrink[i]);
    }
    DPRINTFR(MTP, "\n");


    // Step 1 - Remove supplier threads
    int supplierCount = 0;
    int thrashingCount = 0;
    vector<bool> supplier = vector<bool>(cache->cpuCount, false);
    for(int i=0;i<supplier.size();i++){
        // Thread is supplier is max performance is attained with the guaranteed partition
        if(c_shrink[i] == minSafeSetIndex){
            supplier[i] = true;
            supplierCount++;
            capacity = capacity - baseSetSize;
        }
        else{
            supplier[i] = false;
            thrashingCount++;
        }
    }

    if(thrashingCount <= 1) return false;

    // Step 2 -- determine thrashing thread set
    bool stable = false;
    while(thrashingCount > 0 && !stable){
        stable = true;
        for(int i=0;i<cache->cpuCount;i++){
            if(!supplier[i]){
                //thread i is currently designated as thrashing
                int usedByOtherThrashers = 0;
                for(int j=0;j<cache->cpuCount;j++){
                    if(!supplier[j] && j != i){
                        usedByOtherThrashers += c_shrink[j]+1;
                    }
                }
                int freeSets = capacity - usedByOtherThrashers;

                // reduce expanding capacity if it cannot be met
                c_expand[i] = (freeSets-1 > c_expand[i] ? c_expand[i] : freeSets-1);

                // Thrashing test
                bool result = false;

                double base = misscurves[i][minSafeSetIndex];
                double expandSpeedup = base - misscurves[i][c_expand[i]];

                if(expandSpeedup >= (thrashingCount-1)*(misscurves[i][c_shrink[i]] - base)){
                    result = true;
                }
                else{
                    supplier[i] = true;
                    thrashingCount--;
                    supplierCount++;
                    capacity = capacity - baseSetSize;
                    c_shrink[i] = minSafeSetIndex;
                }
                stable &= result;
            }
        }
    }

    if(thrashingCount <= 1) return false;

    // Step 3 - build partitions
    int partitionCount = thrashingCount;
    mtpPartitions = vector<vector<int> >(partitionCount, vector<int>(cache->cpuCount, 0));
    for(int i=0;i<partitionCount;i++) for(int j=0;j<cache->cpuCount;j++) mtpPartitions[i][j] = c_shrink[j]+1;
    vector<bool> expanded = vector<bool>(cache->cpuCount, false);

    int restCapacity = associativity;
    for(int i=0;i<cache->cpuCount;i++) restCapacity = restCapacity - (c_shrink[i]+1);

    vector<int> thrasherIDs;
    for(int i=0;i<cache->cpuCount;i++) if(!supplier[i]) thrasherIDs.push_back(i);

    for(int p=0;p<partitionCount;p++){
        int j=p;
        int visitedCount = 0;
        int tmpCapacity = restCapacity;
        while(visitedCount < thrasherIDs.size()){
            int extraSpace = (c_expand[thrasherIDs[j]]+1) - (c_shrink[thrasherIDs[j]]+1);
            if(extraSpace <= tmpCapacity){
                mtpPartitions[p][thrasherIDs[j]] += extraSpace;
                tmpCapacity = tmpCapacity - extraSpace;
            }
            else if(tmpCapacity > 0){
                mtpPartitions[p][thrasherIDs[j]] += tmpCapacity;
                tmpCapacity = 0;
            }

            if(mtpPartitions[p][thrasherIDs[j]] >= c_expand[thrasherIDs[j]]+1) expanded[thrasherIDs[j]] = true;
            j = (j + 1) % thrasherIDs.size();
            visitedCount++;
        }
    }

    DPRINTF(MTP, "Analysis resulted in the following partitions:\n");
    for(int p=0;p<mtpPartitions.size();p++){
        DPRINTFR(MTP, "P%d:", p);
        for(int i=0;i<mtpPartitions[p].size();i++){
            DPRINTFR(MTP, " %d:%d", i, mtpPartitions[p][i]);
        }
        DPRINTFR(MTP, "\n");
    }

    return true;
}

