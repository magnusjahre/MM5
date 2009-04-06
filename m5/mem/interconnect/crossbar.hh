
#ifndef __CROSSBAR_HH__
#define __CROSSBAR_HH__

#include <iostream>
#include <vector>
#include <queue>

#include "address_dependent_ic.hh"

#define DEBUG_CROSSBAR

/**
* This class implements a crossbar interconnect inspired by the crossbar used
* in IBM's Power 4 and Power 5 processors. Here, two crossbar connects all L1
* caches to all L2 banks. One crossbar is added in the L1 to L2 direction and
* the other crossbar runs in the L2 to L1 direction. L1 to L1 transfers are
* made possible by connecting all L1 caches to a shared bus. Instruction and
* data caches for the same core share address and data lines.
*
* The crossbar modelled in this class differs from the IBM design in one way:
* - The IBM crossbar only has address lines in the L2 to L1 direction. This
*   implementation has address lines in both directions.
*
* Arbitration and transfer in the crossbar are pipelined.
*
* @author Magnus Jahre
*/

class Crossbar : public AddressDependentIC
{

    private:
        Tick detailedSimStartTick;
        int crossbarTransferDelay;

        int shared_cache_wb_buffers;
        int shared_cache_mshrs;

        struct DeliveryBufferEntry{
            MemReqPtr req;
            Tick enteredAt;
            bool blameForBlocking;

            DeliveryBufferEntry(MemReqPtr& _req, Tick _enteredAt, bool _blameForBlocking){
                req = _req;
                enteredAt = _enteredAt;
                blameForBlocking = _blameForBlocking;
            }
        };

        std::vector<list<std::pair<MemReqPtr, int> > > crossbarRequests;
        std::vector<list<std::pair<MemReqPtr, int> > > crossbarResponses;

        std::vector<list<DeliveryBufferEntry> > slaveDeliveryBuffer;

        std::vector<int> requestsInProgress;

        int perEndPointQueueSize;
        int requestOccupancyTicks;
        int requestL2BankCount;

        ADIArbitrationEvent* crossbarArbEvent;

        bool attemptDelivery(std::list<std::pair<MemReqPtr, int> >* currentQueue, int* crossbarState, bool toSlave);

        int addBlockedInterfaces();

        std::vector<int> findServiceOrder(std::vector<std::list<std::pair<MemReqPtr, int> > >* currentQueue);

        bool blockingDueToPrivateAccesses(int blockedCPU);

    public:

        Crossbar(const std::string &_name,
                 int _width,
                 int _clock,
                 int _transDelay,
                 int _arbDelay,
                 int _cpu_count,
                 HierParams *_hier,
                 AdaptiveMHA* _adaptiveMHA,
                 bool _useNFQArbitration,
                 Tick _detailedSimStartTick,
                 int _shared_cache_wb_buffers,
                 int _shared_cache_mshrs,
                 int _pipe_stages,
                 InterferenceManager* _intman);

        ~Crossbar(){ }

        void send(MemReqPtr& req, Tick time, int fromID);

        void arbitrate(Tick time);

        void deliver(MemReqPtr& req, Tick cycle, int toID, int fromID);

        virtual void clearBlocked(int fromInterface);
};



#endif // __CROSSBAR_HH__
