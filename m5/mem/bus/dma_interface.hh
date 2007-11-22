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
 * Declaration of a wrapper for DMAs to the memory hierarchy.
 */

#ifndef __MEM_BUS_DMA_INTERFACE_HH__
#define __MEM_BUS_DMA_INTERFACE_HH__

#include <list>

#include "base/range.hh"
#include "base/statistics.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"

template <class BusType> class DMABusInterface;

/**
 * A wrapper interface for introducing DMA into the memory hierarchy.
 */
template <class BusType>
class DMAInterface : public SimObject
{
  public:
    /**
     * @addtogroup DMA Transfer Statistics.
     * @{
     */
    /** The total number of DMA transactions. */
    Stats::Formula totalTransactions;
    /** The number of transactions <= blkSize. */
    Stats::Scalar<> totalSmallTransactions;
    /** The number of transactions > blkSize. */
    Stats::Scalar<> totalLargeTransactions;
    /** The number of DMA packets sent on the bus. */
    Stats::Scalar<> totalPacketsSent;
    /** The total cycle latency seen by the DMA packets. */
    Stats::Scalar<> totalPacketLatency;
    /** The total number of bytes sent. */
    Stats::Scalar<> totalBytesSent;
    /** The total cycle latency seen by each DMA transaction. */
    Stats::Formula totalTransactionLatency;
    /** The total cycle latency of small transactions. */
    Stats::Scalar<> totalSmallTransactionLatency;
    /** The total cycle latency of large transactions. */
    Stats::Scalar<> totalLargeTransactionLatency;
    /** The average latency for each packet. */
    Stats::Formula avgPacketLatency;
    /** The average latency for each transaction. */
    Stats::Formula avgTransactionLatency;
    /** The average latency for each small transaction. */
    Stats::Formula avgSmallTransactionLatency;
    /** The average latency for each large transaction. */
    Stats::Formula avgLargeTransactionLatency;

    /**
     * @}
     */

    /**
     * Constructs and initializes this interface.
     * @param name The name of this interface.
     * @param header_bus The bus to connect to and send headers to.
     * @param payload_bus The bus to connect to and send payloads to.
     * @param header_size The number of cacheblocks to send to the header bus.
     */
    DMAInterface(const std::string &name, BusType *header_bus, 
		 BusType *payload_bus, int header_size, bool dma_no_allocate);

    /**
     * Destructor.
     */
    virtual ~DMAInterface();

    /**
     * Returns true if the previous DMA hasn't completed.
     * @return True if this interface is busy.
     */
    bool busy() const
    {
	return completionEvent != NULL;
    }

    /**
     * Called by the interfaces when they complete, schedules completion event.
     */
    void done()
    {
	if (--count == 0) {
	    if (smallTransaction) {
		totalSmallTransactionLatency += curTick - transactionStart;
	    } else {
		totalLargeTransactionLatency += curTick - transactionStart;
	    }
            /** @todo Remove this, temp to verify statistics */
	    completionEvent->schedule(curTick + 1);
	    completionEvent = NULL;
	}
    }

    /**
     * Register statistics.
     */
    void regStats();

    /**
     * Start a DMA.
     * @param cmd The DMA command to perform.
     * @param addr The starting address.
     * @param size The number of bytes in the transfer.
     * @param start The start time of the transfer.
     * @param event The event to schedule on completion.
     * @param data The data of the transfer.
     */
    void doDMA(MemCmd cmd, Addr addr, int size, Tick start,
	       Event *event, bool nic_req = false, uint8_t *data = NULL);

    /**
     * Returns the size of the transfers.
     * @return The transfer size.
     */
    int blockSize() const { return blkSize; }

    /**
     * Set the address ranges of this interface to the list provided. This 
     * function removes any existing ranges.
     * @param range_list List of addr ranges to add.
     * @post range_list is empty.
     */
    void setAddrRange(std::list<Range<Addr> > &range_list)
    {
	if (header) {
	    header->setAddrRange(range_list);
	}
	if (payload) {
	    payload->setAddrRange(range_list);
	}
    }

    /**
     * Add an address range for this interface.
     * @param range The addres range to add.
     */
    void addAddrRange(const Range<Addr> &range)
    {
	if (header) {
	    header->addAddrRange(range);
	}
	if (payload) {
	    payload->addAddrRange(range);
	}
    }

  private:
    /** Size of header to be split in number of cache blocks. */
    const int headerSize;
    /** 
     * Number of times done() should be called before the DMA completes.
     * This is 1 for no header splitting and generaly 2 if we do split headers.
     */
    int count;
    /** Event to call when transfer is complete. */
    Event *completionEvent;
    
    /**
     * Transmit data in the hierarchy?
     * @todo Should get this from the hierarchy params.
     */
    bool doData;

    /** Bus Interface for the header portion of the DMA. */
    DMABusInterface<BusType> *header;
    /** Bus Interface for the payload portion of the DMA. */
    DMABusInterface<BusType> *payload;
    
    /** 
     * Bool that indicates if we should do header splitting (different bus's).
     */
    bool headerSplit;

    /** The transfer size. */
    static const int blkSize = 64;
    /** The address mask for the transfer size. */
    static const Addr blkMask = (Addr) blkSize-1;

    /** The cycle the current transaction started. */
    Tick transactionStart;
    /** True if the current transaction is <= blkSize. */
    bool smallTransaction;
    
    /**
     * Align an address to the transfer size.
     * @param addr The address to align.
     * @return The aligned address.
     */
    Addr blkAlign(Addr addr) const
    {
	return (addr & ~blkMask);
    }

    /**
     * Calculate the offset of an address relative to the transfer size.
     * @param addr The address to get the block offset.
     * @return The offset of the address into the transfer block.
     */
    int blkOffset(Addr addr) const
    {
	return (int)(addr & blkMask);
    }
};

#endif //__MEM_BUS_DMA_INTERFACE_HH__
