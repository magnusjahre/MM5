
#ifndef __INTERCONNECT_HH__
#define __INTERCONNECT_HH__

#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
        
#include "mem/base_hier.hh"
#include "interconnect_interface.hh"
#include "interconnect_profile.hh"
#include "sim/eventq.hh"
#include "sim/stats.hh"

#include "mem/cache/miss/adaptive_mha.hh"
        
#include "cpu/exec_context.hh" // for ExecContext, needed for cpu_id
#include "cpu/base.hh" // for BaseCPU, needed for cpu_id


/** The maximum value of type Tick. */
#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)

class InterconnectInterface;
class InterconnectArbitrationEvent;
class InterconnectDeliverQueueEvent;
class InterconnectProfile;

/**
* This class is the parent class of all interconnect extensions. In other 
* words, all interconnects are sub-classes of this class.
*
* It has three funcions:
* - Firstly, it defines the interface which all interconnects must 
*   implement
* - Secondly, it takes care of registration and administration of 
*   interconnect interfaces. This functionality is common to all interconnects.
* - In addition, defines some classes that the interconnects need. These
*   classes are the event objects used to create delays and a two convenience
*   classes that represent requests and deliveries.
*
* @author Magnus Jahre
*/
class Interconnect : public BaseHier
{
    private:
        
        int masterInterfaceCount;
        int slaveInterfaceCount;
        int totalInterfaceCount;
        
    protected:
        
        bool blocked;
        int waitingFor;
        Tick blockedAt;
        
        int cpu_count;
        
        InterconnectProfile* profiler;
        
        AdaptiveMHA* adaptiveMHA;
        
        std::vector<std::list<int> > processorIDToInterconnectIDs;
        std::map<int, int> interconnectIDToProcessorIDMap;
        std::map<int, int> interconnectIDToL2IDMap;
        std::map<int, int> L2IDMapToInterconnectID;
        
        class InterconnectRequest;
        class InterconnectDelivery;
        
        std::list<InterconnectRequest* > requestQueue;
        std::list<InterconnectDelivery* > grantQueue;
        std::vector<bool> blockedInterfaces;
        
        std::vector<InterconnectInterface* > masterInterfaces;
        std::vector<InterconnectInterface* > slaveInterfaces;
        std::vector<InterconnectInterface* > allInterfaces;
  
        /* Statistics variables */
        Stats::Scalar<> totalArbitrationCycles;
        Stats::Scalar<> totalArbQueueCycles;
        Stats::Formula avgArbCyclesPerRequest;
        Stats::Formula avgArbQueueCyclesPerRequest;
        
        Stats::Scalar<> totalTransferCycles;
        Stats::Scalar<> totalTransQueueCycles;
        Stats::Formula avgTransCyclesPerRequest;
        Stats::Formula avgTransQueueCyclesPerRequest;
        
        Stats::Vector<> perCpuTotalTransferCycles;
        Stats::Vector<> perCpuTotalTransQueueCycles;
        
        Stats::Vector<> cpuInterferenceCycles;
        
        Stats::Formula avgTotalDelayCyclesPerRequest;
        
        Stats::Scalar<> requests;
        Stats::Scalar<> masterRequests;
        Stats::Scalar<> arbitratedRequests;
        Stats::Scalar<> sentRequests;
        Stats::Scalar<> nullRequests;
//         Stats::Scalar<> duplicateRequests;
        Stats::Scalar<> numClearBlocked;
        Stats::Scalar<> numSetBlocked;
        
        /**
        * Convenience class that represents a transfer request.
        */
        class InterconnectRequest{
            public:
                Tick time;
                int fromID;
                
                Tick virtualStartTime;
                int proc;
            
                /**
                * Default constructor
                *
                * @param _time   The tick the request was issued
                * @param _fromID The ID of the requesting interface
                */
                InterconnectRequest(Tick _time, int _fromID){
                    time = _time;
                    fromID = _fromID;
                    
                    virtualStartTime = -1;
                    proc = -1;
                }
        };
        
