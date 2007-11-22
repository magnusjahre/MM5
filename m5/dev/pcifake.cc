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


#include "base/trace.hh"
#include "cpu/intr_control.hh"
#include "dev/ide_ctrl.hh"
#include "dev/ide_disk.hh"
#include "dev/pciconfigall.hh"
#include "dev/pcifake.hh"
#include "dev/pcireg.h"
#include "dev/platform.hh"
#include "mem/bus/bus.hh"
#include "mem/bus/dma_interface.hh"
#include "mem/bus/pio_interface.hh"
#include "mem/bus/pio_interface_impl.hh"
#include "mem/functional/memory_control.hh"
#include "mem/functional/physical.hh"
#include "sim/builder.hh"
#include "sim/sim_object.hh"


PciFake::PciFake(Params *p)
    : PciDev(p)
{

}

PciFake::~PciFake()
{}


Fault
PciFake::read(MemReqPtr &req, uint8_t *data)
{
    switch (req->size) {
      case sizeof(uint8_t):
      case sizeof(uint16_t):
      case sizeof(uint32_t):
         memset(data, 0xFF, req->size);
         break;
      default:
        panic("Invalid access size for fake PCI device registers!\n");
    }

    return No_Fault;
}


Fault
PciFake::write(MemReqPtr &req, const uint8_t *data)
{
    return No_Fault;
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(PciFake)

    Param<Addr> addr;
    SimObjectParam<MemoryController *> mmu;
    SimObjectParam<PciConfigAll *> configspace;
    SimObjectParam<PciConfigData *> configdata;
    SimObjectParam<Platform *> platform;
    Param<uint32_t> pci_bus;
    Param<uint32_t> pci_dev;
    Param<uint32_t> pci_func;
    SimObjectParam<Bus *> io_bus;
    Param<Tick> pio_latency;
    SimObjectParam<HierParams *> hier;

END_DECLARE_SIM_OBJECT_PARAMS(PciFake)


BEGIN_INIT_SIM_OBJECT_PARAMS(PciFake)

    INIT_PARAM(addr, "Device Address"),
    INIT_PARAM(mmu, "Memory controller"),
    INIT_PARAM(configspace, "PCI Configspace"),
    INIT_PARAM(configdata, "PCI Config data"),
    INIT_PARAM(platform, "Platform pointer"),
    INIT_PARAM(pci_bus, "PCI bus ID"),
    INIT_PARAM(pci_dev, "PCI device number"),
    INIT_PARAM(pci_func, "PCI function code"),
    INIT_PARAM_DFLT(io_bus, "Host bus to attach to", NULL),
    INIT_PARAM_DFLT(pio_latency, "Programmed IO latency in bus cycles", 1),
    INIT_PARAM_DFLT(hier, "Hierarchy global variables", &defaultHierParams)

END_INIT_SIM_OBJECT_PARAMS(PciFake)

CREATE_SIM_OBJECT(PciFake)
{
    PciFake::Params *params = new PciFake::Params;
    params->name = getInstanceName();
    params->mmu = mmu;
    params->configSpace = configspace;
    params->configData = configdata;
    params->plat = platform;
    params->busNum = pci_bus;
    params->deviceNum = pci_dev;
    params->functionNum = pci_func;
    params->pio_latency = pio_latency;
    params->hier = hier;
    params->host_bus = io_bus;
    return new PciFake(params);
}

REGISTER_SIM_OBJECT("PciFake", PciFake)

#endif //DOXYGEN_SHOULD_SKIP_THIS
