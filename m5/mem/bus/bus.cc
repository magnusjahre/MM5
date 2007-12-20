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
         bool _uniform_partitioning)
    : BaseHier(_name, hier_params)
{
    width = _width;
    clockRate = _clock;
    
    if(_uniform_partitioning){
        fatal("Uniform bus partitioning not implemented");
    }

    if (width < 1 || (width & (width - 1)) != 0)
	fatal("memory bus width must be positive non-zero and a power of two");

    if (clockRate < 1)
	fatal("memory bus clock must be positive and non-zero");

    nextDataFree = 0;
    nextAddrFree = 0;

    runAddrLast = 0;
    runDataLast = 0;

    busBlockedTime = 0;

    numInterfaces = 0;
    blocked = false;
    waitingFor = -1;

    addrArbiterEvent = new AddrArbiterEvent(this);
    dataArbiterEvent = new DataArbiterEvent(this);
    
    if(_adaptiveMHA != NULL){
//         lastAddrBusIdleCycles = 0;
//         lastDataBusIdleCycles = 0;
        _adaptiveMHA->registerBus(this);
        
        int tmpCpuCount = _adaptiveMHA->getCPUCount();
        perCPUAddressBusUse.resize(tmpCpuCount, 0);
        perCPUAddressBusUseOverflow.resize(tmpCpuCount, 0);
        perCPUDataBusUse.resize(tmpCpuCount, 0);
        perCPUDataBusUseOverflow.resize(tmpCpuCount, 0);
        
        adaptiveSampleSize = _adaptiveMHA->getSampleSize();
        
        addrBusUseSamples[0] = 0;
        addrBusUseSamples[1] = 0;
        
        dataBusUseSamples[0] = 0;
        dataBusUseSamples[1] = 0;
        
    }
    else{
        adaptiveSampleSize = -1;
    }
}

Bus::~Bus()
{
    delete addrArbiterEvent;
    delete dataArbiterEvent;
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
    nextDataFree = curTick;
    nextAddrFree = curTick;
    
}

void
Bus::requestDataBus(int id, Tick time)
{
    
//     if(curTick > 357000){
//         cout << curTick << ": data bus requested by " << id << " at " << time << "\n";
//     }
    
    assert(doEvents());
    assert(time>=curTick);
    DPRINTF(Bus, "id:%d Requesting Data Bus for cycle: %d\n", id, time);
    if (dataBusRequests[id].requested) {
	return;
    }
    dataBusRequests[id].requested = true;
    // fix this: should we set the time to bus cycle?
    dataBusRequests[id].requestTime = time;
    scheduleArbitrationEvent(dataArbiterEvent,time,nextDataFree);
}

void
Bus::requestAddrBus(int id, Tick time)
{
//     if(curTick > 357000){
//         cout << curTick << ": addr bus requested by " << id << " at " << time << "\n";
//     }
    
    assert(doEvents());
    assert(time>=curTick);
    DPRINTF(Bus, "id:%d Requesting Address Bus for cycle: %d\n", id, time);
    if (addrBusRequests[id].requested) {
	return; // already requested
    }
    addrBusRequests[id].requested = true;
    addrBusRequests[id].requestTime = time;
    if (!isBlocked()) {
	scheduleArbitrationEvent(addrArbiterEvent,time,nextAddrFree,2);
    }
}

