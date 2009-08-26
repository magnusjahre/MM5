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

#ifndef __CPU_BASE_HH__
#define __CPU_BASE_HH__

#include <vector>

#include "base/statistics.hh"
#include "config/full_system.hh"
#include "cpu/sampler/sampler.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"
#include "targetarch/isa_traits.hh"

#if FULL_SYSTEM
class System;
#endif

class BranchPred;
class ExecContext;

class BaseCPU : public SimObject
{
  protected:
    // CPU's clock period in terms of the number of ticks of curTime.
    Tick clock;

  public:
    inline Tick frequency() const { return Clock::Frequency / clock; }
    inline Tick cycles(int numCycles) const { return clock * numCycles; }
    inline Tick curCycle() const { return curTick / clock; }

#if FULL_SYSTEM
  protected:
    uint64_t interrupts[NumInterruptLevels];
    uint64_t intstatus;

  public:
    virtual void post_interrupt(int int_num, int index);
    virtual void clear_interrupt(int int_num, int index);
    virtual void clear_interrupts();
    bool checkInterrupts;

    bool check_interrupt(int int_num) const {
	if (int_num > NumInterruptLevels)
	    panic("int_num out of bounds\n");

	return interrupts[int_num] != 0;
    }

    bool check_interrupts() const { return intstatus != 0; }
    uint64_t intr_status() const { return intstatus; }
#endif

    //HACK by Magnus
    std::vector<ExecContext *> execContexts;

    int CPUParamsCpuID;
    bool canExit;

  protected:
    // execContexts was here
    int commitedInstructionSample;
    Counter minInstructionsAllCPUs;
    bool useInExitDesicion;

  public:

    /// Notify the CPU that the indicated context is now active.  The
    /// delay parameter indicates the number of ticks to wait before
    /// executing (typically 0 or 1).
    virtual void activateContext(int thread_num, int delay) {}

    /// Notify the CPU that the indicated context is now suspended.
    virtual void suspendContext(int thread_num) {}

    /// Notify the CPU that the indicated context is now deallocated.
    virtual void deallocateContext(int thread_num) {}

    /// Notify the CPU that the indicated context is now halted.
    virtual void haltContext(int thread_num) {}

  public:
	  struct Params
	  {
		  std::string name;
		  int numberOfThreads;
		  bool deferRegistration;
		  Counter max_insts_any_thread;
		  Counter max_insts_all_threads;
		  Counter max_loads_any_thread;
		  Counter max_loads_all_threads;
		  Tick clock;
		  bool functionTrace;
		  Tick functionTraceStart;
		  int cpu_id; // Magnus
		  Counter min_insts_all_cpus;
#if FULL_SYSTEM
		  System *system;
#endif
	  };

    const Params *params;

    BaseCPU(Params *params);
    virtual ~BaseCPU();

    virtual void init();
    virtual void regStats();

    void registerExecContexts();

    /// Prepare for another CPU to take over execution.  When it is
    /// is ready (drained pipe) it signals the sampler.
    virtual void switchOut(Sampler *);

    /// Take over execution from the given CPU.  Used for warm-up and
    /// sampling.
    virtual void takeOverFrom(BaseCPU *);

    /**
     *  Number of threads we're actually simulating (<= SMT_MAX_THREADS).
     * This is a constant for the duration of the simulation.
     */
    int number_of_threads;

    /**
     * Vector of per-thread instruction-based event queues.  Used for
     * scheduling events based on number of instructions committed by
     * a particular thread.
     */
    EventQueue **comInstEventQueue;

    /**
     * Vector of per-thread load-based event queues.  Used for
     * scheduling events based on number of loads committed by
     *a particular thread.
     */
    EventQueue **comLoadEventQueue;

#if FULL_SYSTEM
    System *system;

    /**
     * Serialize this object to the given output stream.
     * @param os The stream to serialize to.
     */
    virtual void serialize(std::ostream &os);

    /**
     * Reconstruct the state of this object from a checkpoint.
     * @param cp The checkpoint use.
     * @param section The section name of this object
     */
    virtual void unserialize(Checkpoint *cp, const std::string &section);

#endif

    /**
     * Return pointer to CPU's branch predictor (NULL if none).
     * @return Branch predictor pointer.
     */
    virtual BranchPred *getBranchPred() { return NULL; };

    virtual Counter totalInstructions() const { return 0; }

    double getCommittedInstructionSample(int sampleSize){
        return (double) ((double)commitedInstructionSample / (double) sampleSize);
    }

    void resetCommittedInstructionSample(){
        commitedInstructionSample = 0;
    }



    // Function tracing
  private:
    bool functionTracingEnabled;
    std::ostream *functionTraceStream;
    Addr currentFunctionStart;
    Addr currentFunctionEnd;
    Tick functionEntryTick;
    void enableFunctionTrace();
    void traceFunctionsInternal(Addr pc);

  protected:
    void traceFunctions(Addr pc)
    {
	if (functionTracingEnabled)
	    traceFunctionsInternal(pc);
    }

  private:
    static std::vector<BaseCPU *> cpuList;   //!< Static global cpu list

  public:
	  static int numSimulatedCPUs() { return cpuList.size(); }
	  static Counter numSimulatedInstructions()
	  {
		  Counter total = 0;

		  int size = cpuList.size();
		  for (int i = 0; i < size; ++i)
			  total += cpuList[i]->totalInstructions();

		  return total;
	  }

	  static bool issueExitEvent(){
		for(int i=0;i<cpuList.size();i++){
			if(cpuList[i]->useInExitDesicion && !cpuList[i]->canExit){
				return false;
			}
		}
		return true;
	  }

  public:
    // Number of CPU cycles simulated
    Stats::Scalar<> numCycles;
};

#endif // __CPU_BASE_HH__
