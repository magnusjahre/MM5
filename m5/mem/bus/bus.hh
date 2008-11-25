/*
 * Copyright (c) 2002, 2003, 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

/**
 * @file
 * Describes the Bus API.
 */

#ifndef __BUS_HH__
#define __BUS_HH__

#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <fstream>

#include "mem/mem_req.hh"
#include "mem/base_hier.hh"
#include "base/statistics.hh"
#include "base/range.hh"
#include "sim/eventq.hh"

#include "mem/cache/miss/adaptive_mha.hh"
#include "mem/bus/controller/memory_controller.hh"

#include "base/compression/null_compression.hh"
#include "mem/timing/simple_mem_bank.hh"

#include "mem/bus/slave_interface.hh"

//NOTE: these should _never_ be defined when running experiments!
// #define DO_BUS_TRACE 1
// #define INJECT_TEST_REQUESTS 1

// Forward definition of events
class AddrArbiterEvent;
class DataArbiterEvent;
class ForwardEvent;
class DeliverEvent;
class MemoryControllerEvent;

template <class BusType> class BusInterface;
class AdaptiveMHA;
class TimingMemoryController;

using namespace std;

typedef enum{
    BUS_INTERFERENCE,
    BUS_BLOCKING_INTERFERENCE,
    BUS_PRIVATE_BLOCKING_INTERFERENCE,
    CONFLICT_INTERFERENCE,
    HIT_TO_MISS_INTERFERENCE
} interference_type;

/**
 * A arbitrated split transaction bus model.
 */
class Bus : public BaseHier
{
  private:
    TimingMemoryController *fwMemoryController;
    TimingMemoryController *simMemoryController;
    Tick detailedSimulationStart;
    int bank_count;
    
    std::vector<std::vector<int> > busInterference;
    std::vector<std::vector<int> > conflictInterference;
    std::vector<std::vector<int> > hitToMissInterference;
    
    std::vector<int> outstandingReads;
    std::vector<int> outstandingWrites;
    
    bool infiniteBW;
    
  public:
    /** Width of the bus in bytes. */
    int width;
    /** Clock rate (clock period in ticks). */
    int clockRate;

    Tick nextfree;
    Tick lastarbevent;

    int livearbs;

    std::list<AddrArbiterEvent*> arb_events;

    bool arbiter_scheduled_flag; 
 
    bool need_to_sort;

    AddrArbiterEvent* currently_scheduled;

    TimingMemoryController *memoryController;
    
    AdaptiveMHA* adaptiveMHA;

  protected:
    
    // statistics
    Stats::Scalar<> busUseCycles;
    Stats::Formula busUtilization;
    
    Stats::Scalar<> unknownSenderCycles;
    Stats::Formula unknownSenderUtilization;
    
    Stats::Scalar<> unknownSenderRequests;
    Stats::Formula unknownSenderFraction;

    Stats::Scalar<> totalQueueCycles;
    Stats::Scalar<> totalRequests;
    Stats::Formula avgQueueCycles;
    
    Stats::Vector<> accessesPerCPU;
    Stats::Vector<> pageHitsPerCPU;
    Stats::Scalar<> noCPUrequests;
    
    Stats::Scalar<> blockedCycles;
    
    Stats::Scalar<> nullGrants;
    
    Stats::Scalar<> interferenceRemovedHits;
    Stats::Scalar<> constructiveInterferenceHits;
    
    Stats::Vector<> cpuInterferenceCycles;
    Stats::Vector<> cpuConflictInterferenceCycles;
    Stats::Vector<> cpuHtMInterferenceCycles;
    
    Stats::Vector<> blockingInterferenceCycles;
    
    Stats::Vector<> shadowCtrlPageHits;
    Stats::Vector<> shadowCtrlAccesses;
    Stats::Vector<> shadowUseCycles;
    
    Stats::Vector<> shadowBlockedCycles;
    
    Stats::Vector<> perCPUTotalEntryDelay;
    Stats::Vector<> perCPUTotalEntryRequests;
    Stats::Formula perCPUAvgEntryDelay;

    /** The last cycle the data arbiter was run, used for debugging. */
    Tick runDataLast;
    /** The last cycle the address arbiter was run, used for debugging. */
    Tick runAddrLast;
    
    Stats::Vector<> predictedServiceLatencySum;
    Stats::Vector<> numServiceLatencyRequests;
    Stats::Formula avgPredictedServiceLatency;
    
