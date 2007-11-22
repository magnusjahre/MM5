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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <list>
#include <sstream>
#include <string>

#include "base/cprintf.hh"
#include "base/inifile.hh"
#include "base/loader/symtab.hh"
#include "base/misc.hh"
#include "base/pollevent.hh"
#include "base/range.hh"
#include "base/trace.hh"
#include "cpu/base.hh"
#include "cpu/exec_context.hh"
//#include "cpu/smt.hh"
#include "cpu/fast/cpu.hh"
#include "cpu/sampler/sampler.hh"
#include "cpu/static_inst.hh"
#include "mem/base_mem.hh"
#include "mem/mem_interface.hh"
#include "sim/builder.hh"
#include "sim/debug.hh"
#include "sim/host.hh"
#include "sim/sim_events.hh"
#include "sim/sim_object.hh"

#if FULL_SYSTEM
#include "base/remote_gdb.hh"
#include "mem/functional/memory_control.hh"
#include "mem/functional/physical.hh"
#include "sim/system.hh"
#include "targetarch/alpha_memory.hh"
#include "targetarch/vtophys.hh"
#else
#include "mem/functional/functional.hh"
#endif // FULL_SYSTEM

using namespace std;

/*
template <>
void AlphaISA::processInterrupts(FastCPU *xc);

template <>
void AlphaISA::zeroRegisters(FastCPU *xc);
*/

FastCPU::TickEvent::TickEvent(FastCPU *c)
    : Event(&mainEventQueue, CPU_Tick_Pri), cpu(c)
{
}

void
FastCPU::TickEvent::process()
{
    cpu->tick();
}

const char *
FastCPU::TickEvent::description()
{
    return("This is a FastCPU tick");
}

#if FULL_SYSTEM
void
FastCPU::post_interrupt(int int_num, int index)
{
    //Handle the interrupt
    BaseCPU::post_interrupt(int_num, index);

    //Check status of execution context and wakeup if necessary
    if (xc->status() == ExecContext::Suspended) {
        DPRINTF(IPI,"Suspended processor awoke\n");

        //Can insert delay here if not 1 cycle
        xc->activate();
    }
}
#endif // FULL_SYSTEM

FastCPU::FastCPU(Params *p)
    : BaseCPU(p), tickEvent(this), xc(NULL)
{
    _status = Idle;
#if FULL_SYSTEM
    xc = new ExecContext(this, 0, p->system, p->itb, p->dtb, p->mem);

    //Initialize CPU and its registers
    TheISA::initCPU(&(xc->regs));
#else
    xc = new ExecContext(this, /* thread_num */ 0, p->process, /* asid */ 0);
#endif // !FULL_SYSTEM

    memReq = new MemReq();
    memReq->xc = xc;
    memReq->asid = 0;
    memReq->data = new uint8_t[64];

    execContexts.push_back(xc);
}
      
FastCPU::~FastCPU()
{
    //Free up any allocated memory ?
//    delete xc;
//    delete memReq->data;
//    delete memReq;
}

void
FastCPU::switchOut(Sampler *sampler)
{
    _status = SwitchedOut;
    if (tickEvent.scheduled())
        tickEvent.squash();

    sampler->signalSwitched();
}

void
FastCPU::takeOverFrom(BaseCPU *oldCPU)
{
    //Make sure current CPU isn't being used
    assert(!tickEvent.scheduled());

    //Take over from the old CPU
    BaseCPU::takeOverFrom(oldCPU);

    //Check if there's an active xc; if so, set this CPU as Running
    //Currently the CPU is only single threaded, so this may be unnecessary
    for (int i = 0; i < execContexts.size(); ++i) {
        ExecContext *xc = execContexts[i];
        if (xc->status() == ExecContext::Active && _status != Running) {
            _status = Running;
            tickEvent.schedule(curTick);
        }
    }
}


void
FastCPU::activateContext(int thread_num, int delay)
{
    assert(_status == Idle);
    assert(thread_num == 0);
    assert(xc);

    scheduleTickEvent(delay);
    _status = Running;
}

void
FastCPU::suspendContext(int thread_num)
{
    assert(_status == Running);
    assert(thread_num == 0);
    assert(xc);

    unscheduleTickEvent();
    _status = Idle;  //Not sure about this
}

void
FastCPU::deallocateContext(int thread_num)
{
    //Hack...
    suspendContext(thread_num);
}

void
FastCPU::haltContext(int thread_num)
{
    //Hack...
    suspendContext(thread_num);
}

void 
FastCPU::serialize(std::ostream &os)
{
    //Need to serialize all state

    //Main state is the status and current instruction
    SERIALIZE_ENUM(_status);

    //Also the execution context
    nameOut(os, csprintf("%s.xc", name()));
    xc->serialize(os);

    //And finally the tick events
    nameOut(os, csprintf("%s.tickEvent", name()));
    tickEvent.serialize(os);
}

