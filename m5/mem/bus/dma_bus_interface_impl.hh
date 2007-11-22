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
 * Definitions of a DMA master bus interface.
 */

#include <algorithm>

#include "mem/bus/dma_bus_interface.hh"
#include "mem/bus/dma_interface.hh"

using namespace std;

template<class BusType>
DMABusInterface<BusType>::DMABusInterface(const std::string &name, Bus *bus,
					  DMAInterface<BusType> *_dma, 
					  int blk_size, bool dma_no_allocate)
    : BusInterface<BusType>(name, NULL, bus, true), dma(_dma), 
      blkSize(blk_size), blkMask(blkSize-1), dmaNoAllocate(dma_no_allocate)
{
}
				
template <class BusType>
void
DMABusInterface<BusType>::doDMA(MemCmd cmd, Addr addr, int size, Tick start,
				uint8_t *_data, bool _nic_req)
{
    if (cmd.isRead())
	dmaCmd = Read;
    else if (cmd.isWrite())
	dmaCmd = WriteInvalidate;
    else
	fatal("Invalid DMA command");

    startAddr = addr; // address of first byte of transfer range
    endAddr = addr + size; // address of byte after transfer range
    currentBlk = blkAlign(startAddr); //address of first blk to send
    data = _data;
    dataIndex = 0;
    nic_req = _nic_req;

    Addr start_blk = blkAlign(startAddr);
    Addr end_blk = blkAlign(endAddr + blkMask);
    // calculate total number of memory transfers
    numPackets = (end_blk - start_blk) / blkSize;
    packetsSent = 0;
    packetsReceived = 0;
    
    this->bus->requestAddrBus(this->id, start);
}

template <class BusType>
bool
DMABusInterface<BusType>::grantAddr()
{
    MemReqPtr req = new MemReq();
    req->cmd = dmaCmd;
    req->paddr = currentBlk;
    req->busId = this->id;
    req->nic_req = nic_req;

    if (dmaNoAllocate && dmaCmd.isRead()) {
	req->flags |= NO_ALLOCATE;
    }

    if (currentBlk < startAddr) {
	req->offset = blkOffset(startAddr);
    } else {
	req->offset = 0;
    }

    if (packetsSent == 0) {
	// First block
	req->size = currentBlk + blkSize - startAddr;
    } else if (packetsSent == numPackets - 1) {
	// last block
	req->size = endAddr - currentBlk;
    } else {
        req->size = blkSize;
    }

    if (nic_req) 
	DPRINTF(DMA, "DMA Bus Interface grantAddr to NIC req: blk=%#x, offset=%d size=%d\n",
		req->paddr, req->offset, req->size);
    else
	DPRINTF(DMA, "DMA Bus Interface grantAddr blk=%#x, offset=%d size=%d\n",
		req->paddr, req->offset, req->size);

    if (doData) {
        // copy data here
    }

    // EGH fix me: figure out how to calculate queueing delay.
    req->time = curTick;
    bool success = this->bus->sendAddr(req, curTick);
    if (success) {
	if (dmaCmd.isWrite()) {
	    typename BusInterface<BusType>::DataResponseEntry 
                dre(req->size, curTick);
	    this->responseQueue.push_back(dre);
	    this->bus->requestDataBus(req->busId, curTick);
	}
	packetsSent++;
	
	++dma->totalPacketsSent;
	dma->totalBytesSent += req->size;
	currentBlk += blkSize;
    }
    // If unsucessful, or still more to go return
    // true to signal we need the bus some more.
    return (packetsSent < numPackets);
}

template <class BusType>
bool
DMABusInterface<BusType>::grantData()
{	
    typename BusInterface<BusType>::DataResponseEntry entry =
	this->responseQueue.front();

    if (entry.size > 0) {
	DPRINTF(DMA, "DMA Bus Interface grantData size=%d\n",
		entry.size);
	this->bus->delayData(entry.size);
    } else {
	fatal("DMA interface doesn't provide responses");
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


template <class BusType>
void
DMABusInterface<BusType>::deliver(MemReqPtr &req)
{
    packetsReceived++;
    dma->totalPacketLatency += curTick - req->time;
    DPRINTF(DMA, "DMA Interface deliver received=%d, number=%d\n",
	    packetsReceived, numPackets);
    if (packetsReceived == numPackets) {
	dma->done();
    }
}