    Stats::Scalar<> actualServiceLatencySum;
    Stats::Scalar<> actualServiceLatencyRequests;
    Stats::Formula avgActualServiceLatency;
    
    Stats::Vector<> estimatedPrivateQueueLatency;
    Stats::Vector<> estimatedPrivateQueueRequests;
    Stats::Formula avgEstimatedPrivateQueueLatency;
    
    Stats::Scalar<> actualQueueDelaySum;
    Stats::Scalar<> actualQueueDelayRequests;
    Stats::Formula avgActualQueueDelay;
    
    /** Added by magnus **/
//     Stats::Scalar<> freeCycles;


  public:
    // constructor
    /** Constructs a Bus object. */
    Bus(const std::string &name,
        HierParams *hier_params,
        int width,
        int clockRate,
        AdaptiveMHA* _adaptiveMHA,
        int cpu_count,
        int bank_count,
        Tick _switch_at,
        TimingMemoryController* _fwController,
        TimingMemoryController* _memoryController,
        bool _infiniteBW);

    /** Frees locally allocated memory. */
    ~Bus();

    /** Register bus related statistics. */
    void regStats();

    /** Reset bus statistics-related state. */
    void resetStats();

    int getBankCount(){
        return bank_count;
    }
    
    /**
     * Mark the interface as needing the address bus.
     * @param id The id of the BusInterface making the request.
     * @param time The time that the request is made.
     */
    void requestAddrBus(int id, Tick time);

    /**
     * Mark the interface as needing the data bus.
     * @param id The id of the BusInterface making the request.
     * @param time The time that the request is made.
     */
    void requestDataBus(int id, Tick time);

    /**
     * Decide which outstanding request to service.
     * Also reschedules the arbiter event if needed.
     */
    void arbitrateAddrBus(int interfaceid, Tick requestedAt);

    /**
     * Decide which outstanding request to service.
     * Also reschedules the arbiter event if needed.
     */
    void arbitrateDataBus();

    /**
     * Sends the request to the attached interfaces via the address bus.
     * First sends the request to the master interfaces so they can snoop,
     * then sends it to the responsible slave interface.
     * @param req A pointer to the request to service.
     * @param origReqTime time used to calculate queueing delay.
     * @return True if the req was sent successfully.
     */
    bool sendAddr(MemReqPtr &req, Tick origReqTime);

    /**
     * Sends the repsonse to the requesting interface via the data bus.
     * @param req The request we are responding to.
     * @param origReqTime time used to calculate queueing delay.
     */
    void sendData(MemReqPtr &req, Tick origReqTime);

    /**
     * Occupy the data for the number of cycles to send a number of bytes.
     * @param size The number of bytes to send.
     */
    void delayData(int size, int senderID = -1, MemCmdEnum cmd = InvalidCmd);

    /**
     * Sends an ACK to the requesting interface via the address bus.
     * @param req The request that is being ACKed.
     * @param origReqTime time used to calculate queueing delay.
     */
    void sendAck(MemReqPtr &req, Tick origReqTime);

    /**
     * Registers the interface and returns its ID.
     * @param bi The bus interface to register.
     * @param master True if the BusInterface is a master.
     *
     * @retval int The bus assigned ID.
     */
    int registerInterface(BusInterface<Bus> *bi, bool master = false);

    /**
     * Announces to the bus the bus interface is now unblocked.
     * @param id The bus interface id that just unblocked.
     */
    void clearBlocked(int id);

    /**
     * Announces to the bus the bus interface is now blocked.
     * @param id The bus interface id that just unblocked.
     */
    void setBlocked(int id);

    /**
     * Probe the attached interfaces for the given request.
     * @param req The probe request.
     * @param update Update the hierarchy.
     * @return The estimated completion time.
     */
    Tick probe(MemReqPtr &req, bool update);

    void addPrewrite(MemReqPtr &req);
    /**
     * Collect the ranges from the attached interfaces into the provide list.
     */
    void collectRanges(std::list<Range<Addr> > &range_list);

    /**
     * Notify all the attached interfaces that there has been a range change.
     */
    void rangeChange();
    
    void incrementBlockedCycles(Tick cycles);

    void handleMemoryController(bool isShadow, int ctrlID);

    void latencyCalculated(MemReqPtr &req, Tick time, bool fromShadow);
    
    void switchMemoryController();
    
