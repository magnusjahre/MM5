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

/** @file
 * Tsunami PCI interface CSRs
 */

#ifndef __TSUNAMI_PCHIP_HH__
#define __TSUNAMI_PCHIP_HH__

#include "dev/tsunami.hh"
#include "base/range.hh"
#include "dev/io_device.hh"

class MemoryController;

/**
 * A very simple implementation of the Tsunami PCI interface chips.
 */
class TsunamiPChip : public PioDevice
{
  private:
    /** The base address of this device */
    Addr addr;
    
    /** The size of mappad from the above address */
    static const Addr size = 0xfff;

  protected:
    /**
     * pointer to the tsunami object.
     * This is our access to all the other tsunami
     * devices.
     */
    Tsunami *tsunami;

    /** Pchip control register */
    uint64_t pctl;

    /** Window Base addresses */
    uint64_t wsba[4];

    /** Window masks */
    uint64_t wsm[4];

    /** Translated Base Addresses */
    uint64_t tba[4];

  public:
    /**
     * Register the PChip with the mmu and init all wsba, wsm, and tba to 0 
     * @param name the name of thes device
     * @param t a pointer to the tsunami device
     * @param a the address which we respond to
     * @param mmu the mmu we are to register with
     * @param hier object to store parameters universal the device hierarchy
     * @param bus The bus that this device is attached to
     */
    TsunamiPChip(const std::string &name, Tsunami *t, Addr a, 
                 MemoryController *mmu, HierParams *hier, Bus *bus,
		 Tick pio_latency);

    /**
     * Translate a PCI bus address to a memory address for DMA.
     * @todo Andrew says this needs to be fixed. What's wrong with it?
     * @param busAddr PCI address to translate.
     * @return memory system address
     */
    Addr translatePciToDma(Addr busAddr);

     /**
      * Process a read to the PChip.
      * @param req Contains the address to read from.
      * @param data A pointer to write the read data to.
      * @return The fault condition of the access.
      */
    virtual Fault read(MemReqPtr &req, uint8_t *data);

    /**
      * Process a write to the PChip.
      * @param req Contains the address to write to.
      * @param data The data to write.
      * @return The fault condition of the access.
      */
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    /**
     * Serialize this object to the given output stream.
     * @param os The stream to serialize to.
     */
    virtual void serialize(std::ostream &os);
    
    /**
     * Reconstruct the state of this object from a checkpoint.
     * @param cp The checkpoint use.
     * @param section The section name of this object
     */
    virtual void unserialize(Checkpoint *cp, const std::string &section);

    /**
     * Return how long this access will take.
     * @param req the memory request to calcuate
     * @return Tick when the request is done
     */
    Tick cacheAccess(MemReqPtr &req);
};

#endif // __TSUNAMI_PCHIP_HH__
