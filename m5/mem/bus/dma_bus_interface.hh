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
 * Declaration of a DMA master bus interface.
 */

#ifndef __DMA_BUS_INTERFACE_HH__
#define __DMA_BUS_INTERFACE_HH__

#include <list>

#include "mem/bus/bus_interface.hh"
#include "base/range.hh"

template <class BusType> class DMAInterface;

template <class BusType>
class DMABusInterface : public BusInterface<BusType>
{
  public:
    DMABusInterface(const std::string &name, Bus *bus, 
		    DMAInterface<BusType> *_dma, int blk_size,
		    bool dma_no_allocate);
    
    /**
     * Setup/start a DMA transaction.
     * @param cmd The DMA command to perform.
     * @param addr The starting address.
     * @param size The number of bytes in the transfer.
     * @param start The start time of the transfer.
     * @param data The data of the transfer.
     */
    void doDMA(MemCmd cmd, Addr addr, int size, Tick start, uint8_t *data, bool nic_req = false);
    
    /**
     * Called when this interface gets the address bus, forwards the next
     * request to the bus.
     * @return True if a another request is outstanding.
     */
    virtual bool grantAddr();

    /**
     * Called when this interface gets the data bus.
     * @return True if another request is outstanding.
     */
    virtual bool grantData();

    /**
     * Deliver a response to the connected memory.
     * @param req The request being responded to.
     */
    virtual void deliver(MemReqPtr &req);

    /**
     * Notify this interface of a range change on the bus.
     */
    virtual void rangeChange() {}    

    /**
     * Set the address ranges of this interface to the list provided. This 
     * function removes any existing ranges.
     * @param range_list List of addr ranges to add.
     * @post range_list is empty.
     */
    virtual void setAddrRange(std::list<Range<Addr> > &range_list)
    {
	BaseInterface::setAddrRange(range_list);
	this->bus->rangeChange();
    }

    /**
     * Add an address range for this interface.
     * @param range The addres range to add.
     */
    virtual void addAddrRange(const Range<Addr> &range)
    {
	BaseInterface::addAddrRange(range);
	this->bus->rangeChange();
    }

  private:
    /** Pointer to the parent DMA wrapper interface. */
    DMAInterface<BusType> *dma;
    /** Tranfer block size. */
    const int blkSize;
    /** Block mask. */
    const Addr blkMask;

    /** The command of the current transfer. */
    MemCmd dmaCmd;
    /** The address of the section of the transfer on the bus. */
    Addr currentBlk;
    /** The starting address of the transfer. */
    Addr startAddr;
    /** The ending address of the transfer. */
    Addr endAddr;
    /** The number of packets sent in the current transfer. */
    int packetsSent;
    /** The number of packets received in the current transfer. */
    int packetsReceived;
    /** The total number of packets in this transfer. */
    int numPackets;

    /** Send data with the transfer. */
    bool doData;
    /** The data of the transfer. */
    uint8_t* data;
    /** The current data index. */
    int dataIndex;
    /** This came from the NIC or not */
    bool nic_req;

    bool dmaNoAllocate;

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
    
    virtual void snoopResponseCall(MemReqPtr &req)
    {
	//We do nothing on resquest responses for now
    }
};

#endif //__DMA_BUS_INTERFACE_HH__