    void updatePerCPUAccessStats(int cpuID, bool pageHit);
    
    void incConstructiveInterference();
    
    void incInterferenceMisses();
    
    // Adaptive MHA methods
    double getAverageQueue(Tick sampleSize);
    std::vector<double> getAverageQueuePerCPU();
    double getDataBusUtilisation(Tick sampleSize);
    std::vector<int> getDataUsePerCPUId();
    void resetAdaptiveStats();
    
    
    std::vector<std::vector<int> > retrieveBusInterferenceStats();
    void resetBusInterferenceStats();
    std::vector<std::vector<int> > retrieveConflictInterferenceStats();
    void resetConflictInterferenceStats();
    std::vector<std::vector<int> > retrieveHitToMissInterferenceStats();
    void resetHitToMissInterferenceStats();
    void addInterference(int victimID, int interfererID, interference_type iType);
    void addInterferenceCycles(int victimID, Tick delay, interference_type iType);

  private:
    std::vector<int> perCPUDataBusUse;
    std::vector<int> perCPUQueueCycles;
    std::vector<int> perCPURequests;
    int adaptiveSampleSize;
    
    int cpu_count;
    
#ifdef INJECT_TEST_REQUESTS
    std::list<MemReqPtr> testRequests;
#endif
    
    /** The next curTick that the address bus is free. */
    Tick nextAddrFree;
    /** The next curTick that the data bus is free. */
    Tick nextDataFree;

    /** The cycle that the bus blocked. */
    Tick busBlockedTime;

    /** is the bus blocked? */
    bool blocked;

    /** is the reason we blocked a Syncronus block, which req caused it. */
    bool blockSync;


    /** Returns the blocekd status */
    bool isBlocked() const
    {
	return(blocked);
    }

    /** The Id of the blocked interface that the bus is waiting for. */
    int waitingFor;
    /** The blocked request. */
    MemReqPtr blockedReq;

    /** Event used to run the adress arbiter at the proper time. */
    AddrArbiterEvent *addrArbiterEvent;
    /** Event used to run the data arbiter at the proper time. */
    DataArbiterEvent *dataArbiterEvent;

    /** Event used to control the memory controller */
    MemoryControllerEvent *memoryControllerEvent;

    /** The number of interfaces attached to this bus. */
    int numInterfaces;

    /**
     * A simple class to hold the bus requests of bus interfaces.
     */
    class BusRequestRecord
    {
      public:
	/** Set if the bus is requested. */
	bool requested;
	/** The time when the bus was requested. */
	Tick requestTime;
    };

    /** A list of the connected interfaces, accessed by bus id. */
    std::vector<BusInterface<Bus> *> interfaces;
    /** A list of the address bus requests, accessed by bus id. */
    std::vector<BusRequestRecord> addrBusRequests;
    /** A list of the data bus requests, accessed by bus id. */
    std::vector<BusRequestRecord> dataBusRequests;

    /**
     * An ordered list of master and slave interfaces for transmitting.
     * All masters are checked before slaves for coherence.
     */
    std::vector<BusInterface<Bus> *> transmitInterfaces;

    std::vector<BusInterface<Bus> *> slaveInterfaces;

    /**
     * Find the oldest and next to oldest outstanding requests
     * @param requests The list of requests to process.
     * @param grant_id Reference param of request to grant
     * @param old_grant_id Reference param of next request to grant
     *
     * @retval bool true if valid request is found.
     */
    bool findOldestRequest(std::vector<BusRequestRecord> & requests,
			   int & grant_id, int & old_grant_id);

    /**
     * Schedule an arbiter for the correct time.
     * @param arbiterEvent the event to schedule.
     * @param reqTime The time of the request.
     * @param nextFreeCycle the time of the next bus free cycle
     * @param idleAdvance the number of bus cycle to skip when it is idle.
     */
    void scheduleArbitrationEvent(Event * arbiterEvent, Tick reqTime,
				  Tick nextFreeCycle, Tick idleAdvance = 1);

