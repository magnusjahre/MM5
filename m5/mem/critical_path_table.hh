/*
 * critical_path_table.hh
 *
 *  Created on: Jul 18, 2013
 *      Author: jahre
 */

#ifndef CRITICAL_PATH_TABLE_HH_
#define CRITICAL_PATH_TABLE_HH_

#include "mem/requesttrace.hh"
#include "mem/memory_overlap_estimator.hh"

class MemoryOverlapEstimator;
class CriticalPathTableMeasurements;

class CriticalPathTable{
private:

    class CPTRequestEntry{
    public:
       Addr addr;
       int depth;
       bool completed;
       bool valid;
       bool isShared;
       Tick issuedAt;
       Tick completedAt;
       Tick interference;
       Tick cwp;

       CPTRequestEntry() : addr(0), depth(-1), completed(false), valid(false), isShared(false), issuedAt(0), completedAt(0), interference(0), cwp(0){}

       void update(Addr _addr){
           addr = _addr;
           depth = -1;
           completed = false;
           isShared = false;

           issuedAt = curTick;
           completedAt = 0;
           interference = 0;
           cwp = 0;

           assert(!valid);
           valid = true;
       }

       Tick latency(){
    	   return completedAt - issuedAt;
       }
    };

    class CPTCommitEntry{
    public:
    	int id;
    	int depth;
    	Tick startedAt;
    	Tick stalledAt;

    	std::vector<int> children;

    	CPTCommitEntry(){
    		reset();
    	}

    	void reset(){
    		depth = -1;
    		id = 0;
    		startedAt = 0;
    		stalledAt = 0;

    		children.clear();
    	}

    	bool removeChild(int index);

    	bool hasChild(int index);
    };

    MemoryOverlapEstimator* moe;

    std::vector<CPTRequestEntry> pendingRequests;

    Addr stalledOnAddr;
    Tick stalledAt;

    int commitIDCounter;

    CPTCommitEntry pendingCommit;
    int prevCommitDepth;

    int newestValidPtr;
    int oldestValidPtr;

    int traceSampleID;
    int currentSampleID;
    RequestTrace CPTDependencyEdgeTrace;

    CriticalPathTableMeasurements* cplMeasurements;

    bool isSharedRead(MemReqPtr& req, bool hiddenLoad);

    int findRequest(Addr paddr);

    void updateCommitDepthCounter(int newdepth, int criticalPathBufferEntry);

    void updateChildRequest(int bufferIndex, int depth, int commitID);

    void initDependencyEdgeTrace();

    void traceDependencyEdge(Addr from, Addr to, bool fromIsRequest);

    bool isStalled();

    void handleCompletedRequestWhileCommitting(int pendingIndex);

    void updateStallState();

    void incrementBufferPointer(int* bufferPtr);

    void incrementBufferPointerToNextValid(int* bufferPtr);

    void handleFullBuffer();

    void dumpBufferContents();

public:

    CriticalPathTable(MemoryOverlapEstimator* _moe, int _bufferSize);

    void issuedRequest(MemReqPtr& req);

    void completedRequest(MemReqPtr& req, bool hiddenLoad, Tick willCompleteAt);

    void handleCompletedRequestEvent(MemReqPtr& req, bool hiddenLoad);

    void commitPeriodStarted();

    void commitPeriodEnded(Addr stalledOn);

    CriticalPathTableMeasurements getCriticalPathLength(int nextSampleID);

    void addCommitCycle();
};

class CPTMemoryRequestCompletionEvent : public Event
{
    CriticalPathTable* cpt;
	MemReqPtr req;
	bool hiddenLoad;

    public:
    // constructor
    /** A simple constructor. */
	CPTMemoryRequestCompletionEvent (CriticalPathTable* _cpt, MemReqPtr& _req, bool _hiddenLoad)
        : Event(&mainEventQueue), cpt(_cpt), req(_req), hiddenLoad(_hiddenLoad)
    {
    }

    // event execution function
    /** Calls BusInterface::deliver() */
    void process(){
        cpt->handleCompletedRequestEvent(req, hiddenLoad);
        delete this;
    }

    /**
    * Returns the string description of this event.
    * @return The description of this event.
     */
    virtual const char *description(){
        return "Critical Path Table Memory Request Completion Event";
    }
};



#endif /* CRITICAL_PATH_TABLE_HH_ */
