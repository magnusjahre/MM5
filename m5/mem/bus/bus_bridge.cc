/*
 * Copyright (c) 2003, 2004, 2005
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

/** @file
 * Definitions of a unidirectional bus bridge.
 */

#include "mem/bus/bus_bridge.hh"

#include "mem/bus/bus_bridge_slave.hh"
#include "mem/bus/bus_bridge_master.hh"
#include "mem/bus/bus.hh"
#include "sim/builder.hh"

using namespace std;

BusBridge::BusBridge(const string &name, BusBridge::Params params)
    : BaseHier(name, params.hier),  latency(params.latency), 
      max(params.max), inWidth(params.inWidth), inClock(params.inClock), 
      outWidth(params.outWidth), outClock(params.outClock), 
      ackWrites(params.ackWrites), ackDelay(params.ackDelay)
{
    slaveCount = 0;
    slaveBlocked = false;
    masterCount = 0;
    masterBlocked = false;
}

MemAccessResult 
BusBridge::slaveAccess(MemReqPtr &req)
{
    if (++slaveCount == max) {
	slaveBlocked = true;
	si->setBlocked();
    }

    Tick ready = curTick + latency;
    if (req->cmd.isWrite()) {
        // need to wait for the data to arrive.
        if (req->size > inWidth)
            ready += (req->size - inWidth) * inClock / inWidth;
    }

    if (ackWrites && req->cmd.isWrite()) {
        MemReqPtr newreq = new MemReq(*req);
        newreq->completionEvent = NULL;
        newreq->expectCompletionEvent = false;
        req->data = NULL;
        req->flags |= SATISFIED;
        DPRINTF(Bus, "Generating early Ack for addr %#x at cycle %d\n",
                req->paddr, curTick + ackDelay);
        si->respond(req, ackDelay + ready);
    
        slaveBuffer.push_back(newreq);
        slaveTimes.push_back(ready);
        mi->request(ready);
    } else {
        slaveBuffer.push_back(req);
        slaveTimes.push_back(ready);
        mi->request(ready);
        requests[(intptr_t)req.get()] = req->busId;
    }
    
    /** @todo Should change this from cache miss to something else. */
    return MA_CACHE_MISS;
}

MemAccessResult 
BusBridge::masterAccess(MemReqPtr &req)
{
    if (++masterCount == max) {
	masterBlocked = true;
	mi->setBlocked();
    }
    
    Tick ready = curTick + latency;
    if (req->cmd.isWrite()) {
        // need to wait for the data to arrive.
        if (req->size > inWidth)
            ready += (req->size - inWidth) * inClock / inWidth;
    }

    if (ackWrites && req->cmd.isWrite()) {
        MemReqPtr newreq = new MemReq(*req);
        req->flags |= SATISFIED;
        newreq->completionEvent = NULL;
        newreq->expectCompletionEvent = false;
        req->data = NULL;
        DPRINTF(Bus, "Generating early Ack for addr %#x at cycle %d\n",
                req->paddr, curTick + ackDelay);
        mi->respond(req, ackDelay + ready);

        masterBuffer.push_back(newreq);
        masterTimes.push_back(ready);
        si->request(ready);
    } else {
        masterBuffer.push_back(req);
        masterTimes.push_back(ready);
        si->request(ready);
        requests[(intptr_t)req.get()] = req->busId;
    }

    /** @todo Should change this from cache miss to something else. */  
    return MA_CACHE_MISS;
}

void 
BusBridge::sendSlaveResult(MemReqPtr &req, bool success)
{
    if (!success)
        return;

    assert(req == masterBuffer.front());
    if (masterTimes.front() >curTick) {
        warn("BusBridge:: Forwarding request %d cycles before its ready",
             masterTimes.front() - curTick);
    }
    masterBuffer.pop_front();
    masterTimes.pop_front();
    if (masterCount-- == max) {
        masterBlocked = false;
        mi->clearBlocked();
    }
    if (!masterTimes.empty() && masterTimes.front() > curTick) {
        // Need to rerequest the bus.
        si->request(masterTimes.front());
    }
}

