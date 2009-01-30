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
 * Definition of a templatize slave interface.
 */

#include <algorithm>
#include <string>

#include "base/intmath.hh"
#include "mem/bus/slave_interface.hh"
#include "mem/trace/mem_trace_writer.hh"

/** @todo remove stats include once nate fixes bug */
#include "base/statistics.hh"

using namespace std;

template <class MemType, class BusType>
SlaveInterface<MemType, BusType>::SlaveInterface(const string &name,
						 HierParams *hier,
						 MemType *_mem,
						 BusType *bus,
						 MemTraceWriter *mem_trace,
                                                 bool isShadow)
    : BusInterface<BusType>(name, hier, bus, false, isShadow), mem(_mem),
      memTrace(mem_trace)
{
}

/*
 * Calls access on the associated TimingMemObj
 */
template <class MemType, class BusType>
MemAccessResult
SlaveInterface<MemType, BusType>::access(MemReqPtr &req)
{
    int retval;
    bool already_satisfied = req->isSatisfied();
    
   
    assert(!already_satisfied);

    if (this->inRange(req->paddr)) {
        if (this->isBlocked()) {
	    //We now signal blocked when we use the last MSHR, see
	    //the return values lower
	    panic("We are blocked, we shouldn't get a request\n");
	}
	if (memTrace) {
	    memTrace->writeReq(req);
	}

	retval = mem->access(req);

	assert(!this->isBlocked() || mem->isBlocked());

	if (this->isBlocked()) {
            return BA_BLOCKED; //Out of MSHRS, now we block  
        } else {
            return BA_SUCCESS;// This transaction went through ok
        }
    }
    return BA_NO_RESULT;
}

/*
 * Called when granted the data bus, forwards a response to the Bus
 */
template <class MemType, class BusType>
bool
SlaveInterface<MemType, BusType>::grantData()
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
    }
    this->responseQueue.pop_front();
    if (!this->responseQueue.empty()) {
	this->bus->requestDataBus(this->id, max(curTick, this->responseQueue.front().time));
    }
    
    return false;
}

/*
 * Called by attached TimingMemObj when a request is satisfied
 */
template <class MemType, class BusType>
void
SlaveInterface<MemType, BusType>::respond(MemReqPtr &req, Tick time)
{
    this->bus->latencyCalculated(req, time, this->isShadow);
}

template <class MemType, class BusType>
bool
SlaveInterface<MemType, BusType>::grantAddr()
{
    // get coherence message here
    MemReqPtr req = mem->getCoherenceReq();
    if (req) {
	req->busId = this->id;
    }
    this->bus->sendAddr(req,req->time);
    return mem->doSlaveRequest();
}

template<class Mem, class Bus>
void
SlaveInterface<Mem, Bus>::snoopResponseCall(MemReqPtr &req)
{
    //We do nothing on coherence requests for now they are just invalidates
    //being passed up from a lower level (DMA)
}


template<class Mem, class Bus>
bool 
SlaveInterface<Mem, Bus>::isActive(MemReqPtr &req) 
{
  return mem->isActive(req);
}

template<class Mem, class Bus>
bool 
SlaveInterface<Mem, Bus>::bankIsClosed(MemReqPtr &req) 
{
  return mem->bankIsClosed(req);
}

template<class Mem, class Bus>
bool 
SlaveInterface<Mem, Bus>::isReady(MemReqPtr &req) 
{
  return mem->isReady(req);
}

template<class Mem, class Bus>
Tick
SlaveInterface<Mem, Bus>::calculateLatency(MemReqPtr &req)
{
  return mem->calculateLatency(req);
}

template<class Mem, class Bus>
Tick
SlaveInterface<Mem, Bus>::getDataTransTime(){
    return mem->getDataTransTime();
}

template<class Mem, class Bus>
int
SlaveInterface<Mem, Bus>::getPageSize(){
    return mem->getPageSize();
}

template<class Mem, class Bus>
int
SlaveInterface<Mem, Bus>::getMemoryBankID(Addr addr){
    return mem->getMemoryBankID(addr);
}

template<class Mem, class Bus>
int
SlaveInterface<Mem, Bus>::getMemoryBankCount(){
    return mem->getMemoryBankCount();
}

template<class Mem, class Bus>
Tick
SlaveInterface<Mem, Bus>::getBankActivatedAt(int bankID){
    return mem->getBankActivatedAt(bankID);
}
