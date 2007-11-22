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
 * Generic interface for platforms
 */

#ifndef __DEV_PLATFORM_HH__
#define __DEV_PLATFORM_HH__

#include "sim/sim_object.hh"
#include "targetarch/isa_traits.hh"

class PciConfigAll;
class IntrControl;
class SimConsole;
class Uart;

class Platform : public SimObject
{
  public:
    /** Pointer to the interrupt controller */
    IntrControl *intrctrl;

    /** Pointer to the PCI configuration space */
    PciConfigAll *pciconfig;

    /** Pointer to the UART, set by the uart */
    Uart *uart;

  public:
    Platform(const std::string &name, IntrControl *intctrl, PciConfigAll *pci);
    virtual ~Platform();
    virtual void postConsoleInt() = 0;
    virtual void clearConsoleInt() = 0;
    virtual Tick intrFrequency() = 0;
    virtual void postPciInt(int line);
    virtual void clearPciInt(int line);
    virtual Addr pciToDma(Addr pciAddr) const;
};

#endif // __DEV_PLATFORM_HH__