    /**
     * Find the global simulation time (curTick) corresponding to
     * the *next* bus clock edge.  Optional second argument 'n' finds
     * the 'n'th next bus block edge.
     * @param globalCycle The cycle to advance from.
     * @param nthNextClock The number of bus cycles to advance.
     *
     * @retval Tick The cycle of the next bus clock edge.
     */
    Tick nextBusClock(Tick globalCycle, int nthNextClock = 1)
    {
	// Find the bus clock cycle number corresponding the provided
	// global cycle number.  This is the bus cycle that started
	// *on or before* the given global cycle.
	Tick busCycle = globalCycle / clockRate;

	// Advance to desired bus cycle.  Note that if the global
	// cycle is right at a bus clock edge, the default behavior
	// (nthNextClock == 1) will advance to the *next* edge.  This
	// is usually what we want, to model something happening on
	// the *next* bus clock edge.
	busCycle += nthNextClock;

	// Convert back to global cycles & return.
	return busCycle * clockRate;
    }
    
//     void buildShadowControllers(int np, HierParams* hp);
    
#ifdef INJECT_TEST_REQUESTS
    void generateRequests();
#endif

    
#ifdef DO_BUS_TRACE
    void writeTraceFileLine(Addr address, int bank, Addr page, Tick latency, MemCmd cmd, std::string message);
#endif
};

/**
 * Simple Event to schedule the address arbiter.
 */
class AddrArbiterEvent : public Event
{
  public:
    // event data fields
    /** The bus to run the arbiter on. */
    Bus *bus;

    int interfaceid;

    Tick original_time;
    
    int arbitrationLoopCounter;

  public:
    // constructor
    /** Simple Constructor */
    AddrArbiterEvent(Bus *_bus, int _interfaceid, Tick _originaltime)
	: Event(&mainEventQueue), bus(_bus) {
        interfaceid = _interfaceid;
        original_time = _originaltime;
        arbitrationLoopCounter = 0;
    }

    class event_compare :
    public std::binary_function<AddrArbiterEvent *, AddrArbiterEvent*, bool>
    {
      public:
       bool operator()(const AddrArbiterEvent *l, const AddrArbiterEvent *r) const {
         return (l->original_time < r->original_time);
       }
    };


    /** Calls Bus::arbiterAddr(). */
    void process();

    /**
     * Returns the string description of this event.
     * @return The description of this event.
     */
    virtual const char *description();
};

/**
 * Simple Event to schedule the data arbiter.
 */
class DataArbiterEvent : public Event
{
    // event data fields
    /** The bus to run the arbiter on. */
    Bus *bus;

  public:
    // constructor
    /** A simple constructor. */
    DataArbiterEvent(Bus *_bus)
	: Event(&mainEventQueue), bus(_bus)
    {
    }

    // event execution function
    /** Calls Bus::arbiterData(). */
    void process();

    /**
     * Returns the string description of this event.
     * @return The description of this event.
     */
    virtual const char *description();
};

/**
 * Delivers the response to the bus interface at the proper time.
 */
class DeliverEvent : public Event
{
    // event data fields
    /** The response. */
    MemReqPtr req;
    /** The interface to deliver the response to. */
    BusInterface<Bus> *bi;

  public:
    // constructor
    /** A simple constructor. */
    DeliverEvent(BusInterface<Bus> *_bi, MemReqPtr _req)
	: Event(&mainEventQueue), req(_req), bi(_bi)
    {
    }

    // event execution function
    /** Calls BusInterface::deliver() */
    void process();
    /**
     * Returns the string description of this event.
     * @return The description of this event.
     */
    virtual const char *description();
};

/**
 * Memory controller event
 */
class MemoryControllerEvent : public Event
{
    Bus *bus;
    bool isShadow;
    int controllerID;
    
  public:
    // constructor
    /** A simple constructor. */
    MemoryControllerEvent(Bus *_bus, bool _isShadow, int _controllerID)
	: Event(&mainEventQueue, Memory_Controller_Pri), bus(_bus), isShadow(_isShadow), controllerID(_controllerID)
    {
    }

    // event execution function
    void process();
    /**
     * Returns the string description of this event.
     * @return The description of this event.
     */
    virtual const char *description();
};

/**
 * Memory controller switch
 */
class MemoryControllerSwitchEvent : public Event
{
    Bus *bus;
    
    public:
    // constructor
    /** A simple constructor. */
    MemoryControllerSwitchEvent(Bus *_bus)
        : Event(&mainEventQueue), bus(_bus)
    {
    }

    // event execution function
    /** Calls BusInterface::deliver() */
    void process(){
        bus->switchMemoryController();
        delete this;
    }
    
    /**
    * Returns the string description of this event.
    * @return The description of this event.
     */
    virtual const char *description(){
        return "Memory Controller Switch Event";
    }
};

#endif // __BUS_HH__
