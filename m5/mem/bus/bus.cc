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
 * Definition of the Bus class, DeliverEvent, and arbitration events.
 */

#include <cassert>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include "base/cprintf.hh"
#include "base/intmath.hh"
#include "base/misc.hh"
#include "base/callback.hh"
#include "cpu/smt.hh"
#include "mem/bus/bus.hh"
#include "mem/bus/bus_interface.hh"
#include "sim/builder.hh"
#include "sim/host.hh"
#include "sim/stats.hh"

#include <fstream>

using namespace std;

/** The maximum value of type Tick. */
#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)


// NOTE: all times are stored internally as cpu ticks
//       A conversion is made from provided time to
//       bus cycles

// bus constructor
Bus::Bus(const string &_name,
	 HierParams *hier_params,
	 int _width,
	 int _clock,
   AdaptiveMHA* _adaptiveMHA,
   bool _infinite_writeback,
   int _readqueue_size,
   int _writequeue_size,
   int _prewritequeue_size,
   int _reserved_slots,
   int _start_trace,
   int _trace_interval)
    : BaseHier(_name, hier_params)
{
    width = _width;
    clockRate = _clock;

    /* Memory controller */
    infinite_writeback = _infinite_writeback;
    readqueue_size = _readqueue_size;
    writequeue_size = _writequeue_size;
    prewritequeue_size = _prewritequeue_size;
    reserved_slots = _reserved_slots;

    cout << "Configuration : " << infinite_writeback << " : " << readqueue_size << " : " << writequeue_size << " : " << prewritequeue_size << " : " << reserved_slots << endl;

    if (width < 1 || (width & (width - 1)) != 0)
	fatal("memory bus width must be positive non-zero and a power of two");

    if (clockRate < 1)
	fatal("memory bus clock must be positive and non-zero");

    nextDataFree = 0;
    nextAddrFree = 0;

    runAddrLast = 0;
    runDataLast = 0;

    busBlockedTime = 0;
    
    nextfree = 0;

    livearbs = 0;

    numInterfaces = 0;
    blocked = false;
    waitingFor = -1;

    lastarbevent = 0;

    arbiter_scheduled_flag = false;

    need_to_sort = true;

    memoryControllerEvent = new MemoryControllerEvent(this);
    memoryController = new RDFCFSTimingMemoryController();

    // Dirty parameterization
    memoryController->readqueue_size = readqueue_size;
    memoryController->writequeue_size = writequeue_size;
    memoryController->prewritequeue_size = prewritequeue_size;
    memoryController->reserved_slots = reserved_slots;

    // Memory Trace file
    traceFile.open("bustrace.txt");
    memoryTraceEvent = new MemoryTraceEvent(this,_trace_interval);
    memoryTraceEvent->schedule(_start_trace);

}

Bus::~Bus()
{
    delete memoryControllerEvent;
    delete memoryController;
    traceFile.flush();
    traceFile.close();
}