void
Bus::arbitrateAddrBus()
{
    assert(curTick > runAddrLast);
    runAddrLast = curTick;
    assert(doEvents());
    int grant_id;
    int old_grant_id;; // used for rescheduling

#ifndef NDEBUG
    bool found =
#endif
	findOldestRequest(addrBusRequests,grant_id,old_grant_id);

    assert(found);

//     if(curTick > 357000){
//         cout << curTick << ": addr granted to id " << grant_id << " at " << curTick << "\n";
//     }
    
    // grant_id is earliest outstanding request
    // old_grant_id is second earliest request

    DPRINTF(Bus, "addr bus granted to id %d\n", grant_id);

    addrBusRequests[grant_id].requested = false; // clear request bit
    bool do_request = interfaces[grant_id]->grantAddr();


    if (do_request) {
	addrBusRequests[grant_id].requested = true;
	addrBusRequests[grant_id].requestTime = curTick;
    }

    Tick grant_time;
    Tick old_grant_time = (old_grant_id == -1) ? TICK_T_MAX :
	addrBusRequests[old_grant_id].requestTime;

    if (!isBlocked()) {
	grant_time = old_grant_time;
	// find earliest outstand request, need to re-search because could have
	// rerequests in grantAddr()
	if (addrBusRequests[grant_id].requested
	    && addrBusRequests[grant_id].requestTime < old_grant_time) {
	    grant_time = addrBusRequests[grant_id].requestTime;
	}

	if (grant_time != TICK_T_MAX) {
	    if (!addrArbiterEvent->scheduled()) {
		scheduleArbitrationEvent(addrArbiterEvent,grant_time,nextAddrFree,2);
	    }
	} else {
	    // fix up any scheduling errors
	    if (addrArbiterEvent->scheduled() && nextAddrFree > addrArbiterEvent->when()) {
		addrArbiterEvent->reschedule(nextAddrFree);
	    }
	}
    } else {
	if (addrArbiterEvent->scheduled()) {
	    addrArbiterEvent->deschedule();
	}
    }
}

void
Bus::arbitrateDataBus()
{
    assert(curTick>runDataLast);
    runDataLast = curTick;
    assert(doEvents());
    int grant_id;
    int old_grant_id;


#ifndef NDEBUG
    bool found =
#endif
	findOldestRequest(dataBusRequests,grant_id,old_grant_id);
    assert(found);

    DPRINTF(Bus, "Data bus granted to id %d\n", grant_id);

    dataBusRequests[grant_id].requested = false; // clear request bit
    interfaces[grant_id]->grantData();

    Tick grant_time;
    Tick old_grant_time = (old_grant_id == -1) ? TICK_T_MAX :
	dataBusRequests[old_grant_id].requestTime;

    //reschedule arbiter here
    grant_time = old_grant_time;
    // find earliest outstand request, need to re-search because could have
    // rerequests in grantData()
    if (dataBusRequests[grant_id].requested
	&& dataBusRequests[grant_id].requestTime < old_grant_time) {
	grant_time = dataBusRequests[grant_id].requestTime;
    }

    if (grant_time != TICK_T_MAX) {
	scheduleArbitrationEvent(dataArbiterEvent,grant_time,nextDataFree);
    }
}


