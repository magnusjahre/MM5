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
 * Declaration of a memory trace CPU object. Uses a memory trace to drive the
 * provided memory hierarchy.
 */

#ifndef __CPU_TRACE_TRACE_CPU_HH__
#define __CPU_TRACE_TRACE_CPU_HH__

#include <string>

#include "mem/mem_req.hh" // for MemReqPtr
#include "sim/eventq.hh" // for Event
#include "sim/sim_object.hh"

// Forward declaration.
class MemInterface;
class MemTraceReader;

/**
 * A cpu object for running memory traces through a memory hierarchy.
 */
class TraceCPU : public SimObject
{
  private:
    /** Interface for instruction trace requests, if any. */
    MemInterface *icacheInterface;
    /** Interface for data trace requests, if any. */
    MemInterface *dcacheInterface;

    /** Data reference trace. */
    MemTraceReader *dataTrace;

    /** Number of outstanding requests. */
    int outstandingRequests;

    /** Cycle of the next request, 0 if not available. */
    Tick nextCycle;
    
    /** Next request. */
    MemReqPtr nextReq;
    
    /**
     * Event to call the TraceCPU::tick
     */
    class TickEvent : public Event
    {
      private:
	/** The associated CPU */
	TraceCPU *cpu;

      public:
	/**
	 * Construct this event;
	 */
	TickEvent(TraceCPU *c);
	
	/**
	 * Call the tick function.
	 */
	void process();
	
	/**
	 * Return a string description of this event.
	 */
	const char *description();
    };

    TickEvent tickEvent;
    
  public:
    /**
     * Construct a TraceCPU object.
     */
    TraceCPU(const std::string &name,
	     MemInterface *icache_interface,
	     MemInterface *dcache_interface,
	     MemTraceReader *data_trace);

    inline Tick cycles(int numCycles) { return numCycles; }

    /**
     * Perform all the accesses for one cycle.
     */
    void tick();

    /**
     * Handle a completed memory request.
     */
    void completeRequest(MemReqPtr &req);
};

class TraceCompleteEvent : public Event
{
    MemReqPtr req;
    TraceCPU *tester;

  public:

    TraceCompleteEvent(MemReqPtr &_req, TraceCPU *_tester)
	: Event(&mainEventQueue), req(_req), tester(_tester)
    {
	setFlags(AutoDelete);
    }

    void process();

    virtual const char *description();
};

#endif // __CPU_TRACE_TRACE_CPU_HH__

