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

#include "mem/mem_req.hh"
#include "mem/base_hier.hh"
#include "base/statistics.hh"
#include "base/range.hh"
#include "sim/eventq.hh"

#include "mem/bus/bus_interface.hh"

#include "mem/cache/miss/adaptive_mha.hh"

/** The maximum value of type Tick. */
#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)

// #define DO_BUS_TRACE 1

// Forward definition of events
class AddrArbiterEvent;
class DataArbiterEvent;
class ForwardEvent;
class DeliverEvent;

template <class BusType> class BusInterface;
class AdaptiveMHA;

/**
 * A arbitrated split transaction bus model.
 */
class Bus : public BaseHier
{
  public:
    /** Width of the bus in bytes. */
    int width;
    /** Clock rate (clock period in ticks). */
    int clockRate;
    
  protected:
    // statistics
    /** Total number of cycles the address portion of this bus is idle. */
    Stats::Scalar<> addrIdleCycles;
    /** Fraction of total time the address bus was blocked. */
    Stats::Formula addrIdleFraction;

    /** Total number of cycles of the queueing delay on the address bus. */
    Stats::Vector<> addrQdly;
    /** Total number of requests for the address bus. */
    Stats::Vector<> addrRequests;
    /** The average queueing delay of the address bus. */
    Stats::Formula addrQueued;

    /** Total number of cycles the data portion of this bus is idle. */
    Stats::Scalar<> dataIdleCycles;
    /** Fraction of total time the data bus was blocked. */
    Stats::Formula dataIdleFraction;
    
    Stats::Formula dataUseCycles;

    /** Total number of cycles of the queueing delay on the data bus. */
    Stats::Vector<> dataQdly;
    /** Total number of requests for the data bus. */
    Stats::Vector<> dataRequests;
    /** The average queueing delay of the data bus. */
    Stats::Formula dataQueued;

    /** The number of times this bus blocked. */
    Stats::Scalar<> busBlocked;
    /** The total number of cycles this bus is blocked. */
    Stats::Scalar<> busBlockedCycles;
    /** The fraction of total time that the bus was blocked. */
    Stats::Formula busBlockedFraction;
    
    Stats::Scalar<> writebackCycles;
    Stats::Formula writebackFraction;
    
    Stats::Scalar<> unknownSenderCycles;
    Stats::Formula unknownSenderFraction;
    

    /** The number of times the address bus was granted in error. */
    Stats::Scalar<> nullGrants;

    /** The last cycle the data arbiter was run, used for debugging. */
    Tick runDataLast;
    /** The last cycle the address arbiter was run, used for debugging. */
    Tick runAddrLast;
    
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
        int _busCPUCount,
        int _busBankCount);

    /** Frees locally allocated memory. */
    ~Bus();

    /** Register bus related statistics. */
    void regStats();

    /** Reset bus statistics-related state. */
    void resetStats();

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
    virtual void arbitrateAddrBus();

    /**
     * Decide which outstanding request to service.
     * Also reschedules the arbiter event if needed.
     */
    virtual void arbitrateDataBus();

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
    virtual void clearBlocked(int id);

    /**
     * Announces to the bus the bus interface is now blocked.
     * @param id The bus interface id that just unblocked.
     */
    virtual void setBlocked(int id);

    /**
     * Probe the attached interfaces for the given request.
     * @param req The probe request.
     * @param update Update the hierarchy.
     * @return The estimated completion time.
     */
    Tick probe(MemReqPtr &req, bool update);

    /**
     * Collect the ranges from the attached interfaces into the provide list.
     */
    void collectRanges(std::list<Range<Addr> > &range_list);

    /**
     * Notify all the attached interfaces that there has been a range change.
     */
    void rangeChange();
    
    // Adaptive MHA methods
    double getAddressBusUtilisation(Tick sampleSize);
    std::vector<int> getAddressUsePerCPUId();
    double getDataBusUtilisation(Tick sampleSize);
    std::vector<int> getDataUsePerCPUId();
    void resetAdaptiveStats();
    

  protected:
    
    int busCPUCount;
    int busBankCount;
      
    std::vector<int> perCPUAddressBusUse;
    std::vector<int> perCPUAddressBusUseOverflow;
    int addrBusUseSamples[2];

    std::vector<int> perCPUDataBusUse;
    std::vector<int> perCPUDataBusUseOverflow;
    int dataBusUseSamples[2];
    
    int adaptiveSampleSize;
    
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
        
        Tick startTag;
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

    /**
     * A vector containing only masters (for convenience)
     * Hack by Magnus
     */
    std::vector<BusInterface<Bus> *> masterInterfaces;
    
    std::map<int, int> masterIndexToInterfaceIndex;
    std::map<int, int> interfaceIndexToMasterIndex;
    std::map<int, int> slaveIndexToInterfaceIndex;
    
    /**
     * A vector containing only slave (for convenience)
     * Hack by Magnus
     */
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
    virtual void scheduleArbitrationEvent(Event * arbiterEvent, Tick reqTime,
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
    
    void storeUseStats(bool data, int senderID);
    
#ifdef DO_BUS_TRACE
    void writeTraceFileLine(Addr address, std::string message);
#endif
};

/**
 * Simple Event to schedule the address arbiter.
 */
class AddrArbiterEvent : public Event
{
    // event data fields
    /** The bus to run the arbiter on. */
    Bus *bus;

  public:
    // constructor
    /** Simple Constructor */
    AddrArbiterEvent(Bus *_bus)
	: Event(&mainEventQueue), bus(_bus) {
    }

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


#endif // __BUS_HH__
