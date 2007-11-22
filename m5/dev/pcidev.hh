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

/* @file
 * Interface for devices using PCI configuration
 */

#ifndef __DEV_PCIDEV_HH__
#define __DEV_PCIDEV_HH__

#include "dev/io_device.hh"
#include "dev/pcireg.h"
#include "dev/platform.hh"

#define BAR_IO_MASK 0x3
#define BAR_MEM_MASK 0xF
#define BAR_IO_SPACE_BIT 0x1
#define BAR_IO_SPACE(x) ((x) & BAR_IO_SPACE_BIT)
#define BAR_NUMBER(x) (((x) - PCI0_BASE_ADDR0) >> 0x2);

class PciConfigAll;
class MemoryController;


/**
 * This class encapulates the first 64 bytes of a singles PCI
 * devices config space that in configured by the configuration file.
 */
class PciConfigData : public SimObject
{
  public:
    /**
     * Constructor to initialize the devices config space to 0.
     */
    PciConfigData(const std::string &name) 
        : SimObject(name)
    {
        memset(config.data, 0, sizeof(config.data));
        memset(BARAddrs, 0, sizeof(BARAddrs));
        memset(BARSize, 0, sizeof(BARSize));
    }

    /** The first 64 bytes */
    PCIConfig config;
 
    /** The size of the BARs */
    uint32_t BARSize[6];
 
    /** The addresses of the BARs */
    Addr BARAddrs[6];
};

/**
 * PCI device, base implemnation is only config space.
 * Each device is connected to a PCIConfigSpace device
 * which returns -1 for everything but the pcidevs that
 * register with it. This object registers with the PCIConfig space
 * object. 
 */
class PciDev : public DmaDevice
{
  public:
    struct Params
    {
	std::string name;
	Platform *plat;
	MemoryController *mmu;

	/**
	 * A pointer to the configspace all object that calls us when
	 * a read comes to this particular device/function.
	 */
	PciConfigAll *configSpace;

	/**
	 * A pointer to the object that contains the first 64 bytes of
	 * config space
	 */
	PciConfigData *configData;

	/** The bus number we are on */
	uint32_t busNum;

	/** The device number we have */
	uint32_t deviceNum;

	/** The function number */
	uint32_t functionNum;
    };

  protected:
    Params *_params;

  public:
    const Params *params() const { return _params; }

  protected:
    /** The current config space. Unlike the PciConfigData this is
     * updated during simulation while continues to reflect what was
     * in the config file.
     */
    PCIConfig config;
 
    /** The size of the BARs */
    uint32_t BARSize[6];

    /** The current address mapping of the BARs */
    Addr BARAddrs[6];

  protected:
    Platform *plat;
    PciConfigData *configData;

  public:
    Addr pciToDma(Addr pciAddr) const
    { return plat->pciToDma(pciAddr); }

    void
    intrPost()
    { plat->postPciInt(configData->config.interruptLine); }

    void
    intrClear()
    { plat->clearPciInt(configData->config.interruptLine); }

    uint8_t 
    interruptLine()
    { return configData->config.interruptLine; }

  public:
    /**
     * Constructor for PCI Dev. This function copies data from the
     * config file object PCIConfigData and registers the device with
     * a PciConfigAll object.
     */
    PciDev(Params *params);

    virtual Fault read(MemReqPtr &req, uint8_t *data) {
        return No_Fault; 
    }
    virtual Fault write(MemReqPtr &req, const uint8_t *data) { 
        return No_Fault; 
    }

    /**
     * Write to the PCI config space data that is stored locally. This may be
     * overridden by the device but at some point it will eventually call this
     * for normal operations that it does not need to override.
     * @param offset the offset into config space
     * @param size the size of the write
     * @param data the data to write
     */
    virtual void writeConfig(int offset, int size, const uint8_t* data);


    /**
     * Read from the PCI config space data that is stored locally. This may be
     * overridden by the device but at some point it will eventually call this
     * for normal operations that it does not need to override.
     * @param offset the offset into config space
     * @param size the size of the read
     * @param data pointer to the location where the read value should be stored
     */
    virtual void readConfig(int offset, int size, uint8_t *data);

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
};

#endif // __DEV_PCIDEV_HH__
