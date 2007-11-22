
#ifndef __CROSSBAR_HH__
#define __CROSSBAR_HH__

#include <iostream>
#include <vector>
#include <queue>

#include "mem/base_hier.hh"
#include "crossbar_interface.hh"
#include "sim/eventq.hh"
#include "sim/stats.hh"
        
class CrossbarInterface;
class CrossbarArbitrationEvent;

class Crossbar : public BaseHier
{
    private:
        
        class CrossbarRequest{
            public:
                Tick time;
                int fromID;
            
                CrossbarRequest(Tick _time, int _fromID)
                    : time(_time), fromID(_fromID)
                {
                }
        };
        
        /*
        class BlockedRequest{
            public:
                MemReqPtr req;
                int fromID;
                int toID;
                
                BlockedRequest(MemReqPtr &_req, int _fromID, int _toID)
                    : req(_req), fromID(_fromID), toID(_toID)
                {
                }
        };
        */
        
        int interfaceCount;
        int l2InterfaceID;
        
        bool blocked;
        int waitingFor;
        Tick blockedAt;
        
        std::vector<CrossbarInterface* > interfaces;
        std::vector<Tick> arbitrationCycles;
        //std::vector<std::vector<CrossbarRequest*>* > requestQueues;
        std::vector<CrossbarRequest* > requestQueue;

    protected:
  
        Stats::Scalar<> requests;
        
        Stats::Scalar<> totalArbitrationCycles;
        
        Stats::Scalar<> lineUseCycles;
        
        Stats::Scalar<> queueCycles;
        
        Stats::Scalar<> nullRequests;
        
        Stats::Scalar<> duplicateRequests;
        
        Stats::Scalar<> numClearBlocked;
        
        Stats::Scalar<> numSetBlocked;

    public:
        int crossbarWidth;
        int crossbarClock;
        int crossbarDelay;
        int arbitrationDelay;
        int numL1Caches;
        
        std::vector<CrossbarArbitrationEvent*> arbitrationEvents;
    
    private:
        
        /* Private methods */        
        void scheduleArbitrationEvent(Tick cycle);
        void* getRequest(Tick time, Tick* nextArbitrationTime);
        
        // DEBUG
        void printRequestQueue();
        void printArbCycles();
        void printBlockedRequests();
        
        
    public:
        
        Crossbar(const std::string &name, 
                 int width, 
                 int clock,
                 int transDelay,
                 int arbDelay,
                 int L1cacheCount,
                 HierParams *hier);
        
        ~Crossbar();
        
        void regStats();

        void resetStats();
        
        int registerInterface(CrossbarInterface* interface,
                              bool isL2);
        
        void rangeChange();
        
        void request(Tick time, int toID, int fromID);

        void sendData(MemReqPtr& req, Tick time, int toID, int fromID);
        
        void arbitrateCrossbar(Tick cycle);
        
        void deliverData(MemReqPtr& req, Tick cycle, int toID, int fromID);
        
        int getL2interfaceID();
        
        void incNullRequests();
        
        void incDuplicateRequests();
        
        void setBlocked(int fromInterface);
        
        void clearBlocked(int fromInterface);
        
};

class CrossbarArbitrationEvent : public Event
{

    public:
        Crossbar *crossbar;
        
        CrossbarArbitrationEvent(Crossbar *_crossbar)
            : Event(&mainEventQueue), crossbar(_crossbar)
        {
        }

        void process();

        virtual const char *description();
};

class CrossbarDeliverEvent : public Event
{
    
    public:
        
        Crossbar *crossbar;
        MemReqPtr req;
        int toID;
        int fromID;
        
        CrossbarDeliverEvent(Crossbar *_crossbar, MemReqPtr& _req, int _toID, int _fromID)
            : Event(&mainEventQueue), crossbar(_crossbar), req(_req), toID(_toID), fromID(_fromID)
        {
        }

        void process();

        virtual const char *description();
};

#endif // CROSSBAR_HH
