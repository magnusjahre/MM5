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
 * Implements a 8250 UART
 */

#include <string>
#include <vector>

#include "base/inifile.hh"
#include "base/str.hh"        // for to_number
#include "base/trace.hh"
#include "dev/simconsole.hh"
#include "dev/uart.hh"
#include "dev/platform.hh"
#include "mem/bus/bus.hh"
#include "mem/bus/pio_interface.hh"
#include "mem/bus/pio_interface_impl.hh"
#include "mem/functional/memory_control.hh"
#include "sim/builder.hh"

using namespace std;

Uart::Uart(const string &name, SimConsole *c, MemoryController *mmu, Addr a,
	   Addr s, HierParams *hier, Bus *bus, Tick pio_latency, Platform *p)
    : PioDevice(name, p), addr(a), size(s), cons(c)
{
    mmu->add_child(this, RangeSize(addr, size));


    if (bus) {
        pioInterface = newPioInterface(name, hier, bus, this,
                                      &Uart::cacheAccess);
	pioInterface->addAddrRange(RangeSize(addr, size));
	pioLatency = pio_latency * bus->clockRate;
    }   

    status = 0;
    
    // set back pointers 
    cons->uart = this;
    platform->uart = this;
}

Tick
Uart::cacheAccess(MemReqPtr &req)
{
    return curTick + pioLatency;
}

DEFINE_SIM_OBJECT_CLASS_NAME("Uart", Uart)

