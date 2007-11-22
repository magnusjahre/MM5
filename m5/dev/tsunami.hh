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
 * Declaration of top level class for the Tsunami chipset. This class just 
 * retains pointers to all its children so the children can communicate.
 */

#ifndef __DEV_TSUNAMI_HH__
#define __DEV_TSUNAMI_HH__

#include "dev/platform.hh"

class IdeController;
class TlaserClock;
class NSGigE;
class TsunamiCChip;
class TsunamiPChip;
class TsunamiIO;
class PciConfigAll;
class System;

/**
  * Top level class for Tsunami Chipset emulation.
  * This structure just contains pointers to all the 
  * children so the children can commnicate to do the 
  * read work
  */
  
class Tsunami : public Platform
{
  public:
    /** Max number of CPUs in a Tsunami */
    static const int Max_CPUs = 64;

    /** Pointer to the system */
    System *system;

    /** Pointer to the TsunamiIO device which has the RTC */
    TsunamiIO *io;

    /** Pointer to the Tsunami CChip.
     * The chip contains some configuration information and
     * all the interrupt mask and status registers 
     */
    TsunamiCChip *cchip;
    
    /** Pointer to the Tsunami PChip.
     * The pchip is the interface to the PCI bus, in our case
     * it does not have to do much.
     */
    TsunamiPChip *pchip;

    int intr_sum_type[Tsunami::Max_CPUs];
    int ipi_pending[Tsunami::Max_CPUs];

  public:
    /**
     * Constructor for the Tsunami Class.
     * @param name name of the object
     * @param intrctrl pointer to the interrupt controller
     */
    Tsunami(const std::string &name, System *s, IntrControl *intctrl, 
            PciConfigAll *pci);

    /**
     * Return the interrupting frequency to AlphaAccess
     * @return frequency of RTC interrupts
     */
    virtual Tick intrFrequency(); 
     
    /**
     * Cause the cpu to post a serial interrupt to the CPU.
     */
    virtual void postConsoleInt();

    /**
     * Clear a posted CPU interrupt (id=55)
     */
    virtual void clearConsoleInt();

    /**
     * Cause the chipset to post a cpi interrupt to the CPU.
     */
    virtual void postPciInt(int line);

    /**
     * Clear a posted PCI->CPU interrupt
     */
    virtual void clearPciInt(int line);

    virtual Addr pciToDma(Addr pciAddr) const;

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

#endif // __DEV_TSUNAMI_HH__
