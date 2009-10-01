
#ifndef __RING_HH__
#define __RING_HH__

#include "address_dependent_ic.hh"

typedef enum{
    RING_CLOCKWISE,
    RING_COUNTERCLOCKWISE,
    RING_DIRCOUNT
} RING_DIRECTION;

class Ring : public AddressDependentIC{

    private:

        struct RingRequestEntry{
            MemReqPtr req;
            Tick enteredAt;
            std::vector<int> resourceReq;
            int direction;

            RingRequestEntry(MemReqPtr& _req, Tick _enteredAt, std::vector<int> _resourceReq, int _direction){
                req = _req;
                enteredAt = _enteredAt;
                resourceReq = _resourceReq;
                direction = _direction;
            }
        };

        int TOP_LINK_ID;
        int BOTTOM_LINK_ID;

        int sharedCacheBankCount;
        int queueSize;
        int numberOfRequestRings;
        int numberOfResponseRings;

        int detailedSimStartTick;

        int singleProcessorID;

        std::vector<std::map<int, std::vector<std::list<std::pair<Tick, RingRequestEntry> > > > > ringLinkOccupied;

        std::vector<std::list<RingRequestEntry> > ringRequestQueue;
        std::vector<std::list<RingRequestEntry> > ringResponseQueue;

        std::vector<int> inFlightRequests;
        int recvBufferSize;

        std::vector<std::list<RingRequestEntry> > deliverBuffer;

        ADIArbitrationEvent* arbEvent;

        std::vector<int> findResourceRequirements(MemReqPtr& req, int fromIntID, RING_DIRECTION* direction);
        std::vector<int> findMasterPath(MemReqPtr& req, int uphops, int downhops, RING_DIRECTION* direction);
        std::vector<int> findSlavePath(MemReqPtr& req, int uphops, int downhops, RING_DIRECTION* direction);

        int getUphops(int cpuID, int slaveID);
        int getDownhops(int cpuID, int slaveID);


        std::vector<int> findServiceOrder(std::vector<std::list<RingRequestEntry> >* queue);

        void attemptToScheduleArbEvent();

        void removeOldEntries(int ringID);
        bool checkStateAndSend(RingRequestEntry entry, int ringID, bool toSlave);

        void arbitrateRing(std::vector<std::list<RingRequestEntry> >* queue, int startRingID, int endRingID, bool toSlave);

        void checkRingOrdering(int ringID);

        bool hasWaitingRequests();

        void setDestinationIntID(MemReqPtr& req, int fromIntID);

    public:
        Ring(const std::string &_name,
                       int _width,
                       int _clock,
                       int _transDelay,
                       int _arbDelay,
                       int _cpu_count,
                       HierParams *_hier,
                       AdaptiveMHA* _adaptiveMHA,
                       Tick _detailedStart,
                       int _singleProcessorID,
                       InterferenceManager* _intman);

        virtual void send(MemReqPtr& req, Tick time, int fromID);

        virtual void arbitrate(Tick time);

        virtual void deliver(MemReqPtr& req, Tick cycle, int toID, int fromID);

        virtual void clearBlocked(int fromInterface);

        virtual int registerInterface(InterconnectInterface* interface, bool isSlave, int processorID);
};

#endif //__RING_HH__