bool
Bus::sendAddr(MemReqPtr &req, Tick origReqTime)
{
    // update statistics
   
#ifdef DO_BUS_TRACE
    writeTraceFileLine(req->paddr, "Address Bus sending address");
#endif
    
//     if(curTick > 357000){
//         cout << curTick << ": addr sending addr " << req->paddr << ", busId " << req->busId << "\n";
//     }
    
    assert(doEvents());
    if (!req) {
	// if the bus was granted in error
	nullGrants++;
	DPRINTF(Bus, "null request");
	return false;
    }
    
    addrQdly[req->thread_num] += curTick - origReqTime;
    addrIdleCycles += curTick - nextAddrFree;

    // Advance nextAddrFree to next clock cycle
    nextAddrFree = nextBusClock(curTick);
    
    storeUseStats(false, req->adaptiveMHASenderID);
    
    // put it here so we know requesting thread
    addrRequests[req->thread_num]++;

//     if(perCPUAddressBusUse.size() > 0 && req->adaptiveMHASenderID != -1){
//         perCPUAddressBusUse[req->adaptiveMHASenderID] += (nextAddrFree - curTick);
//     }
    
    int responding_interface_id = -1;
    MemAccessResult retval = BA_NO_RESULT;
    MemAccessResult tmp_retval = BA_NO_RESULT;

    DPRINTF(Bus, "issuing req %s addr %x from id %d, name %s\n",
	    req->cmd.toString(), req->paddr,
	    req->busId, interfaces[req->busId]->name());

    for (int i = 0; i < numInterfaces; i++) {
	if (interfaces[req->busId] != transmitInterfaces[i]) {
	    tmp_retval = transmitInterfaces[i]->access(req);
	} else {
	    tmp_retval = BA_NO_RESULT;
	}
	if (req->isNacked()) {
	    //Can only get a NACK if the block was owned, it won't satisfy
	    assert(!req->isSatisfied());
	    //Clear the flags before retrying request
	    DPRINTF(Bus, "Blk: %x Nacked by id:%d\n",
		    req->paddr & (((ULL(1))<<48)-1), i);
	    req->flags &= ~NACKED_LINE;
	    return false;
	    //Signal failure, it will retry for bus
	}
	if (tmp_retval != BA_NO_RESULT) {
	    if (retval != BA_NO_RESULT) {
		fatal("Two suppliers for address %#x on bus %s", req->paddr,
		      name());
	    }
	    retval = tmp_retval;
	    responding_interface_id = transmitInterfaces[i]->getId();
	}
    }

    if (retval == BA_NO_RESULT && (req->cmd.isRead() || req->cmd.isWrite())) {
	fatal("No supplier for address %#x on bus %s", req->paddr, name());
    }

    if (retval == BA_SUCCESS) {
	DPRINTF(Bus, "request for blk %x from id:%d satisfied by id:%d\n",
		req->paddr & (((ULL(1))<<48)-1), req->busId,
		responding_interface_id);
    }
    // If any request if blocked by a memory interface, it blocks
    // the whole bus because the request is still outstanding on
    // the wire.  We cannot let _any_ other request go through.
    switch (retval) {
      case BA_BLOCKED:
	  DPRINTF(Bus, "Bus Blocked, waiting for id:%d on blk_adr: %x\n",
		  responding_interface_id, req->paddr & (((ULL(1))<<48)-1));
	  blockedReq = req;
	  blockSync = true; //This is a synchronus block
	  return true;
	  break;

      case BA_SUCCESS:
	  break;

      case BA_NO_RESULT:
	  break;

      default:
	panic("Illegal bus result %d\n", retval);
	break;
    };
    
    assert(curTick % clockRate == 0);
    
    //New callback function
    //May want to use this to move some requests in front of a NACKED
    //request (depending on consistency model
    interfaces[req->busId]->snoopResponseCall(req);

    return true;
}

void 
Bus::delayData(int size, int senderID, MemCmdEnum cmd)
{
    
    int transfer_cycles = DivCeil(size, width);
    //int transfer_time = transfer_cycles * clockRate;
    assert(curTick >= nextDataFree);
    dataIdleCycles += (curTick - nextDataFree);
    
    // assumes we are cycle aligned right now
    assert(curTick % clockRate == 0);
    nextDataFree = nextBusClock(curTick,transfer_cycles);
    
    storeUseStats(true, senderID);
    if(cmd == Writeback){
//         cout << "adding " << (nextDataFree - curTick) << " cycles to wbCycles (2)\n";
        writebackCycles += (nextDataFree - curTick);
    }
    
//     if(perCPUDataBusUse.size() > 0 && senderID != -1){
//         perCPUDataBusUse[senderID] += (nextDataFree - curTick);
//     }
}