        /**
         * Convenience class that represents a granted request which is in the
         * process of being delivered.
         */
        class InterconnectDelivery{
            public:
                Tick grantTime;
                int fromID;
                int toID;
                MemReqPtr req;
            
                
                /**
                 * Default constructor
                 *
                 * @param _grantTime The tick the request was granted access 
                 *                   and the delivery object created
                 * @param _fromID    The ID of the requesting interface
                 * @param _toID      The ID of the destination interface
                 * @param _req       The MemReqPtr that will be delivered
                 */
                InterconnectDelivery(Tick _grantTime,
                                     int _fromID,
                                     int _toID,
                                     MemReqPtr& _req)
                {
                    grantTime = _grantTime;
                    fromID = _fromID;
                    toID = _toID;
                    req = _req;
                }
        };

    public:
        int clock;
        int width;
        int transferDelay;
        int arbitrationDelay;
        
        std::vector<InterconnectArbitrationEvent *> arbitrationEvents;
        std::vector<InterconnectDeliverQueueEvent* > deliverEvents;
        
    protected:
        
        /**
        * Checks that a list of InterconnectRequest objects is sorted
        * in ascending order according to their request times. This is
        * important because the arbitration methods usually assume that the 
        * request list is sorted. It is used in assertions in the subclasses.
        *
        * @param inList The list to check
        *
        * @return True if the list is sorted
        *
        * @see InterconnectRequest
        */
        bool isSorted(std::list<InterconnectRequest*>* inList);
        
        /**
        * Checks that a list of InterconnectDelivery objects is sorted
        * in ascending order according to their grant times. This is
        * important because the arbitration methods usually assume that the 
        * request list is sorted. It is used in assertions in the subclasses.
        *
        * @param inList The list to check
        *
        * @return True if the list is sorted
        *
        * @see InterconnectDelivery
        */
        bool isSorted(std::list<InterconnectDelivery*>* inList);
        
        
    public:
        
        /**
        * This is the default constructor for the Interconnect class. It stores
        * the arguments and initialises some member variables and does some
        * input checking.
        *
        * The interconnect only supports running at the same frequency as the
        * processor core, and there must be at least one CPU in the system.
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
        */
        Interconnect(const std::string &_name, 
                 int _width, 
                 int _clock,
                 int _transDelay,
                 int _arbDelay,
                 int _cpu_count,
                 HierParams *_hier,
                 AdaptiveMHA* _adaptiveMHA);
        
        ~Interconnect(){ /* does nothing */ }
        
        /**
        * This method registers a InterconnectProfiler. The profiler is used to
        * dump selected statistics to a file at regular time intervals.
        *
        * @param _profiler The InterconnectProfiler to use
        */
        void registerProfiler(InterconnectProfile* _profiler){
            profiler = _profiler;
        }
        
        /**
        * This method is called from the M5 statistics package and initialises
        * the statistics variables used in all interconnects.
        */
        void regStats();

        /**
        * This method is supposed to reset the statistics values. However, it
        * is not used any set-up used in this work and is not implemented.
        */
        void resetStats();
        
        /**
        * An InterconnectInterface must register with the interconnect to be 
        * able to use it. This function is handled by this method.
        *
        * @param interface   A pointer to the interconnect interface that is
        *                    registering itself
        * @param isL2        Is true if the interface represents an L2 cache, 
        *                    false otherwise
        * @param processorID The ID of the processor connected to the 
        *                    interface. If the cache is not connected to any 
        *                    particular processor, -1 should be supplied.
        *
        * @return The ID the interface is given
        */
        int registerInterface(InterconnectInterface* interface,
                              bool isL2,
                              int processorID);
        
        /**
        * This method makes all registers interconnects reevaluate which 
        * address ranges they are responsible for.
        */
        void rangeChange();
        
        /**
        * The cache might issue requests that are later squashed. The 
        * interfaces might detect this situation when they try to retrieve the
        * current request from the cache. This situation needs to be measured
        * as it does cause empty issue slots in the interconnect.
        *
        * The InterconnectInterface calls this method when a squashed request
        * was encountered. In increments a statistic variable that is printed
        * when simulation is finished.
        */
        void incNullRequests();
        
//         void incDuplicateRequests();
        
