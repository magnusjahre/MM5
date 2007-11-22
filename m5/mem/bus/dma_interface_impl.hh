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
 * Definitions of a wrapper for introducing DMA to the memory hierarchy.
 */

#include "mem/bus/dma_interface.hh"
#include "mem/bus/dma_bus_interface.hh"
#include "base/trace.hh"

using namespace std;

template <class BusType>
DMAInterface<BusType>::DMAInterface(const std::string &name, 
				    BusType *header_bus, 
				    BusType *payload_bus, int header_size,
				    bool dma_no_allocate)
    : SimObject(name), headerSize(header_size), completionEvent(NULL),
      headerSplit(false)
{
    doData = false;
    if (header_bus != payload_bus) {
	header = new DMABusInterface<BusType>(name + ".header", header_bus, 
					      this, blkSize, dma_no_allocate);
	headerSplit = true;
    }
    payload = new DMABusInterface<BusType>(name + ".payload", payload_bus, 
					   this, blkSize, dma_no_allocate);
}

template <class BusType>
DMAInterface<BusType>::~DMAInterface()
{
    if (header)
	delete header;
    if (payload)
	delete payload;
}

template <class BusType>
void 
DMAInterface<BusType>::regStats() 
{
    using namespace Stats;
    totalSmallTransactions
	.name(name() + ".small_transactions")
	.desc("Total number of transactions less than the block size.")
	;
    totalLargeTransactions
	.name(name() + ".large_transactions")
	.desc("Total number of transactions greater than the block size.")
	;
    totalTransactions
	.name(name() + ".requests")
	.desc("Total number of dma requests made")
	;
    totalTransactions = totalSmallTransactions + totalLargeTransactions;
    
    totalPacketsSent
	.name(name() + ".packets_sent")
	.desc("number of packets sent(blocks)")
	;
    totalPacketLatency
	.name(name() + ".total_cycle_latency")
	.desc("sum of all latencies")
	;
    avgPacketLatency
	.name(name() + ".round_trip_latency")
	.desc("Average Round-Trip latency")
	;
    avgPacketLatency = totalPacketLatency / totalPacketsSent;
    
    totalBytesSent
	.name(name() + ".bytes_transfered")
	.desc("Total bytes sent")
	;

    totalSmallTransactionLatency
	.name(name() + ".small_transaction_latency")
	.desc("The total cycle latency of all small transactions.")
	;
    totalLargeTransactionLatency
	.name(name() + ".large_transaction_latency")
	.desc("The total cycle latency of all large transactions.")
	;
    totalTransactionLatency
	.name(name() + ".transaction_latency")
	.desc("The total cycle latency of all transactions.")
	;
    totalTransactionLatency = totalSmallTransactionLatency + 
	totalLargeTransactionLatency;
    
    avgSmallTransactionLatency
	.name(name() + ".avg_small_transaction_latency")
	.desc("The average latency of small transactions.")
	;
    avgSmallTransactionLatency = totalSmallTransactionLatency / 
	totalSmallTransactions;

    avgLargeTransactionLatency
	.name(name() + ".avg_large_transaction_latency")
	.desc("The average latency of large transactions.")
	;
    avgLargeTransactionLatency = totalLargeTransactionLatency / 
	totalLargeTransactions;

    avgTransactionLatency
	.name(name() + ".avg_transaction_latency")
	.desc("The average latency of all transactions.")
	;
    avgTransactionLatency = totalTransactionLatency / 
	totalTransactions;
    
}

template <class BusType>
void
DMAInterface<BusType>::doDMA(MemCmd cmd, Addr addr, int size, Tick start,
		    Event *event, bool nic_req, uint8_t *data)
{
    assert(!busy());

    completionEvent = event; // event to schedule when completed transfer

    transactionStart = start;
    smallTransaction = size <= blkSize;

    DPRINTF(DMA,
	    "DMA Interface do DMA addr=%#x, size=%d, start=%+d\n",
	    addr, size, start - curTick);

    if (smallTransaction) {
	++totalSmallTransactions;
    } else {
	++totalLargeTransactions;
    }
    count = 1;
    int header_size = 0;
    if (headerSplit) {
	header_size = headerSize * blkSize;
	if (size <= header_size) {
	    // just send the whole transfer to the header interface.
	    header->doDMA(cmd, addr, size, start, data, nic_req);
	    return;
	}
	// do not send the last partial transfer block
	header_size = header_size - blkOffset(addr);
	header->doDMA(cmd, addr, header_size, start, data, nic_req);
	count = 2;
    }
    // We can't get here unless we have remaining bytes to send
    assert(size - header_size > 0);
    
    payload->doDMA(cmd, addr + header_size, size - header_size, start, 
		   data + header_size, nic_req);
}

