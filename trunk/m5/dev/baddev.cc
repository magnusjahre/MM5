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
 * BadDevice implemenation
 */

#include <deque>
#include <string>
#include <vector>

#include "base/trace.hh"
#include "cpu/exec_context.hh"
#include "dev/baddev.hh"
#include "dev/platform.hh"
#include "mem/bus/bus.hh"
#include "mem/bus/pio_interface.hh"
#include "mem/bus/pio_interface_impl.hh"
#include "mem/functional/memory_control.hh"
#include "sim/builder.hh"
#include "sim/system.hh"

using namespace std;

BadDevice::BadDevice(const string &name, Addr a, MemoryController *mmu, 
                     HierParams *hier, Bus *bus, const string &devicename)
    : PioDevice(name, NULL), addr(a), devname(devicename)
{
    mmu->add_child(this, RangeSize(addr, size));

    if (bus) {
        pioInterface = newPioInterface(name, hier, bus, this,
                                      &BadDevice::cacheAccess);
        pioInterface->addAddrRange(RangeSize(addr, size));
    }   

}

Fault
BadDevice::read(MemReqPtr &req, uint8_t *data)
{

    panic("Device %s not imlpmented\n", devname);
    return No_Fault;
}

Fault
BadDevice::write(MemReqPtr &req, const uint8_t *data)
{
    panic("Device %s not imlpmented\n", devname);
    return No_Fault;
}

Tick
BadDevice::cacheAccess(MemReqPtr &req)
{
    return curTick;
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(BadDevice)

    SimObjectParam<Platform *> platform;
    SimObjectParam<MemoryController *> mmu;
    Param<Addr> addr;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<Bus*> io_bus;
    Param<Tick> pio_latency;
    Param<string> devicename;

END_DECLARE_SIM_OBJECT_PARAMS(BadDevice)

BEGIN_INIT_SIM_OBJECT_PARAMS(BadDevice)

    INIT_PARAM(platform, "Platform"),
    INIT_PARAM(mmu, "Memory Controller"),
    INIT_PARAM(addr, "Device Address"),
    INIT_PARAM_DFLT(hier, "Hierarchy global variables", &defaultHierParams),
    INIT_PARAM_DFLT(io_bus, "The IO Bus to attach to", NULL),
    INIT_PARAM_DFLT(pio_latency, "Programmed IO latency", 1000),
    INIT_PARAM(devicename, "Name of device to error on")

END_INIT_SIM_OBJECT_PARAMS(BadDevice)

CREATE_SIM_OBJECT(BadDevice)
{
    return new BadDevice(getInstanceName(), addr, mmu, hier, io_bus,
			 devicename);
}

REGISTER_SIM_OBJECT("BadDevice", BadDevice)
