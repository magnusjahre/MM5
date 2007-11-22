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
 * This devices just panics when touched. For example if you have a 
 * kernel that touches the frame buffer which isn't allowed.
 */

#ifndef __DEV_BADDEV_HH__
#define __DEV_BADDEV_HH__

#include "base/range.hh"
#include "dev/io_device.hh"

/**
 * BadDevice
 * This device just panics when accessed. It is supposed to warn
 * the user that the kernel they are running has unsupported 
 * options (i.e. frame buffer)
 */
class BadDevice : public PioDevice
{
  private:
    Addr addr;
    static const Addr size = 0xf;

    std::string devname;

  public:
     /**
      * Constructor for the Baddev Class.
      * @param name name of the object
      * @param a base address of the write
      * @param mmu the memory controller
      * @param hier object to store parameters universal the device hierarchy
      * @param bus The bus that this device is attached to
      * @param devicename device that is not implemented
      */
    BadDevice(const std::string &name, Addr a, MemoryController *mmu, 
              HierParams *hier, Bus *bus, const std::string &devicename);

    /**
      * On a read event we just panic aand hopefully print a 
      * meaningful error message.
      * @param req Contains the address to read from.
      * @param data A pointer to write the read data to.
      * @return The fault condition of the access.
      */
    virtual Fault read(MemReqPtr &req, uint8_t *data);

    /**
      * On a write event we just panic aand hopefully print a 
      * meaningful error message.
      * @param req Contains the address to write to.
      * @param data The data to write.
      * @return The fault condition of the access.
      */
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    /**
     * Return how long this access will take.
     * @param req the memory request to calcuate
     * @return Tick when the request is done
     */
    Tick cacheAccess(MemReqPtr &req);
};

#endif // __DEV_BADDEV_HH__
