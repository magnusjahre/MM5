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
         int _cpu_count,
         int _bank_count,
         TimingMemoryController* _memoryController)
    : BaseHier(_name, hier_params)
{
    width = _width;
    clockRate = _clock;
    cpu_count = _cpu_count;
    bank_count = _bank_count;
    
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
    
    perCPUDataBusUse.resize(cpu_count, 0);
    perCPUQueueCycles.resize(cpu_count, 0);
    perCPURequests.resize(cpu_count, 0);
    if(_adaptiveMHA != NULL) _adaptiveMHA->registerBus(this);
    
    if(_memoryController == NULL) fatal("A memory controller must be provided to the memory bus");
    _memoryController->registerBus(this);
    memoryController = _memoryController;
    memoryControllerEvent = new MemoryControllerEvent(this);
    
#ifdef DO_BUS_TRACE
    ofstream file("busAccessTrace.txt");
    file << "";
    file.flush();
    file.close();
#endif

}

Bus::~Bus()
{
    delete memoryControllerEvent;
}

/* register bus stats */
void
Bus::regStats()
{
    using namespace Stats;

    busUseCycles
        .name(name() + ".bus_use_cycles")
        .desc("Number of cycles the data bus is in use")
        ;

    busUtilization
        .name(name() + ".bus_utilization")
        .desc("Data bus utilization")
        ;
    
    busUtilization = busUseCycles / simTicks;
    
    unknownSenderCycles
        .name(name() + ".unknown_sender_bus_use_cycles")
        .desc("Number of cycles the bus was used by an unknown sender")
        ;
    
    unknownSenderUtilization
        .name(name() + ".unknown_sender_utilization")
        .desc("Bus utilization by unknown sender requests")
        ;

    unknownSenderUtilization = unknownSenderCycles / simTicks;
    
    unknownSenderRequests
        .name(name() + ".unknown_sender_requests")
        .desc("Number of requests with unknown sender")
        ;
    
    unknownSenderFraction
        .name(name() + ".unknown_sender_fraction")
        .desc("Percentage of requests that have an unknown origin")
        ;
    
    unknownSenderFraction = unknownSenderRequests / totalRequests;
    
    totalQueueCycles
            .name(name() + ".total_queue_cycles")
            .desc("Total number of cycles spent in memory controller queue")
            ;
    
    totalRequests
            .name(name() + ".total_requests")
            .desc("Total number of requests to memory controller")
            ;

    avgQueueCycles
            .name(name() + ".avg_queue_cycles")
            .desc("Average number of cycles each request spent in queue (not counting blocked cycles)")
            ;

    avgQueueCycles = totalQueueCycles / totalRequests;
    
    blockedCycles
            .name(name() + ".blocked_cycles")
            .desc("Number of cycles the memory controller was blocked")
            ;
    
    nullGrants
            .name(name() + ".null_grants")
            .desc("Number of times the bus was granted in error")
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
       assert(time >= curTick);
       eventptr->schedule(time);
       arbiter_scheduled_flag = true;
       currently_scheduled = eventptr;
    } else if (currently_scheduled->original_time > time) {
      currently_scheduled->deschedule();
      arb_events.push_back(currently_scheduled);
      assert(time >= curTick);
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
    fatal("arbitrateDataBus() called!");
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
    
    // Insert request into memory controller
    memoryController->insertRequest(req);
    totalRequests++;

#ifdef DO_BUS_TRACE
    assert(slaveInterfaces.size() == 1);
    writeTraceFileLine(req->paddr, 
                       slaveInterfaces[0]->getMemoryBankID(req->paddr),
                       (req->paddr >> slaveInterfaces[0]->getPageSize()),
                       -1,
                       req->cmd,
                       "Request");
#endif
    
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
        
#ifdef DO_BUS_TRACE
        assert(slaveInterfaces.size() == 1);
        writeTraceFileLine(request->paddr, 
                           slaveInterfaces[0]->getMemoryBankID(request->paddr),
                           (request->paddr >> slaveInterfaces[0]->getPageSize()),
                           -1,
                           request->cmd,
                           "Send");
#endif

        if(request->cmd != Activate && request->cmd != Close){
            int queue_lat = curTick - request->inserted_into_memory_controller;
            totalQueueCycles += queue_lat;
            if(request->adaptiveMHASenderID != -1){
                perCPUQueueCycles[request->adaptiveMHASenderID] += queue_lat;
                perCPURequests[request->adaptiveMHASenderID] += 1;
                int sum = 0;
                for(int i=0;i<perCPURequests.size();i++) {
                    sum += perCPURequests[i];
                }
            }
        }
        
        assert(slaveInterfaces.size() == 1);
        slaveInterfaces[0]->access(request);
    }
}

