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
 * Declaration of a fake device.
 */

#ifndef __ISA_FAKE_HH__
#define __ISA_FAKE_HH__

#include "dev/tsunami.hh"
#include "base/range.hh"
#include "dev/io_device.hh"

/**
 * IsaFake is a device that returns -1 on all reads and 
 * accepts all writes. It is meant to be placed at an address range
 * so that an mcheck doesn't occur when an os probes a piece of hw
 * that doesn't exist (e.g. UARTs1-3).
 */
class IsaFake : public PioDevice
{
  private:
    /** The address in memory that we respond to */
    Addr addr;

  public:
    /**
      * The constructor for Tsunmami Fake just registers itself with the MMU.
      * @param name name of this device.
      * @param a address to respond to.
      * @param mmu the mmu we register with.
      * @param size number of addresses to respond to
      */
    IsaFake(const std::string &name, Addr a, MemoryController *mmu, 
                HierParams *hier, Bus *bus, Addr size = 0x8);

    /**
     * This read always returns -1.
     * @param req The memory request.
     * @param data Where to put the data.
     */
    virtual Fault read(MemReqPtr &req, uint8_t *data);
    
    /**
     * All writes are simply ignored.
     * @param req The memory request.
     * @param data the data to not write.
     */
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    /**
     * Return how long this access will take.
     * @param req the memory request to calcuate
     * @return Tick when the request is done
     */
    Tick cacheAccess(MemReqPtr &req);
};

#endif // __ISA_FAKE_HH__