        /**
        * To enable transfers between caches at the same level, a means of
        * translating from interconnect IDs to processor IDs is needed. 
        * This information is stored in a map when the interface registers
        * itself and is retrieved through this method.
        *
        * @param processorID The processor ID to translate
        *
        * @return The interconnect ID of the data cache belonging to this
        *         processor
        */
        int getInterconnectID(int processorID);
        
        /**
        * This method provides statistic values to the InterconnectProfile 
        * object. The values are stored in the memory pointed to by the 
        * arguments, and the internal counters are reset.
        *
        * @param dataSends      Pointer to a memory area where the number of
        *                       data sends can be stored
        * @param instSends      Pointer to a memory area where the number of
        *                       instruction sends can be stored
        * @param coherenceSends Pointer to a memory area where the number of
        *                       coherence sends can be stored
        * @param totalSends     Total number of sends which is the sum of the
        *                       other request types 
        *
        * @see InterconnectProfile
        */
        void getSendSample(int* dataSends,
                           int* instSends,
                           int* coherenceSends,
                           int* totalSends);
        
        /**
        * Convenience method that finds the ID of the slave interface that is
        * responsible answering requests related to a given address.
        *
        * @param address The address in question
        *
        * @return The interface ID of the interface responsible for the address
        */
        int getTarget(Addr address);
        
        int getDestinationId(int fromID);
        
        Addr getDestinationAddr(int fromID);
        
        MemCmd getCurrentCommand(int fromID);
        
        /**
        * This method puts the request into a queue and schedules an 
        * arbitration event if needed. The request queue is kept sorted in 
        * ascending order on the clock cycle it was recieved as this simplifies
        * the arbitration method.
        *
        * @param time   The clock cycle the method was called
        * @param fromID The interface ID of the requesting interface
        */
        virtual void request(Tick time, int fromID);

        /**
         * This method is commented in the subclasses where it is implemented
         */
        virtual void send(MemReqPtr& req, Tick time, int fromID) = 0;
        
        /**
         * This method is commented in the subclasses where it is implemented
         */
        virtual void arbitrate(Tick cycle) = 0;
        
        /**
         * This method is commented in the subclasses where it is implemented
         */
        virtual void deliver(MemReqPtr& req, 
                             Tick cycle, 
                             int toID, 
                             int fromID) = 0;
        
        /**
         * This method is commented in the subclasses where it is implemented
         */
        virtual void setBlocked(int fromInterface);
        
        /**
         * This method is commented in the subclasses where it is implemented
         */
        virtual void clearBlocked(int fromInterface);
        
        /**
         * This method is commented in the subclasses where it is implemented
         */
        virtual int getChannelCount() = 0;
        
        /**
         * This method is commented in the subclasses where it is implemented
         */
        virtual std::vector<int> getChannelSample() = 0;
        
        /**
         * This method is commented in the subclasses where it is implemented
         */
        virtual void writeChannelDecriptor(std::ofstream &stream) = 0;
        
        virtual void scheduleArbitrationEvent(Tick candidateTime);
        
        void scheduleDeliveryQueueEvent(Tick candidateTime);
        
        virtual std::vector<std::vector<int> > retrieveInterferenceStats(){
            fatal("retrieveInterferenceStats() called on interconnect which does not support it");
        }
        
        virtual void resetInterferenceStats(){
            fatal("resetInterferenceStats() called on interconnect which does not support it");
        }
};

/**
* This class creates an arbitation event that is compatible with the M5 event 
* queue. It is used by the Interconnect classes to create a time delay from a 
* request is recieved untill the arbitration is carried out.
*
* @see Interconnect
* @see SplitTransBus
* @see Crossbar
* @see Butterfly
* @see IdealInterconnect
*
* @author Magnus Jahre
*/
class InterconnectArbitrationEvent : public Event
{

    public:
        Interconnect *interconnect;
        
        /**
        * Default constructor.
        *
        * @param _interconnect A pointer to the associated interconnect
        */
        InterconnectArbitrationEvent(Interconnect *_interconnect)
            : Event(&mainEventQueue), interconnect(_interconnect)
        {
        }
        
