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
 * Modifications for the FreeBSD kernel.  
 * Based on kern/linux/linux_system.cc.
 *
 */

#include "base/loader/symtab.hh"
#include "cpu/exec_context.hh"
#include "kern/freebsd/freebsd_system.hh"
#include "mem/functional/memory_control.hh"
#include "mem/functional/physical.hh"
#include "sim/builder.hh"
#include "targetarch/vtophys.hh"

#define TIMER_FREQUENCY 1193180

using namespace std;

FreebsdSystem::FreebsdSystem(Params *p)
    : System(p)
{
    /**
     * Any time DELAY is called just skip the function. 
     * Shouldn't we actually emulate the delay?
     */
    skipDelayEvent = addKernelFuncEvent<SkipFuncEvent>("DELAY");
    skipCalibrateClocks =
	addKernelFuncEvent<SkipCalibrateClocksEvent>("calibrate_clocks");
}


FreebsdSystem::~FreebsdSystem()
{
    delete skipDelayEvent;
    delete skipCalibrateClocks;
}


void
FreebsdSystem::doCalibrateClocks(ExecContext *xc)
{
    Addr ppc_vaddr = 0;
    Addr timer_vaddr = 0;
    Addr ppc_paddr = 0;
    Addr timer_paddr = 0;

    ppc_vaddr = (Addr)xc->regs.intRegFile[ArgumentReg1];
    timer_vaddr = (Addr)xc->regs.intRegFile[ArgumentReg2];

    ppc_paddr = vtophys(physmem, ppc_vaddr);
    timer_paddr = vtophys(physmem, timer_vaddr);

    uint8_t *ppc = physmem->dma_addr(ppc_paddr, sizeof(uint32_t));
    uint8_t *timer = physmem->dma_addr(timer_paddr, sizeof(uint32_t));

    *(uint32_t *)ppc = htog((uint32_t)Clock::Frequency);
    *(uint32_t *)timer = htog((uint32_t)TIMER_FREQUENCY);
}


void
FreebsdSystem::SkipCalibrateClocksEvent::process(ExecContext *xc)
{
    SkipFuncEvent::process(xc);
    ((FreebsdSystem *)xc->system)->doCalibrateClocks(xc);
}


BEGIN_DECLARE_SIM_OBJECT_PARAMS(FreebsdSystem)

    Param<Tick> boot_cpu_frequency;
    SimObjectParam<MemoryController *> memctrl;
    SimObjectParam<PhysicalMemory *> physmem;

    Param<string> kernel;
    Param<string> console;
    Param<string> pal;

    Param<string> boot_osflags;
    Param<string> readfile;
    Param<unsigned int> init_param;

    Param<uint64_t> system_type;
    Param<uint64_t> system_rev;

    Param<bool> bin;
    VectorParam<string> binned_fns;
    Param<bool> bin_int;

END_DECLARE_SIM_OBJECT_PARAMS(FreebsdSystem)

BEGIN_INIT_SIM_OBJECT_PARAMS(FreebsdSystem)

    INIT_PARAM(boot_cpu_frequency, "Frequency of the boot CPU"),
    INIT_PARAM(memctrl, "memory controller"),
    INIT_PARAM(physmem, "phsyical memory"),
    INIT_PARAM(kernel, "file that contains the kernel code"),
    INIT_PARAM(console, "file that contains the console code"),
    INIT_PARAM(pal, "file that contains palcode"),
    INIT_PARAM_DFLT(boot_osflags, "flags to pass to the kernel during boot",
		    "a"),
    INIT_PARAM_DFLT(readfile, "file to read startup script from", ""),
    INIT_PARAM_DFLT(init_param, "numerical value to pass into simulator", 0),
    INIT_PARAM_DFLT(system_type, "Type of system we are emulating", 34),
    INIT_PARAM_DFLT(system_rev, "Revision of system we are emulating", 1<<10),
    INIT_PARAM_DFLT(bin, "is this system to be binned", false),
    INIT_PARAM(binned_fns, "functions to be broken down and binned"),
    INIT_PARAM_DFLT(bin_int, "is interrupt code binned seperately?", true)

END_INIT_SIM_OBJECT_PARAMS(FreebsdSystem)

CREATE_SIM_OBJECT(FreebsdSystem)
{
    System::Params *p = new System::Params;
    p->name = getInstanceName();
    p->boot_cpu_frequency = boot_cpu_frequency;
    p->memctrl = memctrl;
    p->physmem = physmem;
    p->kernel_path = kernel;
    p->console_path = console;
    p->palcode = pal;
    p->boot_osflags = boot_osflags;
    p->init_param = init_param;
    p->readfile = readfile;
    p->system_type = system_type;
    p->system_rev = system_rev;
    p->bin = bin;
    p->binned_fns = binned_fns;
    p->bin_int = bin_int; 
    return new FreebsdSystem(p);
}

REGISTER_SIM_OBJECT("FreebsdSystem", FreebsdSystem)