void 
BusBridge::sendMasterResult(MemReqPtr &req, bool success)
{
    if (!success)
        return;

    assert(req == slaveBuffer.front());
    if (slaveTimes.front() >curTick) {
        warn("BusBridge:: Forwarding request %d cycles before its ready",
             slaveTimes.front() - curTick);
    }
    slaveBuffer.pop_front();
    slaveTimes.pop_front();
    if (slaveCount-- == max) {
        slaveBlocked = false;
        si->clearBlocked();
    }
    if (!slaveTimes.empty() && slaveTimes.front() > curTick) {
        // Need to rerequest the bus.
        mi->request(slaveTimes.front());
    }
}

void
BusBridge::handleSlaveResponse(MemReqPtr &req)
{
    if (ackWrites && req->cmd.isWrite())
        return;

    intptr_t key = (intptr_t)req.get();
    if (requests.count(key) != 1)
        DPRINTF(Bus, "count = %d\n", requests.count(key));
    assert(requests.count(key) == 1);
    req->busId = requests[key];
    requests.erase(key);
    Tick ready = curTick + latency;
    if (req->cmd.isRead()) {
        // need to wait for the data to arrive.
        if (req->size > outWidth)
            ready += (req->size - outWidth) * outClock / outWidth;
    }
    mi->respond(req, ready);
}

void
BusBridge::handleMasterResponse(MemReqPtr &req)
{
    if (ackWrites && req->cmd.isWrite())
        return;

    intptr_t key = (intptr_t)req.get();
    if (requests.count(key) != 1)
        cout << "count = " <<  requests.count(key) << endl;
    assert(requests.count(key) == 1);
    req->busId = requests[key];
    requests.erase(key);
    Tick ready = curTick + latency;
    if (req->cmd.isRead()) {
        // need to wait for the data to arrive.
        if (req->size > outWidth)
            ready += (req->size - outWidth) * outClock / outWidth;
    }

    si->respond(req, ready);
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(BusBridge)

    SimObjectParam<Bus*> in_bus;
    SimObjectParam<Bus*> out_bus;
    Param<int> max_buffer;
    Param<int> latency;
    Param<bool> ack_writes;
    Param<int> ack_delay;
    SimObjectParam<HierParams *> hier;

END_DECLARE_SIM_OBJECT_PARAMS(BusBridge)


BEGIN_INIT_SIM_OBJECT_PARAMS(BusBridge)

    INIT_PARAM(in_bus, "The bus to forward from"),
    INIT_PARAM(out_bus, "The bus to forward to"),
    INIT_PARAM(max_buffer, "The number of requests to buffer"),
    INIT_PARAM(latency, "The latency of this bridge"),
    INIT_PARAM(ack_writes, "Should this bridge ack writes directly?"),
    INIT_PARAM(ack_delay,
               "The latency to ack a write if this bridge is acking writes"),
    INIT_PARAM_DFLT(hier, "Hierarchy global variables", &defaultHierParams)

END_INIT_SIM_OBJECT_PARAMS(BusBridge)


CREATE_SIM_OBJECT(BusBridge)
{
    BusBridge::Params params;
    params.max = max_buffer;
    params.latency = latency;
    params.ackWrites = ack_writes;
    params.ackDelay = ack_delay;
    params.hier = hier;
    params.inWidth = in_bus->width;
    params.inClock = in_bus->clockRate;
    params.outWidth = out_bus->width;
    params.outClock = out_bus->clockRate;

    string name = getInstanceName();
    
    BusBridge *retval = new BusBridge(name, params);
    
    retval->setInterfaces(new BusBridgeSlave(name, hier, in_bus, retval),
			  new BusBridgeMaster(name, hier, out_bus, retval));
    return retval;
}

REGISTER_SIM_OBJECT("BusBridge", BusBridge)

#endif //DOXYGEN_SHOULD_SKIP_THIS
