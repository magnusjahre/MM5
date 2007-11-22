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
 * Declarations for FastCPU object and surrounding objects.
 */

#ifndef __FAST_CPU_HH__
#define __FAST_CPU_HH__

#include "config/full_system.hh"
#include "cpu/base.hh"
#include "sim/eventq.hh"
#include "cpu/pc_event.hh"
#include "cpu/exec_context.hh"
#include "cpu/sampler/sampler.hh"
#include "cpu/static_inst.hh"

// forward declarations
#if FULL_SYSTEM
class Processor;
class AlphaITB;
class AlphaDTB;
class PhysicalMemory;

class RemoteGDB;
class GDBListener;
#endif // FULL_SYSTEM

class MemInterface;
class Checkpoint;

/**
 * A fast, simple CPU model for generating checkpoints.
 * @todo: Move things to the ISA level that should be there.  Be more
 * efficient in handling PC-based and number-of-committed-instruction-based
 * events.  Templatize so itb and dtb aren't architecture specific.  
 */

class FastCPU : public BaseCPU
{
  public:
    /** main simulation loop (one cycle). */
    void tick();

  private:
    /** TickEvent class used to schedule cpu ticks. */
    class TickEvent : public Event
    {
      private:
	FastCPU *cpu;

      public:
        /**
         * TickEvent constructor.
         * @param c The CPU associated with this tick event.
         */
	TickEvent(FastCPU *c);

        /** Process tick event. */
	void process();

        /** Accessor of description of event. */
	const char *description();
    };

    /** TickEvent object to schedule CPU ticks. */
    TickEvent tickEvent;

    /** 
     * Schedule tick event, regardless of its current state.
     * @param numCycles The delay, in ticks, of when this event will be
     * scheduled.
     */
        
    void scheduleTickEvent(int numCycles)
    {
	if (tickEvent.squashed())
	    tickEvent.reschedule(curTick + cycles(numCycles));
	else if (!tickEvent.scheduled())
	    tickEvent.schedule(curTick + cycles(numCycles));
    }

    /**
     * Unschedule tick event, regardless of its current state.
     */
    void unscheduleTickEvent()
    {
	if (tickEvent.scheduled())
	    tickEvent.squash();
    }

    /** 
     * Function to check for and process any interrupts.
     * Not sure if this should be private, but will leave it so for now
     * Moved to isa_fullsys_traits.hh and ev5.cc
     */
    void processInterrupts();

  public:
    /** FastCPU statuses. */
    enum Status {
	Running,
	Idle,
	SwitchedOut
    };

  private:
    Status _status;

  public:
    /**
     * Sets machine state after handling an interrupt.
     * @param int_num Interrupt number.
     * @param index Don't know what this is.
     */   
    void post_interrupt(int int_num, int index);

    /**
     * Function to zero fill an address.  Instruction is not actually
     * implemented so this outputs a warning.
     * @param addr Address to zero fill.
     */
    void zero_fill_64(Addr addr) {
      static int warned = 0;
      if (!warned) {
	warn ("WH64 is not implemented");
	warned = 1;
      }
    };


  public:
    struct Params : public BaseCPU::Params
    {
#if FULL_SYSTEM
	AlphaITB *itb;
	AlphaDTB *dtb;
	FunctionalMemory *mem;
#else
	Process *process;
#endif
    };

    FastCPU(Params *params);
    virtual ~FastCPU();

  public:
    /** execution context. */
    ExecContext *xc;

    /** Switch out the current process. */
    void switchOut(Sampler *sampler);

    /**
     * Take over a process from another CPU.  Used for checkpointing.
     * @param oldCPU The old CPU whose contexts are being taken over.
     */
    void takeOverFrom(BaseCPU *oldCPU);

    /** Converts a virtual address to a physical address.
     *  @param addr Virtual address to be translated.
     */
#if FULL_SYSTEM
    Addr dbg_vtophys(Addr addr);
#endif

    /** Current instruction. */
    MachInst inst;

    /** Count of simulated instructions. */
    Counter numInst;

    /** Refcounted pointer to the one memory request. */
    MemReqPtr memReq;

    /**
     * Returns the status of the CPU.
     * @return The status of the CPU.
     */
    Status status() const { return _status; }

    /**
     * Activates a different context, scheduled to start after a given
     * delay.
     * @param thread_num The thread to switch to.
     * @param delay The amount of time to take to switch to the new thread.
     */
    virtual void activateContext(int thread_num, int delay);

    /**
     * Suspends the given context.
     * @param thread_num The thread to suspend.
     */
    virtual void suspendContext(int thread_num);

    /**
     * Deallocates the given context.
     * @param thread_num The thread to deallocate.
     */
    virtual void deallocateContext(int thread_num);

    /**
     * Halts the given context.
     * @param thread_num The thread to halt.
     */
    virtual void haltContext(int thread_num);

    /**
     * Serializes the FastCPU object so that a checkpoint can be generated.
     * @param os The output stream to use.
     */
    virtual void serialize(std::ostream &os);

