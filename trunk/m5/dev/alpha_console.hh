/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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
 * System Console Interface
 */

#ifndef __ALPHA_CONSOLE_HH__
#define __ALPHA_CONSOLE_HH__

#include "base/range.hh"
#include "dev/alpha_access.h"
#include "dev/io_device.hh"
#include "sim/host.hh"
#include "sim/sim_object.hh"

class BaseCPU;
class SimConsole;
class System;
class SimpleDisk;

/**
 * Memory mapped interface to the system console. This device
 * represents a shared data region between the OS Kernel and the
 * System Console.
 *
 * The system console is a small standalone program that is initially
 * run when the system boots.  It contains the necessary code to
 * access the boot disk, to read/write from the console, and to pass
 * boot parameters to the kernel.
 *
 * This version of the system console is very different from the one
 * that would be found in a real system.  Many of the functions use
 * some sort of backdoor to get their job done.  For example, reading
 * from the boot device on a real system would require a minimal
 * device driver to access the disk controller, but since we have a
 * simulator here, we are able to bypass the disk controller and
 * access the disk image directly.  There are also some things like
 * reading the kernel off the disk image into memory that are normally
 * taken care of by the console that are now taken care of by the
 * simulator.
 *
 * These shortcuts are acceptable since the system console is
 * primarily used doing boot before the kernel has loaded its device
 * drivers.
 */
class AlphaConsole : public PioDevice
{
  protected:
    struct Access : public AlphaAccess
    {
        void serialize(std::ostream &os);
        void unserialize(Checkpoint *cp, const std::string &section);
    };

    union {
	Access *alphaAccess;
	uint8_t *consoleData;
    };
    
    /** the disk must be accessed from the console */
    SimpleDisk *disk;

    /** the system console (the terminal) is accessable from the console */
    SimConsole *console;

    /** a pointer to the system we are running in */
    System *system;

    /** a pointer to the CPU boot cpu */
    BaseCPU *cpu;
    
    Addr addr;
    static const Addr size = 0x80; // equal to sizeof(alpha_access);

  public:
    /** Standard Constructor */
    AlphaConsole(const std::string &name, SimConsole *cons, SimpleDisk *d,
		 System *s, BaseCPU *c, Platform *platform,
		 MemoryController *mmu, Addr addr,
		 HierParams *hier, Bus *bus);

    virtual void startup();

    /**
     * memory mapped reads and writes
     */
    virtual Fault read(MemReqPtr &req, uint8_t *data);
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    /**
     * standard serialization routines for checkpointing
     */
    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);

  public:
    Tick cacheAccess(MemReqPtr &req);
};

#endif // __ALPHA_CONSOLE_HH__
