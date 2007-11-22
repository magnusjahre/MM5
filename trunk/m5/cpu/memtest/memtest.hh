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

#ifndef __CPU_MEMTEST_MEMTEST_HH__
#define __CPU_MEMTEST_MEMTEST_HH__

#include <set>

#include "base/statistics.hh"
#include "mem/functional/functional.hh"
#include "mem/mem_interface.hh"
#include "sim/eventq.hh"
#include "sim/sim_exit.hh"
#include "sim/sim_object.hh"
#include "sim/stats.hh"

class ExecContext;
class MemTest : public SimObject
{
  public:

    MemTest(const std::string &name,
	    MemInterface *_cache_interface,
	    FunctionalMemory *main_mem,
	    FunctionalMemory *check_mem,
	    unsigned _memorySize,
	    unsigned _percentReads,
	    unsigned _percentCopies,
	    unsigned _percentUncacheable,
	    unsigned _progressInterval,
	    unsigned _percentSourceUnaligned,
	    unsigned _percentDestUnaligned,
	    Addr _traceAddr,
	    Counter _max_loads);

    // register statistics
    virtual void regStats();

    inline Tick cycles(int numCycles) const { return numCycles; }

    // main simulation loop (one cycle)
    void tick();

  protected:
    class TickEvent : public Event
    {
      private:
	MemTest *cpu;
      public:
	TickEvent(MemTest *c)
	    : Event(&mainEventQueue, CPU_Tick_Pri), cpu(c) {}
	void process() {cpu->tick();}
	virtual const char *description() { return "tick event"; }
    };

    TickEvent tickEvent;

    MemInterface *cacheInterface;
    FunctionalMemory *mainMem;
    FunctionalMemory *checkMem;
    ExecContext *xc;

    unsigned size;		// size of testing memory region

    unsigned percentReads;	// target percentage of read accesses
    unsigned percentCopies;	// target percentage of copy accesses
    unsigned percentUncacheable;

    int id;

    std::set<unsigned> outstandingAddrs;

    unsigned blockSize;

    Addr blockAddrMask;

    Addr blockAddr(Addr addr)
    {
	return (addr & ~blockAddrMask);
    }

    Addr traceBlockAddr;

    Addr baseAddr1;		// fix this to option
    Addr baseAddr2;		// fix this to option
    Addr uncacheAddr;

    unsigned progressInterval;	// frequency of progress reports
    Tick nextProgressMessage;	// access # for next progress report

    unsigned percentSourceUnaligned;
    unsigned percentDestUnaligned;

    Tick noResponseCycles;

    uint64_t numReads;
    uint64_t maxLoads;
    Stats::Scalar<> numReadsStat;
    Stats::Scalar<> numWritesStat;
    Stats::Scalar<> numCopiesStat;

    // called by MemCompleteEvent::process()
    void completeRequest(MemReqPtr &req, uint8_t *data);

    friend class MemCompleteEvent;
};


class MemCompleteEvent : public Event
{
    MemReqPtr req;
    uint8_t *data;
    MemTest *tester;

  public:

    MemCompleteEvent(MemReqPtr &_req, uint8_t *_data, MemTest *_tester)
	: Event(&mainEventQueue),
	  req(_req), data(_data), tester(_tester)
    {
    }

    void process();

    virtual const char *description();
};

#endif // __CPU_MEMTEST_MEMTEST_HH__



