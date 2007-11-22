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

/*
 * @file
 * PCI Config space implementation.
 */

#ifndef __PCICONFIGALL_HH__
#define __PCICONFIGALL_HH__

#include "dev/pcireg.h"
#include "base/range.hh"
#include "dev/io_device.hh"


static const uint32_t MAX_PCI_DEV = 32;
static const uint32_t MAX_PCI_FUNC = 8;

class PciDev;
class MemoryController;

/**
 * PCI Config Space
 * All of PCI config space needs to return -1 on Tsunami, except
 * the devices that exist. This device maps the entire bus config
 * space and passes the requests on to TsunamiPCIDev devices as 
 * appropriate.
 */
class PciConfigAll : public PioDevice
{
  private:
    Addr addr;
    static const Addr size = 0xffffff;

    /**
      * Pointers to all the devices that are registered with this
      * particular config space.
      */
    PciDev* devices[MAX_PCI_DEV][MAX_PCI_FUNC];
    
  public:
    /**
     * Constructor for PCIConfigAll 
     * @param name name of the object
     * @param a base address of the write
     * @param mmu the memory controller
     * @param hier object to store parameters universal the device hierarchy
     * @param bus The bus that this device is attached to
     */
    PciConfigAll(const std::string &name, Addr a, MemoryController *mmu,
                 HierParams *hier, Bus *bus, Tick pio_latency);


    /**
     * Check if a device exists.
     * @param pcidev PCI device to check
     * @param pcifunc PCI function to check
     * @return true if device exists, false otherwise
     */
    bool deviceExists(uint32_t pcidev, uint32_t pcifunc) 
                     { return devices[pcidev][pcifunc] != NULL ? true : false; }
    
    /**
     * Registers a device with the config space object.
     * @param pcidev PCI device to register
     * @param pcifunc PCI function to register
     * @param device device to register
     */
    void registerDevice(uint8_t pcidev, uint8_t pcifunc, PciDev *device)
                        { devices[pcidev][pcifunc] = device; }
    
    /**
     * Read something in PCI config space. If the device does not exist
     * -1 is returned, if the device does exist its PciDev::ReadConfig (or the
     * virtual function that overrides) it is called.
     * @param req Contains the address of the field to read.
     * @param data Return the field read.
     * @return The fault condition of the access.
     */
    virtual Fault read(MemReqPtr &req, uint8_t *data);
    
    /**
     * Write to PCI config spcae. If the device does not exit the simulator
     * panics. If it does it is passed on the PciDev::WriteConfig (or the virtual
     * function that overrides it).
     * @param req Contains the address to write to.
     * @param data The data to write.
     * @return The fault condition of the access.
     */
 
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    /**
     * Start up function to check if more than one person is using an interrupt line
     * and print a warning if such a case exists 
     */
    virtual void startup();

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

#endif // __PCICONFIGALL_HH__
