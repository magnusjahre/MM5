
#ifndef __CROSSBAR_HH__
#define __CROSSBAR_HH__

#include <iostream>
#include <vector>
#include <queue>

#include "interconnect.hh"

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

class CrossbarArbitrationEvent;

class Crossbar : public Interconnect
{
    
    private:
        Tick detailedSimStartTick;
        int crossbarTransferDelay;
        
        std::vector<list<std::pair<MemReqPtr, int> > > crossbarRequests;
        std::vector<list<std::pair<MemReqPtr, int> > > crossbarResponses;
        
        std::vector<list<std::pair<MemReqPtr, Tick> > > slaveDeliveryBuffer;
        std::vector<int> notRetrievedRequests;
        std::vector<int> requestsInProgress;
        
        int perEndPointQueueSize;
        int requestOccupancyTicks;
        int requestL2BankCount;
        std::vector<bool> blockedLocalQueues;
        
        CrossbarArbitrationEvent* crossbarArbEvent;
        
        bool attemptDelivery(std::list<std::pair<MemReqPtr, int> >* currentQueue, int* crossbarState, bool toSlave);
        
        int addBlockedInterfaces();
        
        void retriveAdditionalRequests();
        
        std::vector<int> findServiceOrder(std::vector<std::list<std::pair<MemReqPtr, int> > >* currentQueue);
        
    public:
        
        /**
        * This constructor initialises a few member variables, but sends all 
        * parameters to the Interconnect constructor.
        *
        * @param _name       The object name from the configuration file. This
        *                    is passed on to BaseHier and SimObject
        * @param _width      The bit width of the transmission lines in the
        *                    interconnect
        * @param _clock      The number of processor cycles in one interconnect
        *                    clock cycle.
        * @param _transDelay The end-to-end transfer delay through the 
        *                    interconnect in CPU cycles
        * @param _arbDelay   The lenght of an arbitration in CPU cycles
        * @param _cpu_count  The number of processors in the system
        * @param _hier       Hierarchy parameters for BaseHier
        *
        * @see Interconnect
        */
        Crossbar(const std::string &_name,
                 int _width, 
                 int _clock,
                 int _transDelay,
                 int _arbDelay,
                 int _cpu_count,
                 HierParams *_hier,
                 AdaptiveMHA* _adaptiveMHA,
                 bool _useNFQArbitration,
                 Tick _detailedSimStartTick);
        
        /**
        * This destructor deletes the request queues that are dynamically
        * allocated when the first request is recieved.
        */
        ~Crossbar(){ }
        
        void request(Tick time, int fromID);

        /**
        * The send method is called when an interface is granted access and
        * finds the destination interface from the values in the request. Then,
        * it adds the request to a delivery queue and schedules a delivery
        * event if needed.
        *
        * @param req    The memory request to send.
        * @param time   The clock cycle the method was called at.
        * @param fromID The interface ID of the sender interface.
        */
        void send(MemReqPtr& req, Tick time, int fromID);
        
        /**
        * The crossbar arbitration method removes the oldest request from each
        * request queue each cycle. The request must have experienced the
        * specified arbitration delay to be eligible for being granted access.
        * If all requests can not be granted, it attempts to schedule a new 
        * arbitration event.
        *
        * @param cycle The clock cycle the method was called.
        */
        void arbitrate(Tick time);
        
        void retriveRequest(int fromInterface);
        
        
        /**
        * This method tries to deliver as many requests as possible to its
        * destination. Only, requests that have experienced the defined delay
        * can be delivered. However, if an L2 bank blocks, all requests that
        * are old enough might not be delivered. Since the delivery queue
        * is kept sorted, the oldest requests are delivered first.
        *
        * Since this class uses a delivery queue, all parameters except
        * cycle are discarded.
        *
        * @param req    Not used, must be NULL.
        * @param cycle  The clock cycle the method is called.
        * @param toID   Not used, must be -1.
        * @param fromID Not used, must be -1.
        */
        void deliver(MemReqPtr& req, Tick cycle, int toID, int fromID);
        
        /**
        * This method returns the number of transmission channels and is used
        * by the InterconnectProfile class. In this crossbar implementation,
        * the number of channels is the number of interfaces plus the shared 
        * bus.
        *
        * @return The number of transmission channels
        *
        * @see InterconnectProfile
        */
        int getChannelCount(){
            //one channel for all cpus, all banks and one coherence bus
            fatal("no");
            return 0;
        }
        
        /**
        * This method returns the number of cycles the different channels was
        * occupied since it was called last.
        *
        * @return The number of clock cycles each channel was used since last
        *         time the method was called.
        *
        * @see InterconnectProfile
        */
        std::vector<int> getChannelSample();
        
        /**
        * This method writes a description of the different channels to
        * the provided stream.
        *
        * @param stream The output stream to write to.
        *
        * @see InterconnectProfile
        */
        void writeChannelDecriptor(std::ofstream &stream);
        
        virtual std::vector<std::vector<int> > retrieveInterferenceStats();
        
        virtual void resetInterferenceStats();
        
        virtual void clearBlocked(int fromInterface);
        
        void setBlockedLocal(int fromCPUId);
        void clearBlockedLocal(int fromCPUId);
};

class CrossbarRetrieveReqEvent : public Event
{
    
    public:
        
        Crossbar* cb;
        int fromID;
        
        CrossbarRetrieveReqEvent(Crossbar* _cb, int _fid)
            : Event(&mainEventQueue)
        {
            cb = _cb;
            fromID = _fid;
        }

        void process(){
            cb->retriveRequest(fromID);
            delete this;
        }

        const char *description(){
            return "Crossbar retrive event\n";
        }
};

class CrossbarArbitrationEvent : public Event
{
    
    public:
        
        Crossbar* cb;
        
        CrossbarArbitrationEvent(Crossbar* _cb)
            : Event(&mainEventQueue, Memory_Controller_Pri)
        {
            cb = _cb;
        }

        void process(){
            cb->arbitrate(curTick);
        }
        

        const char *description(){
            return "Crossbar deliver event\n";
        }
};

class CrossbarDeliverEvent : public Event
{
    
    public:
        
        Crossbar* cb;
        MemReqPtr req;
        bool toSlave;
        
        CrossbarDeliverEvent(Crossbar* _cb, MemReqPtr& _req, bool _toSlave)
            : Event(&mainEventQueue)
        {
            cb = _cb;
            req = _req;
            toSlave = _toSlave;
        }

        void process(){
            if(toSlave) cb->deliver(req, curTick, req->toInterfaceID, req->fromInterfaceID);
            else cb->deliver(req, curTick, req->fromInterfaceID, req->toInterfaceID);
            delete this;
        }

        const char *description(){
            return "Crossbar arbitration event\n";
        }
};

#endif // __CROSSBAR_HH__