void
Bus::sendData(MemReqPtr &req, Tick origReqTime)
{
#ifdef DO_BUS_TRACE
    writeTraceFileLine(req->paddr, "Data Bus sending address");
#endif
    
//     if(curTick > 357000){
//         cout << curTick << ": data sending addr " << req->paddr << ", busID is " << req->busId << "\n";
//     }
    
    // put it here so we know requesting thread
    dataRequests[req->thread_num]++;
   
    assert(doEvents());
    int transfer_cycles = DivCeil(req->size, width);
    
    assert(curTick >= nextDataFree);
    dataQdly[req->thread_num] += curTick - origReqTime;
    dataIdleCycles += (curTick - nextDataFree);
    
    nextDataFree = nextBusClock(curTick,transfer_cycles);
    
    storeUseStats(true, req->adaptiveMHASenderID);
    
    if(req->cmd == Writeback){
//         cout << "adding " << (nextDataFree - curTick) << " cycles to wbCycles (1)\n";
        writebackCycles += (nextDataFree - curTick);
    }
    
//     if(perCPUDataBusUse.size() > 0 && req->adaptiveMHASenderID != -1){
//         perCPUDataBusUse[req->adaptiveMHASenderID] += (nextDataFree - curTick);
//     }
    
    DeliverEvent *tmp = new DeliverEvent(interfaces[req->busId], req);
    // let the cache figure out Critical word first
    // schedule event after first block delivered
    tmp->schedule(curTick + clockRate);
}

void
Bus::sendAck(MemReqPtr &req, Tick origReqTime)
{
    addrRequests[req->thread_num]++;
   
    assert(doEvents());
    addrQdly[req->thread_num] += curTick - origReqTime;
    addrIdleCycles += curTick - nextAddrFree;
    // Advance nextAddrFree to next clock cycle
    nextAddrFree = nextBusClock(curTick);
    
    storeUseStats(false, req->adaptiveMHASenderID);
    
    if(adaptiveSampleSize >= 0){
        fatal("Adaptive MHA is not compatible with sendACK method");
    }
    
    DeliverEvent *tmp = new DeliverEvent(interfaces[req->busId], req);
    tmp->schedule(curTick + clockRate);
    DPRINTF(Bus, "sendAck: scheduling deliver for %x on id %d @ %d\n",
            req->paddr, req->busId, curTick + clockRate);
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
	transmitInterfaces.push_back(bi);
    }

    return numInterfaces++;
}

void
Bus::clearBlocked(int id)
{
    assert(!isBlocked() || doEvents());
    if (isBlocked() && waitingFor == id) {
	DPRINTF(Bus, "Unblocking\n");
	if (blockSync) {
	    DPRINTF(Bus, "Bus UnBlocked, waiting for id:%d on blk_adr: %x\n",
		    id, blockedReq->paddr & (((ULL(1))<<48)-1));
	}
	//Only arbitrate if request exists
	//Need to make sure it wasn't requested for later time
	int grant_id, old_grant_id; // used for rescheduling
	if (findOldestRequest(addrBusRequests,grant_id,old_grant_id)) {
	    nextAddrFree = nextBusClock(curTick,1);
	    Tick time = addrBusRequests[grant_id].requestTime;
	    if (time <= curTick) { 
		addrArbiterEvent->schedule(nextBusClock(curTick,2));
	    }
	    else {
		scheduleArbitrationEvent(addrArbiterEvent,time,nextAddrFree,2);
	    }
	}
	blocked = false;
	waitingFor = -1;
	busBlockedCycles += curTick - busBlockedTime;
    }
}

void
Bus::setBlocked(int id)
{
    if (blocked) warn("Blocking on a second cause???\n");
    DPRINTF(Bus, "Blocked, waiting for id:%d\n", id);
    blocked = true;
    waitingFor = id;
    busBlockedTime = curTick;
    busBlocked++;
    blockSync = false;
    if (addrArbiterEvent->scheduled()) {
	addrArbiterEvent->deschedule();
    }

}

bool
Bus::findOldestRequest(std::vector<BusRequestRecord> & requests,
			   int & grant_id, int & old_grant_id)
{
    grant_id = -1;
    old_grant_id = -1;
    Tick grant_time = TICK_T_MAX; // set to arbitrarily large number
    Tick old_grant_time = TICK_T_MAX;
    for (int i=0; i<numInterfaces; i++) {
	if (requests[i].requested) {
	    if (requests[i].requestTime < grant_time) {
		old_grant_time = grant_time;
		grant_time = requests[i].requestTime;
		old_grant_id = grant_id;
		grant_id = i;
	    }
	    else if (requests[i].requestTime < old_grant_time) {
		old_grant_time = requests[i].requestTime;
		old_grant_id = i;
	    }
	}
    }
    return (grant_id != -1);
}

