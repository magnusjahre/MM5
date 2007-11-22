/*
 * Copyright (c) 2004, 2005
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
 * Definition of the slave interface for a BusBridge.
 */

#include "mem/bus/bus_bridge_slave.hh"
#include "mem/bus/bus_bridge.hh"

using namespace std;

BusBridgeSlave::BusBridgeSlave(const std::string &name, HierParams *hier, 
			       Bus *bus, BusBridge *bridge)
    : BusInterface<Bus>(name, hier, bus, false), bb(bridge)
{
    inCollectRanges = false;
    changingRange = false;
}

MemAccessResult
BusBridgeSlave::access(MemReqPtr &req)
{
    int retval;
    if (inRange(req->paddr)) {

        if (isBlocked()) {
	    panic("We are blocked, we shouldn't get a request\n");
	}

	if (!req->isSatisfied()) {
	    retval = bb->slaveAccess(req);
	}
	
	assert(!isBlocked() || bb->isSlaveBlocked());
	if (isBlocked()) return BA_BLOCKED; // We have now become blocked
	else return BA_SUCCESS;  // This transaction went through ok
    }
    return BA_NO_RESULT;
}

void
BusBridgeSlave::respond(MemReqPtr &req, Tick time)
{
    if (!req->cmd.isNoResponse()) {
	responseQueue.push_back(DataResponseEntry(req, time));
	bus->requestDataBus(id, time);
    }
}

bool
BusBridgeSlave::grantAddr()
{
    MemReqPtr req = bb->getSlaveReq();
    Tick req_time = curTick;
    if (req) {
	req->busId = id;
	req_time = req->time;
	if (req->cmd.isWrite()) {
	    responseQueue.push_back(DataResponseEntry(req->size, curTick));
	    bus->requestDataBus(id, curTick);
	}
    }
    bool successful = bus->sendAddr(req, req_time);
    bb->sendSlaveResult(req, successful);
    
    return bb->doSlaveRequest();
}


bool
BusBridgeSlave::grantData()
{  
    DataResponseEntry entry = responseQueue.front();

    if (entry.size > 0) {
	bus->delayData(entry.size);
    } else {
	if (entry.req->cmd.isWrite()){
	    bus->sendAck(entry.req, entry.time);
	} else {
	    bus->sendData(entry.req, entry.time);
	}
    }
    responseQueue.pop_front();
    if (!responseQueue.empty()) {
	bus->requestDataBus(id, max(curTick, responseQueue.front().time));
    }
    
    return false;
}

void
BusBridgeSlave::deliver(MemReqPtr &req)
{
    bb->handleSlaveResponse(req);
}

Tick
BusBridgeSlave::probe(MemReqPtr &req, bool update)
{
    return bb->slaveProbe(req, update);
}

void
BusBridgeSlave::collectRanges(list<Range<Addr> > &range_list)
{
    inCollectRanges = true;
    BusInterface<Bus>::collectRanges(range_list);
    inCollectRanges = false;
}

void
BusBridgeSlave::getRange(list<Range<Addr> > &range_list)
{
    if (!inCollectRanges) {
	for (int i = 0; i < ranges.size(); ++i) {
	    range_list.push_back(ranges[i]);
	}
    }
}

void
BusBridgeSlave::rangeChange()
{
    if (!changingRange) {
	bb->slaveRangeChange();
    }
}

void
BusBridgeSlave::setAddrRange(list<Range<Addr> > &range_list)
{
    BaseInterface::setAddrRange(range_list);
    changingRange = true;
    bus->rangeChange();
    changingRange = false;
}

void
BusBridgeSlave::addAddrRange(const Range<Addr> &range)
{
    BaseInterface::addAddrRange(range);
    changingRange = true;
    bus->rangeChange();
    changingRange = false;
}