/* register bus stats */
void
Bus::regStats()
{
    using namespace Stats;

    addrIdleCycles
	.name(name() + ".addr_idle_cycles")
	.desc("number of cycles bus was idle")
	;

    addrIdleFraction
	.name(name() + ".addr_idle_fraction")
	.desc("fraction of time addr bus was idle")
	;

    addrIdleFraction = addrIdleCycles / simTicks;

    addrQdly
	.init(maxThreadsPerCPU)
	.name(name() + ".addr_queued_cycles")
	.desc("total number of queued cycles for all requests")
	.flags(total)
	;

    addrRequests
	.init(maxThreadsPerCPU)
	.name(name() + ".addr_requests")
	.desc("number of transmissions on bus")
	.flags(total)
	;

    addrQueued
	.name(name() + ".addr_queued")
	.desc("average queueing delay seen by bus request")
	.flags(total)
	;
    addrQueued = addrQdly / addrRequests;

    dataIdleCycles
	.name(name() + ".data_idle_cycles")
	.desc("number of cycles bus was idle")
	;
    
    dataUseCycles
        .name(name() + ".data_use_cycles")
        .desc("number of cycles bus was in use")
        ;
    dataUseCycles = (simTicks - dataIdleCycles);

    dataIdleFraction
	.name(name() + ".data_idle_fraction")
	.desc("fraction of time data bus was idle")
	;
    dataIdleFraction = dataIdleCycles / simTicks;


    dataQdly
	.init(maxThreadsPerCPU)
	.name(name() + ".data_queued_cycles")
	.desc("total number of queued cycles for all requests")
	.flags(total)
	;

    dataRequests
	.init(maxThreadsPerCPU)
	.name(name() + ".data_requests")
	.desc("number of transmissions on bus")
	.flags(total)
	;

    dataQueued
	.name(name() + ".data_queued")
	.desc("average queueing delay seen by bus request")
	.flags(total)
	;
    dataQueued = dataQdly / dataRequests;

    busBlocked
	.name(name() + ".bus_blocked")
	.desc("number of times bus was blocked")
	;

    busBlockedCycles
	.name(name() + ".bus_blocked_cycles")
	.desc("number of cycles bus was blocked")
	;

    busBlockedFraction
	.name(name() + ".bus_blocked_fraction")
	.desc("fraction of time bus was blocked")
	;
    busBlockedFraction = busBlockedCycles / simTicks;

    writebackCycles
            .name(name() + ".data_writeback_cycles")
            .desc("number of bus cycles used for writebacks")
            ;
    
    writebackFraction
            .name(name() + ".data_writeback_fraction")
            .desc("fraction of time used for writebacks")
            ;
    
    writebackFraction = writebackCycles / simTicks;
    
    unknownSenderCycles
            .name(name() + ".data_unknown_sender_cycles")
            .desc("number of bus cycles with req that is not from an L1 cache")
            ;
    
    unknownSenderFraction
            .name(name() + ".data_unknown_sender_fraction")
            .desc("fraction of time bus used by req that is not from an L1 cache")
            ;
    
    unknownSenderFraction = unknownSenderCycles / simTicks;

    nullGrants
	.name(name() + ".null_grants")
	.desc("number of null grants (wasted cycles)")
	;
}


void
Bus::resetStats()
{

}

void
Bus::requestDataBus(int id, Tick time)
{
    requestAddrBus(id, time);
}

void
Bus::requestAddrBus(int id, Tick time)
{
    assert(doEvents());
    assert(time>=curTick);
    DPRINTF(Bus, "id:%d Requesting Address Bus for cycle: %d\n", id, time);

    AddrArbiterEvent *eventptr = new AddrArbiterEvent(this,id,time);
    
    need_to_sort = true;

    if (!arbiter_scheduled_flag) {
       assert(arb_events.empty());
       eventptr->schedule(time);
       arbiter_scheduled_flag = true;
       currently_scheduled = eventptr;
    } else if (currently_scheduled->original_time > time) {
      currently_scheduled->deschedule();
      arb_events.push_back(currently_scheduled);
      eventptr->schedule(time);
      currently_scheduled = eventptr;
      assert(arbiter_scheduled_flag);
    } else {
       arb_events.push_back(eventptr);
    }

    livearbs++;
}

void
Bus::arbitrateAddrBus(int interfaceid)
{
    DPRINTF(Bus, "addr bus granted to id %d\n", interfaceid);
    livearbs--;
    interfaces[interfaceid]->grantAddr();
}

void
Bus::arbitrateDataBus()
{
    fatal("arbitrateDAtaBus() called!");
}

bool
Bus::sendAddr(MemReqPtr &req, Tick origReqTime)
{
    assert(doEvents());
    if (!req) {
	    // if the bus was granted in error
	    nullGrants++;
	    DPRINTF(Bus, "null request");
	    return false;
    }
    

    DPRINTF(Bus, "issuing req %s addr %x from id %d, name %s\n",
	    req->cmd.toString(), req->paddr,
	    req->busId, interfaces[req->busId]->name());

    if (infinite_writeback && (req->cmd == Write || req->cmd == Prewrite)) {
      return true;
    }
    
    // Insert request into memory controller
    memoryController->insertRequest(req);

    // Schedule memory controller if not scheduled yet.
    if (!memoryControllerEvent->scheduled()) {
        memoryControllerEvent->schedule(curTick);
    }

    return true;
}

