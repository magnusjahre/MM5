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

#include <algorithm> // For min

#include "cpu/trace/trace_cpu.hh"
#include "cpu/trace/reader/mem_trace_reader.hh"
#include "mem/base_mem.hh" // For PARAM constructor
#include "mem/mem_interface.hh"
#include "sim/builder.hh"
#include "sim/sim_events.hh"

using namespace std;

TraceCPU::TraceCPU(const string &name,
		   MemInterface *icache_interface,
		   MemInterface *dcache_interface,
		   MemTraceReader *data_trace)
    : SimObject(name), icacheInterface(icache_interface),
      dcacheInterface(dcache_interface),
      dataTrace(data_trace), outstandingRequests(0), tickEvent(this)
{
    assert(dcacheInterface);
    nextCycle = dataTrace->getNextReq(nextReq);
    tickEvent.schedule(0);
}
    
void
TraceCPU::tick()
{
    assert(outstandingRequests >= 0);
    assert(outstandingRequests < 1000);
    int instReqs = 0;
    int dataReqs = 0;
    
    while (nextReq && curTick >= nextCycle) {
	assert(nextReq->thread_num < 4 && "Not enough threads");
	if (nextReq->isInstRead() && icacheInterface) {
	    if (icacheInterface->isBlocked())
		break;
	    
	    nextReq->time = curTick;
	    if (nextReq->cmd == Squash) {
		icacheInterface->squash(nextReq->asid);
	    } else {
		++instReqs;
		if (icacheInterface->doEvents()) {
		    nextReq->completionEvent = 
			new TraceCompleteEvent(nextReq, this);
		    icacheInterface->access(nextReq);
                    nextReq->expectCompletionEvent = true;
		} else {
		    icacheInterface->access(nextReq);
                    nextReq->expectCompletionEvent = false;
		    completeRequest(nextReq);
		}
	    }
	} else {
	    if (dcacheInterface->isBlocked())
		break;
	    
	    ++dataReqs;
	    nextReq->time = curTick;
	    if (dcacheInterface->doEvents()) {
		nextReq->completionEvent = 
		    new TraceCompleteEvent(nextReq, this);
		dcacheInterface->access(nextReq);
                nextReq->expectCompletionEvent = true;
	    } else {
		dcacheInterface->access(nextReq);
                nextReq->expectCompletionEvent = false;
		completeRequest(nextReq);
	    }
		
	}
	nextCycle = dataTrace->getNextReq(nextReq);
    }
    
    if (!nextReq) {
	// No more requests to send. Finish trailing events and exit.
	if (mainEventQueue.empty()) {
	    new SimExitEvent("Finshed Memory Trace");
	} else {
	    tickEvent.schedule(mainEventQueue.nextEventTime() + cycles(1));
	}
    } else {
	tickEvent.schedule(max(curTick + cycles(1), nextCycle));
    }
}

void
TraceCPU::completeRequest(MemReqPtr& req)
{
}
    
void
TraceCompleteEvent::process()
{
    tester->completeRequest(req);
}

const char *
TraceCompleteEvent::description()
{
    return "trace access complete";
}

TraceCPU::TickEvent::TickEvent(TraceCPU *c)
    : Event(&mainEventQueue, CPU_Tick_Pri), cpu(c)
{
}

void
TraceCPU::TickEvent::process()
{
    cpu->tick();
}

const char *
TraceCPU::TickEvent::description()
{
    return "TraceCPU tick event";
}



BEGIN_DECLARE_SIM_OBJECT_PARAMS(TraceCPU)
  
    SimObjectParam<BaseMem *> icache;
    SimObjectParam<BaseMem *> dcache;
    SimObjectParam<MemTraceReader *> data_trace; 

END_DECLARE_SIM_OBJECT_PARAMS(TraceCPU)

BEGIN_INIT_SIM_OBJECT_PARAMS(TraceCPU)

    INIT_PARAM_DFLT(icache, "instruction cache", NULL),
    INIT_PARAM_DFLT(dcache, "data cache", NULL),
    INIT_PARAM_DFLT(data_trace, "data trace", NULL)
    
END_INIT_SIM_OBJECT_PARAMS(TraceCPU)

CREATE_SIM_OBJECT(TraceCPU)
{
    return new TraceCPU(getInstanceName(),
			(icache) ? icache->getInterface() : NULL, 
			(dcache) ? dcache->getInterface() : NULL,
			data_trace);
}

REGISTER_SIM_OBJECT("TraceCPU", TraceCPU)

