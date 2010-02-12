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

#include <string>

#include "cpu/base.hh"
#include "cpu/exec_context.hh"

#if FULL_SYSTEM
#include "base/cprintf.hh"
#include "kern/kernel_stats.hh"
#include "sim/serialize.hh"
#include "sim/system.hh"
#else
#include "sim/process.hh"
#endif

using namespace std;

// constructor
#if FULL_SYSTEM
ExecContext::ExecContext(BaseCPU *_cpu, int _thread_num, System *_sys,
			 AlphaITB *_itb, AlphaDTB *_dtb,
			 FunctionalMemory *_mem)
    : _status(ExecContext::Unallocated), cpu(_cpu), thread_num(_thread_num),
      cpu_id(-1), mem(_mem), itb(_itb), dtb(_dtb), system(_sys),
      memctrl(_sys->memctrl), physmem(_sys->physmem),
      kernelBinning(system->kernelBinning), bin(kernelBinning->bin),
      fnbin(kernelBinning->fnbin), func_exe_inst(0), storeCondFailures(0)
{
    kernelStats = new Kernel::Statistics(this);
    memset(&regs, 0, sizeof(RegFile));
}
#else
ExecContext::ExecContext(BaseCPU *_cpu, int _thread_num,
			 Process *_process, int _asid)
    : _status(ExecContext::Unallocated),
      cpu(_cpu), thread_num(_thread_num), cpu_id(-1),
      process(_process), mem(process->getMemory()), asid(_asid),
      func_exe_inst(0), storeCondFailures(0)
{
    hasBeenStarted = false;
    isSuspended = false;
    memset(&regs, 0, sizeof(RegFile));
    currentCheckpoint = NULL;
}

ExecContext::ExecContext(BaseCPU *_cpu, int _thread_num,
			 FunctionalMemory *_mem, int _asid)
    : cpu(_cpu), thread_num(_thread_num), process(0), mem(_mem), asid(_asid),
      func_exe_inst(0), storeCondFailures(0)
{
    hasBeenStarted = false;
    isSuspended = false;
    memset(&regs, 0, sizeof(RegFile));
}
#endif

ExecContext::~ExecContext()
{
#if FULL_SYSTEM
    delete kernelStats;
#endif
}


void
ExecContext::takeOverFrom(ExecContext *oldContext)
{
    // some things should already be set up
    assert(mem == oldContext->mem);
#if FULL_SYSTEM
    assert(system == oldContext->system);
#else
    assert(process == oldContext->process);
#endif

    // copy over functional state
    _status = oldContext->_status;
    regs = oldContext->regs;
    cpu_id = oldContext->cpu_id;
    func_exe_inst = oldContext->func_exe_inst;

    // Fix by Magnus
    // go through the waitList and make the entries point to the new context
    for(list<Process::WaitRec>::iterator i = process->waitList.begin();
        i != process->waitList.end();
        i++){
            if(i->waitingContext == oldContext){
                assert(i->waitingContext->cpu->params->cpu_id 
                        == cpu->params->cpu_id);
                i->waitingContext = this;
            }
    }
    
    storeCondFailures = 0;

    oldContext->_status = ExecContext::Unallocated;
}

#if FULL_SYSTEM
void
ExecContext::execute(const StaticInstBase *inst)
{
    assert(kernelStats);
    system->kernelBinning->execute(this, inst);
}
#endif

void
ExecContext::serialize(ostream &os)
{
    SERIALIZE_ENUM(_status);
    regs.serialize(os);
    // thread_num and cpu_id are deterministic from the config
    SERIALIZE_SCALAR(func_exe_inst);
    SERIALIZE_SCALAR(inst);

#if FULL_SYSTEM
    kernelStats->serialize(os);
#endif
}

void
ExecContext::restartProcess(int cpuID, int cpuCount){

	stringstream xcName;
	xcName << "simpleCPU" << cpuID << ".xc";

	stringstream wlName;
	wlName << "simpleCPU" << cpuID;
	if(cpuCount > 1) wlName << ".workload";
	else wlName << ".workload0";
	assert(cpuCount > 0);

	cout << "RESTART: Unserializing the execution context section " << xcName.str() << "...\n";
	unserialize(currentCheckpoint, xcName.str());
	cout << "RESTART: Unserializing the process and functional memory " << wlName.str() << "...\n";
	process->unserialize(NULL, "simpleCPU0.workload0");
	cout << "RESTART: Done!\n";
}

void
ExecContext::unserialize(Checkpoint *cp, const std::string &section)
{

    if(currentCheckpoint == NULL){
    	currentCheckpoint = cp;
    }
    assert(cp != NULL);

	UNSERIALIZE_ENUM(_status);
    regs.unserialize(cp, section);
    // thread_num and cpu_id are deterministic from the config
    UNSERIALIZE_SCALAR(func_exe_inst);
    UNSERIALIZE_SCALAR(inst);

#if FULL_SYSTEM
    kernelStats->unserialize(cp, section);
#endif
}


void
ExecContext::activate(int delay)
{
    if (status() == Active)
	return;

    _status = Active;
    cpu->activateContext(thread_num, delay);
}

void
ExecContext::suspend()
{
    if (status() == Suspended)
	return;

#if FULL_SYSTEM
    // Don't change the status from active if there are pending interrupts
    if (cpu->check_interrupts()) {
	assert(status() == Active);
	return;
    }
#endif

    _status = Suspended;
    cpu->suspendContext(thread_num);
}

void
ExecContext::deallocate()
{
    if (status() == Unallocated)
	return;

    _status = Unallocated;
    cpu->deallocateContext(thread_num);
}

void
ExecContext::halt()
{
    if (status() == Halted)
	return;

    _status = Halted;
    cpu->haltContext(thread_num);
}

void
ExecContext::unallocate(){
    _status = Unallocated;
}

void
ExecContext::regStats(const string &name)
{
#if FULL_SYSTEM
    kernelStats->regStats(name + ".kern");
#endif
}

void
ExecContext::trap(Fault fault)
{
    //TheISA::trap(fault);    //One possible way to do it...

    /** @todo: Going to hack it for now.  Do a true fixup later. */
#if FULL_SYSTEM
    ev5_trap(fault);
#else
    fatal("fault (%d) detected @ PC 0x%08p", fault, readPC());
#endif
}