void 
FastCPU::unserialize(Checkpoint *cp, const std::string &section)
{
    //Need to unserialize all state from serialize()
   
    //Main state is the status and current instruction
    UNSERIALIZE_ENUM(_status);

    //Also the execution context
    xc->unserialize(cp, csprintf("%s.xc", section));

    //And finally the tick events
    tickEvent.unserialize(cp, csprintf("%s.tickEvent", section));
}

template <class T>
Fault
FastCPU::read(Addr addr, T &data, unsigned flags)
{
    memReq->reset(addr, sizeof(T), flags);

    //Translate address first
    Fault fault = xc->translateDataReadReq(memReq);

    //If successful, do a read of the memory
    if (fault == No_Fault) {
        fault = xc->read(memReq, data);
    }

    //Need to somehow check if the read was successful
    //Actually without caches, this should be sufficient

    return fault;
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

template
Fault
FastCPU::read(Addr addr, uint64_t &data, unsigned flags);

template
Fault
FastCPU::read(Addr addr, uint32_t &data, unsigned flags);

template
Fault
FastCPU::read(Addr addr, uint16_t &data, unsigned flags);

template
Fault
FastCPU::read(Addr addr, uint8_t &data, unsigned flags);

#endif //DOXYGEN_SHOULD_SKIP_THIS

template<>
Fault
FastCPU::read(Addr addr, double &data, unsigned flags)
{
    return read(addr, *(uint64_t*)&data, flags);
}

template<>
Fault
FastCPU::read(Addr addr, float &data, unsigned flags)
{
    return read(addr, *(uint32_t*)&data, flags);
}


template<>
Fault
FastCPU::read(Addr addr, int32_t &data, unsigned flags)
{
    return read(addr, (uint32_t&)data, flags);
}

template <class T>
Fault
FastCPU::write(T data, Addr addr, unsigned flags, uint64_t *res)
{
    memReq->reset(addr, sizeof(T), flags);

    //Translate address first
    Fault fault = xc->translateDataWriteReq(memReq);

    //If successful, do a write to the memory
    if (fault == No_Fault) {
        fault = xc->write(memReq, data);
    }

    //Need to somehow check if the write was successful
    //Actually without caches, this should be sufficient

    if (res && fault == No_Fault) {
        *res = memReq->result;
    }

    return fault;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
template
Fault
FastCPU::write(uint64_t data, Addr addr, unsigned flags, uint64_t *res);

template
Fault
FastCPU::write(uint32_t data, Addr addr, unsigned flags, uint64_t *res);

template
Fault
FastCPU::write(uint16_t data, Addr addr, unsigned flags, uint64_t *res);

template
Fault
FastCPU::write(uint8_t data, Addr addr, unsigned flags, uint64_t *res);

#endif //DOXYGEN_SHOULD_SKIP_THIS

template<>
Fault
FastCPU::write(double data, Addr addr, unsigned flags, uint64_t *res)
{
    return write(*(uint64_t*)&data, addr, flags, res);
}

template<>
Fault
FastCPU::write(float data, Addr addr, unsigned flags, uint64_t *res)
{
    return write(*(uint32_t*)&data, addr, flags, res);
}


template<>
Fault
FastCPU::write(int32_t data, Addr addr, unsigned flags, uint64_t *res)
{
    return write((uint32_t)data, addr, flags, res);
}


Fault
FastCPU::copySrcTranslate(Addr src)
{
    memReq->reset(src, 64);

    //Translate address first
    Fault fault = xc->translateDataReadReq(memReq);

    //Copy over the source addr and physical addr (hack)
    if (fault == No_Fault) {
        xc->copySrcAddr = src;
        xc->copySrcPhysAddr = memReq->paddr;
    } else {
        xc->copySrcAddr = 0;
        xc->copySrcPhysAddr = 0;
    }
    return fault;
}

Fault
FastCPU::copy(Addr dest)
{
    int blk_size = 64;

    uint8_t data[blk_size];
    
    assert(xc->copySrcPhysAddr);
    memReq->reset(dest, blk_size);
    
    //Translate address first
    Fault fault = xc->translateDataWriteReq(memReq);

    //If successful, do copy
    if (fault == No_Fault) {
        //Copy the old physical address so we don't need to re-execute
        //the initial memory request translation
        Addr dest_physaddr = memReq->paddr;

        //Now set the phys addr to the src set by copySrcTranslate()
        memReq->paddr = xc->copySrcPhysAddr;
        xc->mem->read(memReq, data);

        //Now set the phys addr to the destination translated above
        memReq->paddr = dest_physaddr;
        xc->mem->write(memReq, data);
    }
    return fault;
}

/* start simulation, program loaded, processor precise state initialized */
void
FastCPU::tick()
{
    //Process interrupts if interrupts are enabled and not a PAL inst
#if FULL_SYSTEM
    if (checkInterrupts && check_interrupts() && !xc->inPalMode())
        TheISA::processInterrupts(this);
#endif // FULL_SYSTEM
      
#if FULL_SYSTEM
    TheISA::zeroRegisters(this);
#else
    // Insure ISA semantics
    xc->setIntReg(ZeroReg, 0);
    xc->setFloatRegDouble(ZeroReg, 0.0);
#endif

    //Read current PC
    Addr pc = xc->readPC();
    
    //If a PAL instruction, then the memory address is physical
#if FULL_SYSTEM
    unsigned flags = xc->inPalMode() ? PHYSICAL : 0;
#else
    unsigned flags = 0;
#endif // FULL_SYSTEM

    //Setup memory request
    memReq->cmd = Read;
    memReq->reset(pc & ~3, sizeof(uint32_t), flags);

    //Translate instruction to physical address
    Fault fault = xc->translateInstReq(memReq);

    //Check if translation was successful
    if (fault == No_Fault) {
        fault = xc->instRead(memReq);

        //If we have a valid instruction, then execute
        if (fault == No_Fault) {
            //Increment number of completed instructions
            numInst++;
            
            //Do any committed-instruction-based events
            comInstEventQueue[0]->serviceEvents(numInst);
            
            //Create smart pointer to instruction
            StaticInstPtr<TheISA> si(xc->getInst());
            
            //Execute instruction
            fault = si->execute(this, NULL);
        }
    }

    //Either set PC to next PC if no faults, or trap to handle the fault
    if (fault == No_Fault) {
        xc->regs.pc = xc->regs.npc;
        xc->regs.npc += sizeof(MachInst);
    }
    else {
        xc->trap(fault);
    }

    //Handle any PC based events
#if FULL_SYSTEM
    Addr oldpc;
    do {
        oldpc = xc->regs.pc;
        system->pcEventQueue.service(xc);
    } while (oldpc != xc->regs.pc);
#endif

    //Reschedule CPU to run
    if (status() == Running && !tickEvent.scheduled())
        tickEvent.schedule(curTick + cycles(1));


}

#if FULL_SYSTEM
Addr
FastCPU::dbg_vtophys(Addr addr)
{
    return vtophys(xc, addr);
}
#endif // FULL_SYSTEM

////////////////////////////////////////////////////////////////////////
//
//  FastCPU Simulation Object
//
BEGIN_DECLARE_SIM_OBJECT_PARAMS(FastCPU)

    Param<Counter> max_insts_any_thread;
    Param<Counter> max_insts_all_threads;
    Param<Counter> max_loads_any_thread;
    Param<Counter> max_loads_all_threads;

#if FULL_SYSTEM
    SimObjectParam<AlphaITB *> itb;
    SimObjectParam<AlphaDTB *> dtb;
    SimObjectParam<FunctionalMemory *> mem;
    SimObjectParam<System *> system;
    Param<int> cpu_id;
#else
    SimObjectParam<Process *> workload;
#endif // FULL_SYSTEM

    Param<int> clock;
    Param<bool> defer_registration;

END_DECLARE_SIM_OBJECT_PARAMS(FastCPU)

BEGIN_INIT_SIM_OBJECT_PARAMS(FastCPU)

    INIT_PARAM(max_insts_any_thread,
	       "terminate when any thread reaches this inst count"),
    INIT_PARAM(max_insts_all_threads,
	       "terminate when all threads have reached this inst count"),
    INIT_PARAM(max_loads_any_thread,
	       "terminate when any thread reaches this load count"),
    INIT_PARAM(max_loads_all_threads,
	       "terminate when all threads have reached this load count"),

#if FULL_SYSTEM
    INIT_PARAM(itb, "Instruction TLB"),
    INIT_PARAM(dtb, "Data TLB"),
    INIT_PARAM(mem, "memory"),
    INIT_PARAM(system, "system object"),
    INIT_PARAM(cpu_id, "processor ID"),
#else
    INIT_PARAM(workload, "processes to run"),
#endif // FULL_SYSTEM

    INIT_PARAM(clock, "clock speed"),
    INIT_PARAM(defer_registration, "defer system registration (for sampling)")

END_INIT_SIM_OBJECT_PARAMS(FastCPU)


CREATE_SIM_OBJECT(FastCPU)
{
    FastCPU::Params *params = new FastCPU::Params();
    params->name = getInstanceName();
    params->max_insts_any_thread = max_insts_any_thread;
    params->max_insts_all_threads = max_insts_all_threads;
    params->max_loads_any_thread = max_loads_any_thread;
    params->max_loads_all_threads = max_loads_all_threads;
    params->deferRegistration = defer_registration;
    params->clock = clock;
    params->functionTrace = false;
    params->functionTraceStart = 0;
#if FULL_SYSTEM
    params->itb = itb;
    params->dtb = dtb;
    params->mem = mem;
    params->system = system;
    params->cpu_id = cpu_id;
#else
    params->process = workload;
#endif

    return new FastCPU(params);
}

REGISTER_SIM_OBJECT("FastCPU", FastCPU)
