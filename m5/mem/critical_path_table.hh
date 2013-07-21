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

    class CPTRequestEntry{
    public:
       Addr addr;
       int depth;
       bool completed;
       bool valid;
       bool isShared;

       CPTRequestEntry() : addr(0), depth(-1), completed(false), valid(false), isShared(false){}

       void update(Addr _addr){
           addr = _addr;
           depth = -1;
           completed = false;
           isShared = false;

           assert(!valid);
           valid = true;
       }
    };

    class CPTCommitEntry{
    public:
       int depth;
       Tick startedAt;
       Tick stalledAt;

       std::vector<int> children;

       CPTCommitEntry(){
    	   reset();
       }

       void reset(){
    	   depth = -1;
    	   startedAt = 0;
    	   stalledAt = 0;

    	   children.clear();
       }

       void removeChild(int index);
    };

    MemoryOverlapEstimator* moe;

    std::vector<CPTRequestEntry> pendingRequests;

    Addr stalledOnAddr;
    Tick stalledAt;

    int curCommitDepth;

    CPTCommitEntry pendingCommit;

    int nextValidPtr;

    bool isSharedRead(MemReqPtr& req, bool hiddenLoad);

    int findRequest(Addr paddr);

    bool hasAddress(Addr paddr);

public:

    CriticalPathTable(MemoryOverlapEstimator* _moe);

    void issuedRequest(MemReqPtr& req);

    void completedRequest(MemReqPtr& req, bool hiddenLoad);

    void commitPeriodStarted();

    void commitPeriodEnded(Addr stalledOn);

    int getCriticalPathLength();
};


#endif /* CRITICAL_PATH_TABLE_HH_ */
