/*
 * critical_path_table.cc
 *
 *  Created on: Jul 18, 2013
 *      Author: jahre
 */

#include "critical_path_table.hh"

using namespace std;

CriticalPathTable::CriticalPathTable(MemoryOverlapEstimator* _moe){
    moe = _moe;

    curCommitDepth = 0;

    nextValidPtr = 0;

    int bufferSize = 20;
    pendingRequests.resize(bufferSize, CPTRequestEntry());
}

int
CriticalPathTable::findRequest(Addr paddr){
    int foundIndex = -1;
    for(int i=0;i<pendingRequests.size();i++){
        if(pendingRequests[i].addr == paddr && pendingRequests[i].valid){
            assert(foundIndex == -1);
            foundIndex = i;
        }
    }
    assert(foundIndex != -1);
    return foundIndex;
}

bool
CriticalPathTable::hasAddress(Addr paddr){
    for(int i=0;i<pendingRequests.size();i++){
        if(pendingRequests[i].addr == paddr && pendingRequests[i].valid){
            return true;
        }
    }
    return false;
}

void
CriticalPathTable::issuedRequest(MemReqPtr& req){
    Addr addr = req->paddr & ~(MOE_CACHE_BLK_SIZE-1);
    int thisDepth = curCommitDepth+1;

    DPRINTF(CPLTable, " %s: Got memory request for addr %d, command %s, inserting at depth %d, pos %d\n",
            moe->name(),
            addr,
            req->cmd,
            thisDepth,
            nextValidPtr);

    if(hasAddress(addr)){
        DPRINTF(CPLTable, " %s: Addr %d already allocated, skipping\n",
                    moe->name(),
                    addr);
        return;
    }

    pendingRequests[nextValidPtr].update(addr, thisDepth);

    int checkCnt = 0;
    while(pendingRequests[nextValidPtr].valid){
        nextValidPtr = (nextValidPtr + 1) % pendingRequests.size();
        checkCnt++;
        if(checkCnt > pendingRequests.size()) fatal("Ran out of CPL buffer space");
    }

    DPRINTF(CPLTable, " %s: Next valid pointer is now %d\n",
            moe->name(),
            nextValidPtr);
}

void
CriticalPathTable::completedRequest(MemReqPtr& req, bool hiddenLoad){

    DPRINTF(CPLTable, " %s: Memory request completed for addr %d, %s, %s, %s\n",
            moe->name(),
            req->paddr,
            req->beenInSharedMemSys ? "shared request" : "private request",
            hiddenLoad ? "hidden load" : "not hidden load",
            req->isStore ? "store" : "not store");

    int pendingIndex = findRequest(req->paddr);
    if(isSharedRead(req, hiddenLoad)){
        DPRINTF(CPLTable, " %s: Request for address %d (index %d) is shared and complete\n",
                        moe->name(),
                        req->paddr,
                        pendingIndex);

        if(pendingRequests[pendingIndex].depth > curCommitDepth){
            curCommitDepth = pendingRequests[pendingIndex].depth;
            DPRINTF(CPLTable, " %s: Setting current commit depth to %d\n",
                    moe->name(),
                    curCommitDepth);

        }

        pendingRequests[pendingIndex].completed = true;
        pendingRequests[pendingIndex].valid = false;
    }
    else{
        DPRINTF(CPLTable, " %s: Request for address %d (index %d) is not applicable, invalidating it\n",
                moe->name(),
                req->paddr,
                pendingIndex);

        pendingRequests[pendingIndex].valid = false;
    }
}

void
CriticalPathTable::commitPeriodStarted(){
    DPRINTF(CPLTable, " %s: commit period started\n", moe->name());
}

void
CriticalPathTable::commitPeriodEnded(){
    DPRINTF(CPLTable, " %s: commit period ended, current commit depth %d\n",
            moe->name(),
            curCommitDepth);
}

int
CriticalPathTable::getCriticalPathLength(){

    DPRINTF(CPLTable, "%s: Returning current commit depth %d, resetting commit depth\n",
            moe->name(),
            curCommitDepth);

    for(int i=0;i<pendingRequests.size();i++){
        if(pendingRequests[i].valid){
            pendingRequests[i].depth = 0;
            DPRINTF(CPLTable, "%s: Resetting depth for request %d, resetting commit depth\n",
                    moe->name(),
                    pendingRequests[i].depth);
        }
    }

    int tmpCommitDepth = curCommitDepth;
    curCommitDepth = 0;
    return tmpCommitDepth;
}

bool
CriticalPathTable::isSharedRead(MemReqPtr& req, bool hiddenLoad){
    if(req->beenInSharedMemSys){
        if(hiddenLoad){
            DPRINTF(CPLTable, "Request %d hides a load, add it\n", req->paddr);
            return true;
        }
        if(req->isStore){
            DPRINTF(CPLTable, "Request %d is a store, skip it\n", req->paddr);
            return false;
        }
        DPRINTF(CPLTable, "Request %d is a regular load, add it\n", req->paddr);
        return true;
    }
    DPRINTF(CPLTable, "Request %d is private, skip it\n", req->paddr);
    return false;
}