/* This function is called when the DRAM has calculated the latency */
void Bus::latencyCalculated(MemReqPtr &req, Tick time) 
{
    assert(!memoryControllerEvent->scheduled());
    assert(time >= curTick);
    memoryControllerEvent->schedule(time);
    nextfree = time;
    
#ifdef DO_BUS_TRACE
    assert(slaveInterfaces.size() == 1);
    writeTraceFileLine(req->paddr, 
                       slaveInterfaces[0]->getMemoryBankID(req->paddr),
                       (req->paddr >> slaveInterfaces[0]->getPageSize()),
                       time - curTick,
                       req->cmd,
                       "Latency");
#endif
    
    
    if(req->cmd != Activate && req->cmd != Close){
        assert(slaveInterfaces.size() == 1);
        busUseCycles += slaveInterfaces[0]->getDataTransTime();
        if(req->adaptiveMHASenderID != -1){
            perCPUDataBusUse[req->adaptiveMHASenderID] += slaveInterfaces[0]->getDataTransTime();
        }
        else{
            unknownSenderCycles += slaveInterfaces[0]->getDataTransTime();
        }
    }
    
    if (req->cmd == Read) { 
        assert(req->busId < interfaces.size() && req->busId > -1);
        DeliverEvent *deliverevent = new DeliverEvent(interfaces[req->busId], req);
        deliverevent->schedule(time);
    }
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
Bus::incrementBlockedCycles(Tick cycles){
    blockedCycles += cycles;
}

void
Bus::resetAdaptiveStats(){
    for(int i=0;i<perCPUDataBusUse.size();i++) perCPUDataBusUse[i] = 0;
    for(int i=0;i<perCPUQueueCycles.size();i++) perCPUQueueCycles[i] = 0;
    for(int i=0;i<perCPURequests.size();i++) perCPURequests[i] = 0;
}

double
Bus::getAverageQueue(Tick sampleSize){
    
    int sum = 0;
    int reqs = 0;
    for(int i=0;i<perCPUQueueCycles.size();i++) sum += perCPUQueueCycles[i];
    for(int i=0;i<perCPURequests.size();i++) reqs += perCPURequests[i];
    if(reqs == 0) return 0;
    return (double) ((double) sum / (double) reqs);
}

double
Bus::getDataBusUtilisation(Tick sampleSize){
    
    int sum = 0;
    for(int i=0;i<cpu_count;i++) sum += perCPUDataBusUse[i];
    return (double) ((double) sum / (double) sampleSize);
}

vector<int>
Bus::getDataUsePerCPUId(){
    
    vector<int> retval= perCPUDataBusUse;
    return retval;
}

vector<double>
Bus::getAverageQueuePerCPU(){
    
    vector<double> retval;
    retval.resize(perCPUQueueCycles.size(), 0);
    assert(perCPUQueueCycles.size() == perCPURequests.size());
    for(int i=0;i<perCPUQueueCycles.size();i++){
        if(perCPURequests[i] == 0) retval[i] = 0;
        else retval[i] = (double) ((double) perCPUQueueCycles[i] / (double) perCPURequests[i]);
    }
    return retval;
}

#ifdef DO_BUS_TRACE
void
Bus::writeTraceFileLine(Addr address, int bank, Addr page, Tick latency, MemCmd cmd, std::string message){
    ofstream file("busAccessTrace.txt", ofstream::app);
    file << curTick << ","
         << address << ","
         << bank << ","
         << page << ","
         << latency << ","
         << cmd << ","
         << message << "\n";
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
        if(bus->nextfree <= curTick) this->schedule(curTick);
        else this->schedule(bus->nextfree);
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
                assert(tmp->original_time >= curTick);
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
MemoryControllerEvent::process()
{
    bus->handleMemoryController();
}

const char *
MemoryControllerEvent::description()
{
    return "memory controller invocation";
}

// Unused methods

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


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Bus)

    Param<int> width;
    Param<int> clock;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
    Param<int> cpu_count;
    Param<int> bank_count;
    SimObjectParam<TimingMemoryController *> memory_controller;
    

END_DECLARE_SIM_OBJECT_PARAMS(Bus)


BEGIN_INIT_SIM_OBJECT_PARAMS(Bus)

    INIT_PARAM(width, "bus width in bytes"),
    INIT_PARAM(clock, "bus frequency"),
    INIT_PARAM_DFLT(hier,
		    "Hierarchy global variables",
		    &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "Adaptive MHA object",NULL),
    INIT_PARAM(cpu_count, "Number of CPUs"),
    INIT_PARAM(bank_count, "Number of L2 cache banks"),
    INIT_PARAM_DFLT(memory_controller, "Memory controller object", NULL)
                    

END_INIT_SIM_OBJECT_PARAMS(Bus)


CREATE_SIM_OBJECT(Bus)
{
    return new Bus(getInstanceName(),
                   hier,
                   width,
                   clock,
                   adaptive_mha,
                   cpu_count,
                   bank_count,
                   memory_controller);
}

REGISTER_SIM_OBJECT("Bus", Bus)

#endif //DOXYGEN_SHOULD_SKIP_THIS
