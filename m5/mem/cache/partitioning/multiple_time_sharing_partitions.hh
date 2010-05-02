
#ifndef __MTP_HH__
#define __MTP_HH__

#include "mem/cache/partitioning/cache_partitioning.hh"

class MultipleTimeSharingPartitions : public CachePartitioning{

  private:
    std::vector<std::vector<double> > misscurves;
    std::vector<vector<int> > mtpPartitions;
    int mtpPhase;
    int curMTPPartition;

    void setEqualShare();

    bool calculatePartitions();

  public:
    MultipleTimeSharingPartitions(std::string _name,
                                  int _associativity,
                                  Tick _epochSize,
                                  int _np);

    void handleRepartitioningEvent();

};

#endif //__MTP_HH__

