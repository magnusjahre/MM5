/*
 * critical_path_table.hh
 *
 *  Created on: Jul 18, 2013
 *      Author: jahre
 */

#ifndef CRITICAL_PATH_TABLE_HH_
#define CRITICAL_PATH_TABLE_HH_

#include "mem/memory_overlap_estimator.hh"

class MemoryOverlapEstimator;

class CriticalPathTable{
private:

    class CPTCommitEntry{
    public:
       int depth;

       CPTCommitEntry() : depth(-1){}
    };

    class CPTRequestEntry{
    public:
       Addr addr;
       int depth;
       bool completed;
       bool valid;

       CPTRequestEntry() : addr(0), depth(-1), completed(false), valid(false){}

       void update(Addr _addr, int _depth){
           addr = _addr;
           depth = _depth;
           completed = false;

           assert(!valid);
           valid = true;
       }
    };

    MemoryOverlapEstimator* moe;

    std::vector<CPTRequestEntry> pendingRequests;

    int curCommitDepth;

    int nextValidPtr;

    bool isSharedRead(MemReqPtr& req, bool hiddenLoad);

    int findRequest(Addr paddr);

    bool hasAddress(Addr paddr);

public:

    CriticalPathTable(MemoryOverlapEstimator* _moe);

    void issuedRequest(MemReqPtr& req);

    void completedRequest(MemReqPtr& req, bool hiddenLoad);

    void commitPeriodStarted();

    void commitPeriodEnded();

    int getCriticalPathLength();
};


#endif /* CRITICAL_PATH_TABLE_HH_ */