void
Bus::scheduleArbitrationEvent(Event * arbiterEvent, Tick reqTime,
			      Tick nextFreeCycle, Tick idleAdvance)
{
    bool bus_idle = (nextFreeCycle <= curTick);
    Tick next_schedule_time;
    if (bus_idle) {
	if (reqTime < curTick) {
	    next_schedule_time = nextBusClock(curTick,idleAdvance);
	} else {
	    next_schedule_time = nextBusClock(reqTime,idleAdvance);
	}
    } else {
	if (reqTime < nextFreeCycle) {
	    next_schedule_time = nextFreeCycle;
	} else {
	    next_schedule_time = nextBusClock(reqTime,idleAdvance);
	}
    }

    if (arbiterEvent->scheduled()) {
	if (arbiterEvent->when() > next_schedule_time) {
	    arbiterEvent->reschedule(next_schedule_time);
            DPRINTF(Bus, "Rescheduling arbiter event for cycle %d\n",
                    next_schedule_time);
	}
    } else {
	arbiterEvent->schedule(next_schedule_time);
        DPRINTF(Bus, "scheduling arbiter event for cycle %d\n",
                next_schedule_time);
    }
}

Tick
Bus::probe(MemReqPtr &req, bool update)
{
    bool satisfied = req->isSatisfied();
    Tick time_sat = 0;
    for (int i = 0; i < numInterfaces; i++) {
	if (interfaces[req->busId] != transmitInterfaces[i]) {
	    if (!satisfied) {
		time_sat = transmitInterfaces[i]->probe(req, update);
		satisfied = req->isSatisfied();
	    } else {
		transmitInterfaces[i]->probe(req, update);
	    }
	}
    }
    assert(satisfied);
    return time_sat;
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
    
    addrBusUseSamples[0] = 0;
    addrBusUseSamples[1] = 0;
        
    dataBusUseSamples[0] = 0;
    dataBusUseSamples[1] = 0;
    
    for(int i=0;i<perCPUAddressBusUse.size();i++) perCPUAddressBusUse[i] = 0;
    for(int i=0;i<perCPUAddressBusUseOverflow.size();i++) perCPUAddressBusUseOverflow[i] = 0;
    for(int i=0;i<perCPUDataBusUse.size();i++) perCPUDataBusUse[i] = 0;
    for(int i=0;i<perCPUDataBusUseOverflow.size();i++) perCPUDataBusUseOverflow[i] = 0;
}

void
Bus::storeUseStats(bool data, int senderID){
    
    int* array;
    vector<int>* useVector;
    vector<int>* overflowVector;
    Tick nextFree;
    
    if(data){
        array = dataBusUseSamples;
        useVector = &perCPUDataBusUse;
        overflowVector = &perCPUDataBusUseOverflow;
        nextFree = nextDataFree;
    }
    else{
        array = addrBusUseSamples;
        useVector = &perCPUAddressBusUse;
        overflowVector = &perCPUAddressBusUseOverflow;
        nextFree = nextAddrFree;
    }
    
    if(adaptiveSampleSize >= 0){
        
        // utilisation measurements
        if((curTick % (2 * adaptiveSampleSize) < adaptiveSampleSize 
            && nextFree % (2 * adaptiveSampleSize) < adaptiveSampleSize) 
            || (curTick % (2 * adaptiveSampleSize) >= adaptiveSampleSize 
            && nextFree % (2 * adaptiveSampleSize) >= adaptiveSampleSize) 
          ){
            if(curTick % (2 * adaptiveSampleSize) < adaptiveSampleSize){
//                 cout << curTick << ": buffer 0, adding " << (nextFree - curTick) << "\n";
                array[0] += (nextFree - curTick);
            }
            else{
//                 cout << curTick << ": buffer 1, adding " << (nextFree - curTick) << "\n";
                array[1] += (nextFree - curTick);
            }
        }
        else{
            int firstCycles = adaptiveSampleSize - (curTick %  adaptiveSampleSize);
            int rest = nextFree % adaptiveSampleSize;
        
            if(curTick % (2 * adaptiveSampleSize) < adaptiveSampleSize){
                
//                 cout << curTick << ": special (cur to 0), first " << firstCycles << ", rest " << rest << "\n";
                
                array[0] += firstCycles;
                array[1] += rest;
            }
            else{
                
//                 cout << curTick << ": special (cur to 1), first " << firstCycles << ", rest " << rest << "\n";
                
                array[1] += firstCycles;
                array[0] += rest;
            }
        }
        
        // per CPU use
        if(senderID != -1){
            if((curTick % adaptiveSampleSize) > (nextFree % adaptiveSampleSize)){
                (*useVector)[senderID] += (adaptiveSampleSize - (curTick % adaptiveSampleSize));
                (*overflowVector)[senderID] += (nextFree % adaptiveSampleSize);
            }
            else{
                (*useVector)[senderID] += (nextFree - curTick);
            }
        }
        else{
            if(data) unknownSenderCycles += (nextFree - curTick);
        }
        
        
    }
}


