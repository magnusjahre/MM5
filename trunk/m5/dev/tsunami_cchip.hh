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
 * Emulation of the Tsunami CChip CSRs
 */

#ifndef __TSUNAMI_CCHIP_HH__
#define __TSUNAMI_CCHIP_HH__

#include "dev/tsunami.hh"
#include "base/range.hh"
#include "dev/io_device.hh"

class MemoryController;

/**
 * Tsunami CChip CSR Emulation. This device includes all the interrupt
 * handling code for the chipset.
 */
class TsunamiCChip : public PioDevice
{
  private:
    /** The base address of this device */
    Addr addr;

    /** The size of mappad from the above address */
    static const Addr size = 0xfffffff;

  protected:
    /**
     * pointer to the tsunami object.
     * This is our access to all the other tsunami
     * devices.
     */
    Tsunami *tsunami;

    /**
     * The dims are device interrupt mask registers.
     * One exists for each CPU, the DRIR X DIM = DIR
     */
    uint64_t dim[Tsunami::Max_CPUs];
    
    /**
     * The dirs are device interrupt registers.
     * One exists for each CPU, the DRIR X DIM = DIR
     */
    uint64_t dir[Tsunami::Max_CPUs];
    
    /**
     * This register contains bits for each PCI interrupt
     * that can occur.
     */
    uint64_t drir;

    /** Indicator of which CPUs have an IPI interrupt */
    uint64_t ipint;

    /** Indicator of which CPUs have an RTC interrupt */
    uint64_t itint;

  public:
    /**
     * Initialize the Tsunami CChip by setting all of the 
     * device register to 0.
     * @param name name of this device.
     * @param t pointer back to the Tsunami object that we belong to.
     * @param a address we are mapped at.
     * @param mmu pointer to the memory controller that sends us events.
     * @param hier object to store parameters universal the device hierarchy
     * @param bus The bus that this device is attached to
     */
    TsunamiCChip(const std::string &name, Tsunami *t, Addr a, 
                 MemoryController *mmu, HierParams *hier, Bus *bus,
		 Tick pio_latency);

    /**
      * Process a read to the CChip.
      * @param req Contains the address to read from.
      * @param data A pointer to write the read data to.
      * @return The fault condition of the access.
      */
    virtual Fault read(MemReqPtr &req, uint8_t *data);
    
    
    /**
      * Process a write to the CChip.
      * @param req Contains the address to write to.
      * @param data The data to write.
      * @return The fault condition of the access.
      */
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    /**
     * post an RTC interrupt to the CPU
     */
    void postRTC();
    
    /**
     * post an interrupt to the CPU.
     * @param interrupt the interrupt number to post (0-64)
     */
    void postDRIR(uint32_t interrupt);
    
    /**
     * clear an interrupt previously posted to the CPU.
     * @param interrupt the interrupt number to post (0-64)
     */
    void clearDRIR(uint32_t interrupt);

    /**
     * post an ipi interrupt  to the CPU.
     * @param ipintr the cpu number to clear(bitvector) 
     */
    void clearIPI(uint64_t ipintr);

    /**
     * clear a timer interrupt previously posted to the CPU.
     * @param itintr the cpu number to clear(bitvector)
     */
    void clearITI(uint64_t itintr);

    /**
     * request an interrupt be posted to the CPU.
     * @param ipreq the cpu number to interrupt(bitvector)
     */
    void reqIPI(uint64_t ipreq);


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

#endif // __TSUNAMI_CCHIP_HH__