        /**
        * This method is called when the event is serviced. It searches through
        * the Interconnects arbitration tick queue to find the current clock 
        * tick, removes this and calls the arbitrate method in Interconnect.
        * Then, it deletes itself.
        *
        * @see Interconnect
        */
        void process();

        /**
        * @return A textual description of the event
        */
        virtual const char *description();
};

/**
 * This class creates a deliver event that is compatible with the M5 event 
 * queue. It is used by the Interconnect classes to create a time delay from a 
 * request is granted access untill it is delivered.
 *
 * This event is for use in interconnects that do _not_ use delivery queue.
 * Such classes should use InterconnectDeliverQueueEvent instead.
 *
 * @see InterconnectDeliverQueueEvent
 * @see Interconnect
 * @see SplitTransBus
 * @see Crossbar
 * @see Butterfly
 * @see IdealInterconnect
 *
 * @author Magnus Jahre
 */
class InterconnectDeliverEvent : public Event
{
    
    public:
        
        Interconnect *interconnect;
        MemReqPtr req;
        int toID;
        int fromID;
        
        /**
        * Constructs a delivery event for interconnects that do not use a
        * delivery queue.
        *
        * @param _interconnect A pointer to the interconnect that created the 
                               event
        * @param _req          The request to deliver
        * @param _toID         The interface ID the request will be delivered to
        * @param _fromID       The interface ID the request was sent from
        */
        InterconnectDeliverEvent(Interconnect *_interconnect,
                                 MemReqPtr& _req,
                                 int _toID,
                                 int _fromID)
            : Event(&mainEventQueue)
        {
            interconnect = _interconnect;
            req = _req;
            toID = _toID;
            fromID = _fromID;
        }

        /**
        * This method is called when the event is serviced and calls the 
        * deliver method in an Interconnect class. Afterwards, it deletes 
        * itself.
        *
        * @see Interconnect
        */
        void process();

        /**
         * @return A textual description of the event
         */
        virtual const char *description();
};


/**
 * This class creates a deliver event that is compatible with the M5 event 
 * queue. It is used by the Interconnect classes to create a time delay from a 
 * request is granted access untill it is delivered.
 *
 * This event is for use in interconnects that do _not_ use delivery queue.
 * Such classes should use InterconnectDeliverEvent instead.
 *
 * @see InterconnectDeliverEvent
 * @see Interconnect
 * @see SplitTransBus
 * @see Crossbar
 * @see Butterfly
 * @see IdealInterconnect
 *
 * @author Magnus Jahre
 */
class InterconnectDeliverQueueEvent : public Event
{
    
    public:
        
        Interconnect *interconnect;
        
        /**
         * Constructs a delivery event for interconnects that uses a delivery 
         * queue.
         *
         * @param _interconnect A pointer to the interconnect that created the 
         *                      event
         */
        InterconnectDeliverQueueEvent(Interconnect* _interconnect)
            : Event(&mainEventQueue) {
            interconnect = _interconnect;
    }

    /**
     * This method is called when the event is serviced. First, it removes 
     * itself from the delivery queue. Then it calls the deliver method in an
     * Interconnect subclass.
     *
     * Only the tick argument to deliver is provided when this method is 
     * serviced. The memory request provided is NULL and the from and to IDs
     * are set to -1.
     *
     * @see Interconnect
    */
    void process(){
        bool found = false;
        int foundIndex = -1;
        for(int i=0;i<interconnect->deliverEvents.size();i++){
            if((InterconnectDeliverQueueEvent*) interconnect->deliverEvents[i] 
                == this){
                foundIndex = i;
                found = true;
            }
        }
        assert(found);
        interconnect->deliverEvents.erase(
                interconnect->deliverEvents.begin()+foundIndex);
            
        MemReqPtr noReq = NULL;
        interconnect->deliver(noReq, this->when(), -1, -1);
        delete this;
    }
    
    /**
     * @return A textual description of the event
     */
    virtual const char *description(){
        return "InterconnectDeliverQueueEvent";
    }
};

#endif // __INTERCONNECT_HH__