double
Bus::getAddressBusUtilisation(Tick sampleSize){
    
    int useIndex = -1;
    if(curTick % (2 * adaptiveSampleSize) == adaptiveSampleSize) useIndex = 0;
    else useIndex = 1;
    
    int sample = addrBusUseSamples[useIndex];
    addrBusUseSamples[useIndex] = 0;
    
    return (double) ((double) sample / (double) sampleSize);;
}

double
Bus::getDataBusUtilisation(Tick sampleSize){
    
    int useIndex = -1;
    if(curTick % (2 * adaptiveSampleSize) == adaptiveSampleSize) useIndex = 0;
    else useIndex = 1;
    
    int sample = dataBusUseSamples[useIndex];
    dataBusUseSamples[useIndex] = 0;
    
    return (double) ((double) sample / (double) sampleSize);
    
}

vector<int>
Bus::getDataUsePerCPUId(){
    
    vector<int> retval = perCPUDataBusUse;
    assert(perCPUDataBusUse.size() == perCPUDataBusUseOverflow.size());
    for(int i=0;i<perCPUDataBusUse.size();i++) perCPUDataBusUse[i] = perCPUDataBusUseOverflow[i];
    for(int i=0;i<perCPUDataBusUseOverflow.size();i++) perCPUDataBusUseOverflow[i] = 0;
    return retval;
}

vector<int>
Bus::getAddressUsePerCPUId(){
    vector<int> retval = perCPUAddressBusUse;
    assert(perCPUAddressBusUse.size() == perCPUAddressBusUseOverflow.size());
    for(int i=0;i<perCPUAddressBusUse.size();i++) perCPUAddressBusUse[i] = perCPUAddressBusUseOverflow[i];
    for(int i=0;i<perCPUAddressBusUseOverflow.size();i++) perCPUAddressBusUseOverflow[i] = 0;
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
    bus->arbitrateAddrBus();
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Bus)

    Param<int> width;
    Param<int> clock;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
    Param<bool> uniform_partitioning;

END_DECLARE_SIM_OBJECT_PARAMS(Bus)


BEGIN_INIT_SIM_OBJECT_PARAMS(Bus)

    INIT_PARAM(width, "bus width in bytes"),
    INIT_PARAM(clock, "bus frequency"),
    INIT_PARAM_DFLT(hier,
		    "Hierarchy global variables",
		    &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "Adaptive MHA object",NULL),
    INIT_PARAM_DFLT(uniform_partitioning, "Partition bus bandwidth uniformly between cores", false)

END_INIT_SIM_OBJECT_PARAMS(Bus)


CREATE_SIM_OBJECT(Bus)
{
    return new Bus(getInstanceName(), hier,
		   width, clock, adaptive_mha, uniform_partitioning);
}

REGISTER_SIM_OBJECT("Bus", Bus)

#endif //DOXYGEN_SHOULD_SKIP_THIS
