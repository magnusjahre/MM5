
#ifndef __MTP_HH__
#define __MTP_HH__

#include "mem/cache/base_cache.hh"
#include "mem/cache/tags/lru.hh"

class MultipleTimeSharingParititions{


  private:
    BaseCache* cache;

    int associativity;
    std::vector<LRU*> shadowTags;

    CacheRepartitioningEvent* repartEvent;
    std::vector<std::vector<double> > misscurves;
    std::vector<vector<int> > mtpPartitions;
    int mtpPhase;
    int curMTPPartition;
    Tick mtpEpochSize;

  public:
    MultipleTimeSharingParititions(BaseCache* _cache,
                                   int _associativity,
                                   Tick _epochSize,
                                   std::vector<LRU*> _shadowTags,
                                   Tick _uniformPartitioningStart);

    ~MultipleTimeSharingParititions();

    void handleRepartitioningEvent();

    bool calculatePartitions();
};

#endif //__MTP_HH__

