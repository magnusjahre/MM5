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
 * Definitions of PioInterface class.
 */

#include <string>

#include "mem/bus/pio_interface.hh"

using namespace std;

template <class BusType, class Device>
PioInterface<BusType, Device>::
PioInterface(const std::string &name, HierParams *hier, BusType *bus,
	     Device *dev, Function func)

    : BusInterface<BusType>(name, hier, bus, false), device(dev),
      function(func)
{
}

template <class BusType, class Device>
MemAccessResult
PioInterface<BusType, Device>::access(MemReqPtr &req)
{
    if (this->inRange(req->paddr)) {
	Tick time = (device->*function)(req);
	req->flags |= SATISFIED;

	if (time > 0)
	    respond(req, time);
	/* else the device will respond later */

	return BA_SUCCESS;
    }

    return BA_NO_RESULT;
}

/*
 * Called when granted the data bus, forwards a response to the Bus
 */
template <class BusType, class Device>
bool
PioInterface<BusType, Device>::grantData()
{	
    typename BusInterface<BusType>::DataResponseEntry entry = 
	this->responseQueue.front();

    if (entry.size > 0) {
	this->bus->delayData(entry.size);
    } else {
	if (entry.req->cmd.isWrite()){
	    this->bus->sendAck(entry.req, entry.time);
	} else {
	    this->bus->sendData(entry.req, entry.time);
	}
    }
    this->responseQueue.pop_front();
    if (!this->responseQueue.empty()) {
	this->bus->requestDataBus(this->id, max(curTick, 
                                          this->responseQueue.front().time));
    }
    
    return false;
}

/*
 * Called by attached TimingMemObj when a request is satisfied
 */
template <class BusType, class Device>
void
PioInterface<BusType, Device>::respond(MemReqPtr &req, Tick time)
{
    if (!req->cmd.isNoResponse()) {
        typename BusInterface<BusType>::DataResponseEntry dre(req,time);
	this->responseQueue.push_back(dre);
	this->bus->requestDataBus(this->id, time);
    }
}


template <class BusType, class Device>
Tick
PioInterface<BusType, Device>::probe(MemReqPtr &req, bool update)
{
    fatal("Unimplemented\n");
    return 0;
}
