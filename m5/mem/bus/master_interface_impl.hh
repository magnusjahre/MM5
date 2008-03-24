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

/**
 * @file
 * Definitions for the bus master interface.
 */
#include <algorithm>
#include <string>

#include "base/intmath.hh"
#include "mem/bus/master_interface.hh"

using namespace std;

template<class MemType, class BusType>
MasterInterface<MemType, BusType>::MasterInterface(const string &name,
						   HierParams *hier,
						   MemType *_mem,
						   BusType *bus)
    : BusInterface<BusType>(name, hier, bus, true), mem(_mem)
{
}

template<class MemType, class BusType>
bool
MasterInterface<MemType, BusType>::grantAddr()
{
    MemReqPtr req = mem->getMemReq();
    if (req) {
        req->busId = this->id;
    }
    bool successful = this->bus->sendAddr(req, curTick);
    mem->sendResult(req, successful);
    return mem->doMasterRequest();
}

template<class MemType, class BusType>
void
MasterInterface<MemType, BusType>::deliver(MemReqPtr &req)
{
    if (curTick - req->time > 10000) {
      cout << curTick << " : Request for " << req->paddr << " took " << curTick - req->time << endl;
    }
    mem->handleResponse(req);
}


template<class MemType, class BusType>
MemAccessResult
MasterInterface<MemType, BusType>::access(MemReqPtr &req)
{
    int satisfied_before = req->flags & SATISFIED;
    // Cache Coherence call goes here
//     cout << "access from MasterInterface " << this->id << "\n";
    
    mem->snoop(req);
    
    if (satisfied_before != (req->flags & SATISFIED)) {
	return BA_SUCCESS;
    }
    return BA_NO_RESULT;
}

template<class Mem, class Bus>
void
MasterInterface<Mem, Bus>::snoopResponseCall(MemReqPtr &req)
{
    mem->snoopResponse(req);
}

/*
 * Called when granted the data bus, forwards a response to the Bus
 */
template<class MemType, class BusType>
bool
MasterInterface<MemType, BusType>::grantData()
{	
    typename BusInterface<BusType>::DataResponseEntry entry = 
	this->responseQueue.front();

    if (entry.size > 0) {
	this->bus->delayData(entry.size, entry.senderID, entry.cmd);
    } else {
	if (entry.req->cmd.isWrite()){
	    this->bus->sendAck(entry.req, entry.time);
	} else {
	    this->bus->sendData(entry.req, entry.time);
	}
	DPRINTF(Bus, "Sending the master-side response to blk_addr: %x\n",
		entry.req->paddr & (((ULL(1))<<48)-1));
    }
    this->responseQueue.pop_front();
    if (!this->responseQueue.empty()) {
	this->bus->requestDataBus(
            this->id, max(curTick, this->responseQueue.front().time));
    }
    
    return false;
}

/*
 * Called by attached TimingMemObj when a request is satisfied
 */
template<class MemType, class BusType>
void
MasterInterface<MemType, BusType>::respond(MemReqPtr &req, Tick time)
{
    assert(req->cmd == Read || req->cmd == ReadEx);
    DPRINTF(Bus, "Queueing a master-side response to blk_addr: %x\n",
	    req->paddr & (((ULL(1))<<48)-1));
    typename BusInterface<BusType>::DataResponseEntry dre(req,time);
    this->responseQueue.push_back(dre);
    this->bus->requestDataBus(this->id, time);
}

template<class MemType, class BusType>
void 
MasterInterface<MemType, BusType>::addPrewrite(MemReqPtr &req)
{
    // DEtte kommer vi aldri til aa huske :p
    this->bus->sendAddr(req,curTick+3);
}

template<class MemType, class BusType>
bool
MasterInterface<MemType, BusType>::canPrewrite()
{
   return (this->bus->memoryController->isPrewriteBlocked());
}
