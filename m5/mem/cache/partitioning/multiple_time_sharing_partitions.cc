
#include "mem/cache/partitioning/multiple_time_sharing_partitions.hh"

using namespace std;

MultipleTimeSharingPartitions::MultipleTimeSharingPartitions(std::string _name,
                                                             int _associativity,
                                                             Tick _epochSize,
                                                             int _np)
: CachePartitioning(_name, _associativity, _epochSize, _np){

    misscurves.resize(_np, vector<double>(_associativity, 0));
    mtpPhase = -1;
}

void
MultipleTimeSharingPartitions::setEqualShare(){
	vector<int> initialPartition = vector<int>(partitioningCpuCount, associativity / partitioningCpuCount);

	debugPrintPartition(initialPartition, "Initializing sharing with partition: ");

	cache->setCachePartition(initialPartition);
}

void
MultipleTimeSharingPartitions::handleRepartitioningEvent(){

	DPRINTF(CachePartitioning, "Running MTP partitioning\n");

    switch(mtpPhase){

        case -1:{
            mtpPhase = 0; //enter measuring phase
            setEqualShare();
            cache->enablePartitioning();
            break;
        }
        case 0:{

            // measurement phase finished
            for(int i=0;i<shadowTags.size();i++){
                vector<double> missRateProfile = shadowTags[i]->getMissRates();
                misscurves[i] = missRateProfile;
            }

            DPRINTF(CachePartitioning, "Miss rate curves:\n");
            for(int i=0;i<cache->cpuCount;i++){
                DPRINTFR(CachePartitioning, "CPU %d:", i);
                for(int j=0;j<misscurves[i].size();j++){
                    DPRINTFR(CachePartitioning, " %d:%f", j, misscurves[i][j]);
                }
                DPRINTFR(CachePartitioning, "\n");
            }

            // Calculate partitions
            bool partitioningNeeded = calculatePartitions();

            // If all threads are suppliers, no partitioning is needed
            // Use static uniform partitioning
            if(!partitioningNeeded){
                DPRINTF(CachePartitioning, "No partitioning found\n");

                // enforce static partitioning
                int equalQuota = associativity / cache->cpuCount;
                vector<int> staticPartition(cache->cpuCount, equalQuota);

                cache->setCachePartition(staticPartition);
                curMTPPartition = -1;

            }
            else{

				// exit measurement phase
				mtpPhase = 1;

				// enforce the first MTP partition
				curMTPPartition = 0;
				cache->setCachePartition(mtpPartitions[curMTPPartition]);

				debugPrintPartition(mtpPartitions[curMTPPartition], "Partition found, enforcing first partition: ");
            }

            break;
        }
        case 1:{

            // switch to next partitioning
            curMTPPartition = curMTPPartition + 1;
            if(curMTPPartition < mtpPartitions.size()){
            	debugPrintPartition(mtpPartitions[curMTPPartition], "Updating partition, setting partition to: ");
				assert(curMTPPartition >= 0 && curMTPPartition < mtpPartitions.size());
				cache->setCachePartition(mtpPartitions[curMTPPartition]);
            }
            else{
            	DPRINTF(CachePartitioning, "Partitioning finished, returning to equal share\n");
            	setEqualShare();
            	mtpPhase = 0;
            }
            break;
        }
        default:{
            fatal("MTP: unknown selection value encountered");
        }
    }

    schedulePartitionEvent();
}

/**
* This method implements Chang and Sohi's MTP allocation algorithm
*/
bool
MultipleTimeSharingPartitions::calculatePartitions(){

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

    DPRINTF(CachePartitioning, "Initial c_expand:");
    for(int i=0;i<cache->cpuCount;i++){
        DPRINTFR(CachePartitioning, " P%d=%d", i, c_expand[i]);
    }
    DPRINTFR(CachePartitioning, "\n");

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

    DPRINTF(CachePartitioning, "Initial c_shrink:");
    for(int i=0;i<cache->cpuCount;i++){
        DPRINTFR(CachePartitioning, " P%d=%d", i, c_shrink[i]);
    }
    DPRINTFR(CachePartitioning, "\n");


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

    DPRINTF(CachePartitioning, "Analysis resulted in the following partitions:\n");
    for(int p=0;p<mtpPartitions.size();p++){
        DPRINTFR(CachePartitioning, "P%d:", p);
        for(int i=0;i<mtpPartitions[p].size();i++){
            DPRINTFR(CachePartitioning, " %d:%d", i, mtpPartitions[p][i]);
        }
        DPRINTFR(CachePartitioning, "\n");
    }

    return true;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MultipleTimeSharingPartitions)
    Param<int> associativity;
    Param<int> epoch_size;
    Param<int> np;
END_DECLARE_SIM_OBJECT_PARAMS(MultipleTimeSharingPartitions)


BEGIN_INIT_SIM_OBJECT_PARAMS(MultipleTimeSharingPartitions)
    INIT_PARAM(associativity, "Cache associativity"),
    INIT_PARAM_DFLT(epoch_size, "Size of an epoch", 10000000),
	INIT_PARAM(np, "Number of cores")
END_INIT_SIM_OBJECT_PARAMS(MultipleTimeSharingPartitions)


CREATE_SIM_OBJECT(MultipleTimeSharingPartitions)
{
    return new MultipleTimeSharingPartitions(getInstanceName(),
											 associativity,
											 epoch_size,
											 np);
}

REGISTER_SIM_OBJECT("MultipleTimeSharingPartitions", MultipleTimeSharingPartitions)

#endif //DOXYGEN_SHOULD_SKIP_THIS

