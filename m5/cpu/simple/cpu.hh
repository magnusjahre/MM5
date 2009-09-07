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

#ifndef __CPU_SIMPLE_CPU_SIMPLE_CPU_HH__
#define __CPU_SIMPLE_CPU_SIMPLE_CPU_HH__

#include "base/statistics.hh"
#include "config/full_system.hh"
#include "cpu/base.hh"
#include "cpu/exec_context.hh"
#include "cpu/pc_event.hh"
#include "cpu/sampler/sampler.hh"
#include "cpu/static_inst.hh"
#include "sim/eventq.hh"

// forward declarations
#if FULL_SYSTEM
class Processor;
class AlphaITB;
class AlphaDTB;
class PhysicalMemory;

class RemoteGDB;
class GDBListener;

#else

class Process;

#endif // FULL_SYSTEM

class MemInterface;
class Checkpoint;

namespace Trace {
class InstRecord;
}

class SimpleCPU : public BaseCPU
{
public:
	// main simulation loop (one cycle)
	void tick();

private:
	struct TickEvent : public Event
	{
		SimpleCPU *cpu;
		int width;

		TickEvent(SimpleCPU *c, int w);
		void process();
		const char *description();
	};

	int bbv_num_inst;
	bool generateBBVs;
	TickEvent tickEvent;
	Counter checkpointAtInstruction;

	/// Schedule tick event, regardless of its current state.
	void scheduleTickEvent(int numCycles)
	{
		if (tickEvent.squashed())
			tickEvent.reschedule(curTick + cycles(numCycles));
		else if (!tickEvent.scheduled())
			tickEvent.schedule(curTick + cycles(numCycles));
	}

	/// Unschedule tick event, regardless of its current state.
	void unscheduleTickEvent()
	{
		if (tickEvent.scheduled())
			tickEvent.squash();
	}

	private:
		Trace::InstRecord *traceData;

	public:
		//
		enum Status {
			Running,
			Idle,
			IcacheMissStall,
			IcacheMissComplete,
			DcacheMissStall,
			DcacheMissSwitch,
			SwitchedOut
		};

	private:
		Status _status;
		std::string switchFileName;

	public:
		void post_interrupt(int int_num, int index);

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
			MemInterface *icache_interface;
			MemInterface *dcache_interface;
			int width;
			int bbv_simpoint_size;
			Counter checkpoint_at_instruction;

#if FULL_SYSTEM
			AlphaITB *itb;
			AlphaDTB *dtb;
			FunctionalMemory *mem;
#else
			Process *process;
#endif
		};
		SimpleCPU(Params *params);
		virtual ~SimpleCPU();

		public:
			// execution context
			ExecContext *xc;

			void switchOut(Sampler *s);
			void takeOverFrom(BaseCPU *oldCPU);

#if FULL_SYSTEM
			Addr dbg_vtophys(Addr addr);

			bool interval_stats;
#endif

			// L1 instruction cache
			MemInterface *icacheInterface;

			// L1 data cache
			MemInterface *dcacheInterface;

			// current instruction
			MachInst inst;

			// Refcounted pointer to the one memory request.
			MemReqPtr memReq;

			// Pointer to the sampler that is telling us to switchover.
			// Used to signal the completion of the pipe drain and schedule
			// the next switchover
			Sampler *sampler;

			StaticInstPtr<TheISA> curStaticInst;

			class CacheCompletionEvent : public Event
			{
			private:
				SimpleCPU *cpu;

			public:
				CacheCompletionEvent(SimpleCPU *_cpu);

				virtual void process();
				virtual const char *description();
			};

			CacheCompletionEvent cacheCompletionEvent;

			Status status() const { return _status; }

			virtual void activateContext(int thread_num, int delay);
			virtual void suspendContext(int thread_num);
			virtual void deallocateContext(int thread_num);
			virtual void haltContext(int thread_num);

			// statistics
			virtual void regStats();
			virtual void resetStats();

			// number of simulated instructions
		private:
			Counter numInst;
			Counter startNumInst;
			Stats::Scalar<> numInsts;
		public:

			virtual Counter totalInstructions() const
			{
				return numInst - startNumInst;
			}

			// number of simulated memory references
			Stats::Scalar<> numMemRefs;

			// number of simulated loads
			Counter numLoad;
			Counter startNumLoad;

			// number of idle cycles
			Stats::Average<> notIdleFraction;
			Stats::Formula idleFraction;

			// number of cycles stalled for I-cache misses
			Stats::Scalar<> icacheStallCycles;
			Counter lastIcacheStall;

			// number of cycles stalled for D-cache misses
			Stats::Scalar<> dcacheStallCycles;
			Counter lastDcacheStall;

			void processCacheCompletion();

			virtual void serialize(std::ostream &os);
			virtual void unserialize(Checkpoint *cp, const std::string &section);

			template <class T>
			Fault read(Addr addr, T &data, unsigned flags);

			template <class T>
			Fault write(T data, Addr addr, unsigned flags, uint64_t *res);

			// These functions are only used in CPU models that split
			// effective address computation from the actual memory access.
			void setEA(Addr EA) { panic("SimpleCPU::setEA() not implemented\n"); }
			Addr getEA() 	{ panic("SimpleCPU::getEA() not implemented\n"); }

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
			// ref-counted StaticInstPtr to redice overhead.  This is fine as
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
			void setNextPC(uint64_t val) { xc->setNextPC(val); }

			uint64_t readUniq() { return xc->readUniq(); }
			void setUniq(uint64_t val) { xc->setUniq(val); }

			uint64_t readFpcr() { return xc->readFpcr(); }
			void setFpcr(uint64_t val) { xc->setFpcr(val); }

#if FULL_SYSTEM
			uint64_t readIpr(int idx, Fault &fault) { return xc->readIpr(idx, fault); }
			Fault setIpr(int idx, uint64_t val) { return xc->setIpr(idx, val); }
			Fault hwrei() { return xc->hwrei(); }
			int readIntrFlag() { return xc->readIntrFlag(); }
			void setIntrFlag(int val) { xc->setIntrFlag(val); }
			bool inPalMode() { return xc->inPalMode(); }
			void ev5_trap(Fault fault) { xc->ev5_trap(fault); }
			bool simPalCheck(int palFunc) { return xc->simPalCheck(palFunc); }
#else
			void syscall() {
				//         std::cout << name() << " syscall called\n";
				xc->syscall();
			}
#endif

			bool misspeculating() { return xc->misspeculating(); }
			ExecContext *xcBase() { return xc; }
};

#endif // __CPU_SIMPLE_CPU_SIMPLE_CPU_HH__
