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
 * Tsunami Fake Device implementation
 */

#include <deque>
#include <string>
#include <vector>

#include "base/trace.hh"
#include "cpu/exec_context.hh"
#include "dev/tsunami_fake.hh"
#include "mem/bus/bus.hh"
#include "mem/bus/pio_interface.hh"
#include "mem/bus/pio_interface_impl.hh"
#include "mem/functional/memory_control.hh"
#include "sim/builder.hh"
#include "sim/system.hh"

using namespace std;

TsunamiFake::TsunamiFake(const string &name, Addr a, MemoryController *mmu,
                         HierParams *hier, Bus *bus, Addr size)
    : PioDevice(name, NULL), addr(a)
{ 
    mmu->add_child(this, RangeSize(addr, size));

    if (bus) {
        pioInterface = newPioInterface(name, hier, bus, this,
                                      &TsunamiFake::cacheAccess);
        pioInterface->addAddrRange(RangeSize(addr, size));
    } 
}

Fault
TsunamiFake::read(MemReqPtr &req, uint8_t *data)
{
    DPRINTF(Tsunami, "read  va=%#x size=%d\n",
	    req->vaddr, req->size);

#if TRACING_ON
    Addr daddr = (req->paddr - (addr & EV5::PAddrImplMask)) >> 6;
#endif

    switch (req->size) {

      case sizeof(uint64_t):
         *(uint64_t*)data = 0xFFFFFFFFFFFFFFFFULL;
         return No_Fault;
      case sizeof(uint32_t):
         *(uint32_t*)data = 0xFFFFFFFF;
         return No_Fault;
      case sizeof(uint16_t):
         *(uint16_t*)data = 0xFFFF;
         return No_Fault;
      case sizeof(uint8_t):
         *(uint8_t*)data = 0xFF;
         return No_Fault;
          
      default:
        panic("invalid access size(?) for PCI configspace!\n");
    }
    DPRINTFN("Tsunami FakeSMC  ERROR: read  daddr=%#x size=%d\n", daddr, req->size);

    return No_Fault;
}

Fault
TsunamiFake::write(MemReqPtr &req, const uint8_t *data)
{
    DPRINTF(Tsunami, "write - va=%#x size=%d \n",
	    req->vaddr, req->size);

    //:Addr daddr = (req->paddr & addr_mask) >> 6;

    return No_Fault;
}

Tick
TsunamiFake::cacheAccess(MemReqPtr &req)
{
    return curTick;
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(TsunamiFake)

    SimObjectParam<MemoryController *> mmu;
    Param<Addr> addr;
    SimObjectParam<Bus*> io_bus;
    Param<Tick> pio_latency;
    SimObjectParam<HierParams *> hier;
    Param<Addr> size;

END_DECLARE_SIM_OBJECT_PARAMS(TsunamiFake)

BEGIN_INIT_SIM_OBJECT_PARAMS(TsunamiFake)

    INIT_PARAM(mmu, "Memory Controller"),
    INIT_PARAM(addr, "Device Address"),
    INIT_PARAM(io_bus, "The IO Bus to attach to"),
    INIT_PARAM(pio_latency, "Programmed IO latency"),
    INIT_PARAM_DFLT(hier, "Hierarchy global variables", &defaultHierParams),
    INIT_PARAM(size, "Size of address range")

END_INIT_SIM_OBJECT_PARAMS(TsunamiFake)

CREATE_SIM_OBJECT(TsunamiFake)
{
    return new TsunamiFake(getInstanceName(), addr, mmu, hier, io_bus, size);
}

REGISTER_SIM_OBJECT("TsunamiFake", TsunamiFake)
