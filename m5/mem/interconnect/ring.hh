
#ifndef __RING_HH__
#define __RING_HH__

#include "address_dependent_ic.hh"

class Ring : public AddressDependentIC{ 
    
    private:
        
        struct RingRequestEntry{
            MemReqPtr req;
            Tick enteredAt;
            std::vector<int> resourceReq;
            
            RingRequestEntry(MemReqPtr& _req, Tick _enteredAt, std::vector<int> _resourceReq){
                req = _req;
                enteredAt = _enteredAt;
                resourceReq = _resourceReq;
            }
        };
        
        int TOP_LINK_ID;
        int BOTTOM_LINK_ID;
        
        int sharedCacheBankCount;
        int queueSize;
        int numberOfRequestRings;
        int numberOfResponseRings;
        
        std::vector<std::map<int, std::list<Tick> > > ringLinkOccupied;
        
        std::vector<std::list<RingRequestEntry> > ringRequestQueue;
        std::vector<std::list<RingRequestEntry> > ringResponseQueue;
        
        std::vector<int> inFlightRequests;
        int recvBufferSize;
        
        ADIArbitrationEvent* arbEvent;
        
        std::vector<int> findMasterResourceRequirements(MemReqPtr& req, int fromIntID);
        std::vector<int> findSlaveResourceRequirements(MemReqPtr& req);
        
        std::vector<int> findServiceOrder(std::vector<std::list<RingRequestEntry> >* queue);
        
        void attemptToScheduleArbEvent();
        
        void removeOldEntries(int ringID);
        bool checkStateAndSend(RingRequestEntry entry, int ringID, bool toSlave);
        
        void arbitrateRing(std::vector<std::list<RingRequestEntry> >* queue, int startRingID, int endRingID, bool toSlave);
    
    public:
        Ring(const std::string &_name, 
                       int _width, 
                       int _clock,
                       int _transDelay,
                       int _arbDelay,
                       int _cpu_count,
                       HierParams *_hier,
                       AdaptiveMHA* _adaptiveMHA);
        
        virtual void send(MemReqPtr& req, Tick time, int fromID);
        
        virtual void arbitrate(Tick time);
        
        virtual void deliver(MemReqPtr& req, Tick cycle, int toID, int fromID);
};

#endif //__RING_HH__