void
Bus::handleMemoryController()
{
    if (memoryController->hasMoreRequests()) {
        MemReqPtr &request = memoryController->getRequest();

        assert(slaveInterfaces.size() == 1);
        slaveInterfaces[0]->access(request);
    }
}

/* This tunction is called when the DRAM has calculated the latency */
void Bus::latencyCalculated(MemReqPtr &req, Tick time) 
{
    assert(!memoryControllerEvent->scheduled());
    memoryControllerEvent->schedule(time);
    nextfree = time;
    if (req->cmd == Read) { 
        assert(req->busId < interfaces.size() && req->busId > -1);
        DeliverEvent *deliverevent = new DeliverEvent(interfaces[req->busId], req);
        deliverevent->schedule(time);
    }
}

void
Bus::traceBus(void) 
{
  traceFile << curTick << " ";
  traceFile << memoryController->dumpstats();
  traceFile << endl;
}

void 
Bus::delayData(int size, int senderID, MemCmdEnum cmd)
{
    fatal("delayData() not used in new bus implementation");
}

void
Bus::sendData(MemReqPtr &req, Tick origReqTime)
{
    fatal("sendData not used in new bus implementation");
}

void
Bus::sendAck(MemReqPtr &req, Tick origReqTime)
{
    fatal("sendAck not used in new bus implementation");
}

int
Bus::registerInterface(BusInterface<Bus> *bi, bool master)
{
    assert(numInterfaces == interfaces.size());
    interfaces.push_back(bi);
    addrBusRequests.push_back(BusRequestRecord());
    dataBusRequests.push_back(BusRequestRecord());

    DPRINTF(Bus, "registering interface %s as %s\n",
            bi->name(), master ? "master" : "slave");

    if (master) {
        transmitInterfaces.insert(transmitInterfaces.begin(),bi);
    } else {
        slaveInterfaces.push_back(bi);
        memoryController->registerInterface(bi);
        transmitInterfaces.push_back(bi);
    }
    
    return numInterfaces++;
}

void
Bus::clearBlocked(int id)
{
    fatal("clearBlocked is not used in new bus implementation");
}

void
Bus::setBlocked(int id)
{
    fatal("setBlocked is not used in the new bus implementation");
}

bool
Bus::findOldestRequest(std::vector<BusRequestRecord> & requests,
                       int & grant_id, int & old_grant_id)
{
    fatal("findOldestRequest is not used in the new bus implementation");
    return false;
}

void
Bus::scheduleArbitrationEvent(Event * arbiterEvent, Tick reqTime,
                              Tick nextFreeCycle, Tick idleAdvance)
{
    fatal("scheduleArbitrationEvent is not used in the new bus implementation");
}

Tick
Bus::probe(MemReqPtr &req, bool update)
{
    fatal("probe is not used in the new bus implementation");
    return 0;
}

void
Bus::collectRanges(list<Range<Addr> > &range_list)
{
    for (int i = 0; i < numInterfaces; ++i) {
	interfaces[i]->getRange(range_list);
    }
}

void
Bus::rangeChange()
{
    for (int i = 0; i < numInterfaces; ++i) {
	interfaces[i]->rangeChange();
    }
}

void
Bus::resetAdaptiveStats(){
    fatal("reset adaptive stats is not implemented");
}

void
Bus::storeUseStats(bool data, int senderID){
    
    fatal("storeUseStats is not implemented");
}


double
Bus::getAddressBusUtilisation(Tick sampleSize){
    
    fatal("getAddressBusUtil is not implemented");
    return 0.0;
}

double
Bus::getDataBusUtilisation(Tick sampleSize){
    
    fatal("getDataBusUtil is not implemented");
    return 0.0;
    
}

vector<int>
Bus::getDataUsePerCPUId(){
    
    vector<int> retval;
    fatal("getDataUsePerCPUId not implemented");
    return retval;
}

vector<int>
Bus::getAddressUsePerCPUId(){
    vector<int> retval = perCPUAddressBusUse;
    fatal("getAddrUsePerCPUId not implemented");
    return retval;
}

