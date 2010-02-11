/*
 * Copyright (c) 2002, 2003, 2004, 2005
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

#include <iostream>
#include <string>
#include <sstream>

#include "base/cprintf.hh"
#include "base/loader/symtab.hh"
#include "base/misc.hh"
#include "base/output.hh"
#include "cpu/base.hh"
#include "cpu/exec_context.hh"
#include "cpu/sampler/sampler.hh"
#include "sim/param.hh"
#include "sim/sim_events.hh"

#include "base/trace.hh"

using namespace std;

vector<BaseCPU *> BaseCPU::cpuList;

// This variable reflects the max number of threads in any CPU.  Be
// careful to only use it once all the CPUs that you care about have
// been initialized
int maxThreadsPerCPU = 1;

#if FULL_SYSTEM
BaseCPU::BaseCPU(Params *p)
    : SimObject(p->name), clock(p->clock), checkInterrupts(true),
      params(p), number_of_threads(p->numberOfThreads), system(p->system)
#else
BaseCPU::BaseCPU(Params *p)
    : SimObject(p->name), clock(p->clock), params(p),
      number_of_threads(p->numberOfThreads)
#endif
{
    DPRINTF(FullCPU, "BaseCPU: Creating object, mem address %#x.\n", this);

    // add self to global list of CPUs
    cpuList.push_back(this);

    DPRINTF(FullCPU, "BaseCPU: CPU added to cpuList, mem address %#x.\n",
            this);

    minInstructionsAllCPUs = p->min_insts_all_cpus;

    if (number_of_threads > maxThreadsPerCPU)
	maxThreadsPerCPU = number_of_threads;

    // allocate per-thread instruction-based event queues
    comInstEventQueue = new EventQueue *[number_of_threads];
    for (int i = 0; i < number_of_threads; ++i)
	comInstEventQueue[i] = new EventQueue("instruction-based event queue");

    //
    // set up instruction-count-based termination events, if any
    //
    if (p->max_insts_any_thread != 0)
	for (int i = 0; i < number_of_threads; ++i)
	    new SimExitEvent(comInstEventQueue[i], p->max_insts_any_thread,
                "a thread reached the max instruction count");

    if (p->max_insts_all_threads != 0) {
	// allocate & initialize shared downcounter: each event will
	// decrement this when triggered; simulation will terminate
	// when counter reaches 0
	int *counter = new int;
	*counter = number_of_threads;
	for (int i = 0; i < number_of_threads; ++i)
	    new CountedExitEvent(comInstEventQueue[i],
	        "all threads reached the max instruction count",
		p->max_insts_all_threads, *counter);
    }

    // allocate per-thread load-based event queues
    comLoadEventQueue = new EventQueue *[number_of_threads];
    for (int i = 0; i < number_of_threads; ++i)
	comLoadEventQueue[i] = new EventQueue("load-based event queue");

    //
    // set up instruction-count-based termination events, if any
    //
    if (p->max_loads_any_thread != 0)
	for (int i = 0; i < number_of_threads; ++i)
	    new SimExitEvent(comLoadEventQueue[i], p->max_loads_any_thread,
                "a thread reached the max load count");

    if (p->max_loads_all_threads != 0) {
	// allocate & initialize shared downcounter: each event will
	// decrement this when triggered; simulation will terminate
	// when counter reaches 0
	int *counter = new int;
	*counter = number_of_threads;
	for (int i = 0; i < number_of_threads; ++i)
	    new CountedExitEvent(comLoadEventQueue[i],
	        "all threads reached the max load count",
		p->max_loads_all_threads, *counter);
    }

#if FULL_SYSTEM
    memset(interrupts, 0, sizeof(interrupts));
    intstatus = 0;
#endif

    functionTracingEnabled = false;
    if (p->functionTrace) {
	functionTraceStream = simout.find(csprintf("ftrace.%s", name()));
	currentFunctionStart = currentFunctionEnd = 0;
	functionEntryTick = p->functionTraceStart;

	if (p->functionTraceStart == 0) {
	    functionTracingEnabled = true;
	} else {
	    Event *e =
		new EventWrapper<BaseCPU, &BaseCPU::enableFunctionTrace>(this,
									 true);
	    e->schedule(p->functionTraceStart);
	}
    }

    CPUParamsCpuID = p->cpu_id;
    commitedInstructionSample = 0;
    canExit = false;
    useInExitDesicion = false;
}


void
BaseCPU::enableFunctionTrace()
{
    functionTracingEnabled = true;
}

BaseCPU::~BaseCPU()
{
}

void
BaseCPU::init()
{
    if (!params->deferRegistration)
	registerExecContexts();
}

void
BaseCPU::regStats()
{
    using namespace Stats;

    numCycles
	.name(name() + ".numCycles")
	.desc("number of cpu cycles simulated")
	;

    int size = execContexts.size();
    if (size > 1) {
	for (int i = 0; i < size; ++i) {
	    stringstream namestr;
	    ccprintf(namestr, "%s.ctx%d", name(), i);
	    execContexts[i]->regStats(namestr.str());
	}
    } else if (size == 1)
	execContexts[0]->regStats(name());
}


void
BaseCPU::registerExecContexts()
{
    for (int i = 0; i < execContexts.size(); ++i) {
	ExecContext *xc = execContexts[i];
#if FULL_SYSTEM
        int id = params->cpu_id;
        if (id != -1)
            id += i;

	xc->cpu_id = system->registerExecContext(xc, id);
#else
	xc->cpu_id = xc->process->registerExecContext(xc);
#endif
    }
}


void
BaseCPU::switchOut(Sampler *sampler)
{
    panic("This CPU doesn't support sampling!");
}

void
BaseCPU::takeOverFrom(BaseCPU *oldCPU)
{

    assert(execContexts.size() == oldCPU->execContexts.size());

    for (int i = 0; i < execContexts.size(); ++i) {
	ExecContext *newXC = execContexts[i];
	ExecContext *oldXC = oldCPU->execContexts[i];

	newXC->takeOverFrom(oldXC);
	assert(newXC->cpu_id == oldXC->cpu_id);
#if FULL_SYSTEM
	system->replaceExecContext(newXC, newXC->cpu_id);
#else
	assert(newXC->process == oldXC->process);
	newXC->process->replaceExecContext(newXC, newXC->cpu_id);
#endif
    }

#if FULL_SYSTEM
    for (int i = 0; i < NumInterruptLevels; ++i)
	interrupts[i] = oldCPU->interrupts[i];
    intstatus = oldCPU->intstatus;
#endif
}


#if FULL_SYSTEM
void
BaseCPU::post_interrupt(int int_num, int index)
{
    DPRINTF(Interrupt, "Interrupt %d:%d posted\n", int_num, index);

    if (int_num < 0 || int_num >= NumInterruptLevels)
	panic("int_num out of bounds\n");

    if (index < 0 || index >= sizeof(uint64_t) * 8)
	panic("int_num out of bounds\n");

    checkInterrupts = true;
    interrupts[int_num] |= 1 << index;
    intstatus |= (ULL(1) << int_num);
}

void
BaseCPU::clear_interrupt(int int_num, int index)
{
    DPRINTF(Interrupt, "Interrupt %d:%d cleared\n", int_num, index);

    if (int_num < 0 || int_num >= NumInterruptLevels)
	panic("int_num out of bounds\n");

    if (index < 0 || index >= sizeof(uint64_t) * 8)
	panic("int_num out of bounds\n");

    interrupts[int_num] &= ~(1 << index);
    if (interrupts[int_num] == 0)
	intstatus &= ~(ULL(1) << int_num);
}

void
BaseCPU::clear_interrupts()
{
    DPRINTF(Interrupt, "Interrupts all cleared\n");

    memset(interrupts, 0, sizeof(interrupts));
    intstatus = 0;
}


void
BaseCPU::serialize(std::ostream &os)
{
    SERIALIZE_ARRAY(interrupts, NumInterruptLevels);
    SERIALIZE_SCALAR(intstatus);
}

void
BaseCPU::unserialize(Checkpoint *cp, const std::string &section)
{
    UNSERIALIZE_ARRAY(interrupts, NumInterruptLevels);
    UNSERIALIZE_SCALAR(intstatus);
}

#endif // FULL_SYSTEM

void
BaseCPU::traceFunctionsInternal(Addr pc)
{
    if (!debugSymbolTable)
	return;

    // if pc enters different function, print new function symbol and
    // update saved range.  Otherwise do nothing.
    if (pc < currentFunctionStart || pc >= currentFunctionEnd) {
	string sym_str;
	bool found = debugSymbolTable->findNearestSymbol(pc, sym_str,
							 currentFunctionStart,
							 currentFunctionEnd);

	if (!found) {
	    // no symbol found: use addr as label
	    sym_str = csprintf("0x%x", pc);
	    currentFunctionStart = pc;
	    currentFunctionEnd = pc + 1;
	}

	ccprintf(*functionTraceStream, " (%d)\n%d: %s",
		 curTick - functionEntryTick, curTick, sym_str);
	functionEntryTick = curTick;
    }
}

void
BaseCPU::registerProcessHalt(){
	fatal("process halt handled by BaseCPU");
}

DEFINE_SIM_OBJECT_CLASS_NAME("BaseCPU", BaseCPU)