    /**
     * Unserializes a FastCPU object so execution can restart at a
     * checkpoint.
     * @param cp Pointer to the checkpoint.
     * @param section The name of the FastCPU being resumed from.
     */
    virtual void unserialize(Checkpoint *cp, const std::string &section);

    /**
     * Does a read of the given address.
     * @param addr The address to read from.
     * @param data The variable where the result of the read is placed.
     * @param flags Any flags for the read operation.
     * @return The fault status of the read.
     */
    template <class T>
    Fault read(Addr addr, T &data, unsigned flags);

    /*
     * Does a write of the given address.
     * @param data The data to write to memory.
     * @param addr The address to write to.
     * @param res A pointer to the result of the memory access.  Used for
     * store conditional.
     * @return The fault status of the write.
     */
    template <class T>
    Fault write(T data, Addr addr, unsigned flags,
			uint64_t *res);

    // These functions are only used in CPU models that split
    // effective address computation from the actual memory access.
    void setEA(Addr EA) { panic("FastCPU::setEA() not implemented\n"); }
    Addr getEA() 	{ panic("FastCPU::getEA() not implemented\n"); }

    void prefetch(Addr addr, unsigned flags)
    {
	// need to do this...
    }

    void writeHint(Addr addr, int size, unsigned flags)
    {
	// need to do this...
    }

    Fault copySrcTranslate(Addr src);

    Fault copy(Addr dest);

    // The register accessor methods provide the index of the
    // instruction's operand (e.g., 0 or 1), not the architectural
    // register index, to simplify the implementation of register
    // renaming.  We find the architectural register index by indexing
    // into the instruction's own operand index table.  Note that a
    // raw pointer to the StaticInst is provided instead of a
    // ref-counted StaticInstPtr to reduce overhead.  This is fine as
    // long as these methods don't copy the pointer into any long-term
    // storage (which is pretty hard to imagine they would have reason
    // to do).

    uint64_t readIntReg(const StaticInst<TheISA> *si, int idx)
    {
	return xc->readIntReg(si->srcRegIdx(idx));
    }

    float readFloatRegSingle(const StaticInst<TheISA> *si, int idx)
    {
	int reg_idx = si->srcRegIdx(idx) - TheISA::FP_Base_DepTag;
	return xc->readFloatRegSingle(reg_idx);
    }

    double readFloatRegDouble(const StaticInst<TheISA> *si, int idx)
    {
	int reg_idx = si->srcRegIdx(idx) - TheISA::FP_Base_DepTag;
	return xc->readFloatRegDouble(reg_idx);
    }

    uint64_t readFloatRegInt(const StaticInst<TheISA> *si, int idx)
    {
	int reg_idx = si->srcRegIdx(idx) - TheISA::FP_Base_DepTag;
	return xc->readFloatRegInt(reg_idx);
    }

    void setIntReg(const StaticInst<TheISA> *si, int idx, uint64_t val)
    {
	xc->setIntReg(si->destRegIdx(idx), val);
    }

    void setFloatRegSingle(const StaticInst<TheISA> *si, int idx, float val)
    {
	int reg_idx = si->destRegIdx(idx) - TheISA::FP_Base_DepTag;
	xc->setFloatRegSingle(reg_idx, val);
    }

    void setFloatRegDouble(const StaticInst<TheISA> *si, int idx, double val)
    {
	int reg_idx = si->destRegIdx(idx) - TheISA::FP_Base_DepTag;
	xc->setFloatRegDouble(reg_idx, val);
    }

    void setFloatRegInt(const StaticInst<TheISA> *si, int idx, uint64_t val)
    {
	int reg_idx = si->destRegIdx(idx) - TheISA::FP_Base_DepTag;
	xc->setFloatRegInt(reg_idx, val);
    }

    uint64_t readPC() { return xc->readPC(); }
    void setNextPC(uint64_t val) { return xc->setNextPC(val); }

    uint64_t readUniq() { return xc->readUniq(); }
    void setUniq(uint64_t val) { return xc->setUniq(val); }

    uint64_t readFpcr() { return xc->readFpcr(); }
    void setFpcr(uint64_t val) { return xc->setFpcr(val); }

#if FULL_SYSTEM
    uint64_t readIpr(int idx, Fault &fault) 
    { return xc->readIpr(idx, fault); }

    Fault setIpr(int idx, uint64_t val) 
    { return xc->setIpr(idx, val); }

    uint64_t *getIprPtr()
    { return xc->regs.ipr; }

    Fault hwrei() { return xc->hwrei(); }
    int readIntrFlag() { return xc->readIntrFlag(); }
    void setIntrFlag(int val) { xc->setIntrFlag(val); }
    bool inPalMode() { return xc->inPalMode(); }
    void trap(Fault fault) { return xc->trap(fault); }

    bool simPalCheck(int palFunc) { return xc->simPalCheck(palFunc); }
#else
    void syscall() { xc->syscall(); }
#endif

    bool misspeculating() { return xc->misspeculating(); }
    ExecContext *xcBase() { return xc; }
};

#endif // __FAST_CPU_HH__