#ifdef DO_BUS_TRACE
void
Bus::writeTraceFileLine(Addr address, string message){
    ofstream file("busAccessTrace.txt", ofstream::app);
    file << curTick << ": " << message << " " << address << "\n"; 
    file.flush();
    file.close();
}
#endif

// Event implementations
void
AddrArbiterEvent::process()
{
    assert(bus->lastarbevent <= this->original_time);
    assert(this->original_time <= curTick);
    bus->lastarbevent = this->original_time;

    if (bus->memoryController->isBlocked()) { 
        this->setpriority(Resched_Arb_Pri);
        this->schedule(bus->nextfree);
    } else {
        bus->arbitrateAddrBus(interfaceid);
        if (!bus->arb_events.empty()) {
            if (bus->need_to_sort) {
                bus->arb_events.sort(event_compare());
                bus->need_to_sort = false;
            } 

            // Find next arb event and schedule it.
            AddrArbiterEvent *tmp = bus->arb_events.front();
            if (curTick > tmp->original_time) {
                tmp->schedule(curTick);
            } else {
                tmp->schedule(tmp->original_time);
            }
            bus->arb_events.pop_front();
            bus->currently_scheduled = tmp;
        } else {
            bus->arbiter_scheduled_flag = false;
        }
        delete this;
    }
}

const char *
AddrArbiterEvent::description()
{
    return "address bus arbitration";
}


void
DataArbiterEvent::process()
{
    DPRINTF(Bus, "Data Arb processing event\n");
    bus->arbitrateDataBus();
}

const char *
DataArbiterEvent::description()
{
    return "data bus arbitration";
}


void
DeliverEvent::process()
{
    bi->deliver(req);
    delete this;
}

const char *
DeliverEvent::description()
{
    return "bus deliver";
}

void
MemoryTraceEvent::process()
{
  bus->traceBus();
  this->schedule(curTick + rescheduleTime);
}

const char *
MemoryTraceEvent::description()
{
    return "Memory Tracing point";
}

void
MemoryControllerEvent::process()
{
    bus->handleMemoryController();
}

const char *
MemoryControllerEvent::description()
{
    return "memory controller invocation";
}
#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Bus)

    Param<int> width;
    Param<int> clock;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
    Param<bool> infinite_writeback;
    Param<int> readqueue_size;
    Param<int> writequeue_size;
    Param<int> prewritequeue_size;
    Param<int> reserved_slots;
    Param<int> start_trace;
    Param<int> trace_interval;

END_DECLARE_SIM_OBJECT_PARAMS(Bus)


BEGIN_INIT_SIM_OBJECT_PARAMS(Bus)

    INIT_PARAM(width, "bus width in bytes"),
    INIT_PARAM(clock, "bus frequency"),
    INIT_PARAM_DFLT(hier,
		    "Hierarchy global variables",
		    &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "Adaptive MHA object",NULL),
    INIT_PARAM_DFLT(infinite_writeback, "Infinite Writeback Queue", false),
    INIT_PARAM_DFLT(readqueue_size, "Max size of read queue", 64),
    INIT_PARAM_DFLT(writequeue_size, "Max size of write queue", 64),
    INIT_PARAM_DFLT(prewritequeue_size, "Max size of prewriteback queue", 64),
    INIT_PARAM_DFLT(reserved_slots, "Numer of activations reserved for reads", 2),
    INIT_PARAM_DFLT(start_trace, "Point to start tracing", 0),
    INIT_PARAM_DFLT(trace_interval, "How often to trace", 100000)

END_INIT_SIM_OBJECT_PARAMS(Bus)


CREATE_SIM_OBJECT(Bus)
{
    return new Bus(getInstanceName(),
                   hier,
                   width,
                   clock,
                   adaptive_mha,
                   infinite_writeback,
                   readqueue_size,
                   writequeue_size,
                   prewritequeue_size,
                   reserved_slots,
                   start_trace,
                   trace_interval);
}

REGISTER_SIM_OBJECT("Bus", Bus)

#endif //DOXYGEN_SHOULD_SKIP_THIS
