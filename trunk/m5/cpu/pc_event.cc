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

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/trace.hh"
#include "config/full_system.hh"
#include "cpu/base.hh"
#include "cpu/exec_context.hh"
#include "cpu/pc_event.hh"
#include "sim/debug.hh"
#include "sim/root.hh"

using namespace std;

PCEventQueue::PCEventQueue()
{}

PCEventQueue::~PCEventQueue()
{}

bool
PCEventQueue::remove(PCEvent *event)
{
    int removed = 0;
    range_t range = equal_range(event);
    for (iterator i = range.first; i != range.second; ++i) {
	if (*i == event) {
	    DPRINTF(PCEvent, "PC based event removed at %#x: %s\n",
		    event->pc(), event->descr());
	    pc_map.erase(i);
	    ++removed;
	}
    }

    return removed > 0;
}

bool
PCEventQueue::schedule(PCEvent *event)
{
    pc_map.push_back(event);
    sort(pc_map.begin(), pc_map.end(), MapCompare());

    DPRINTF(PCEvent, "PC based event scheduled for %#x: %s\n",
	    event->pc(), event->descr());

    return true;
}

bool
PCEventQueue::doService(ExecContext *xc)
{
    Addr pc = xc->regs.pc & ~0x3;
    int serviced = 0;
    range_t range = equal_range(pc);
    for (iterator i = range.first; i != range.second; ++i) {
	// Make sure that the pc wasn't changed as the side effect of
	// another event.  This for example, prevents two invocations
	// of the SkipFuncEvent.  Maybe we should have separate PC
	// event queues for each processor?
	if (pc != (xc->regs.pc & ~0x3))
	    continue;

	DPRINTF(PCEvent, "PC based event serviced at %#x: %s\n",
		(*i)->pc(), (*i)->descr());

	(*i)->process(xc);
	++serviced;
    }

    return serviced > 0;
}

void
PCEventQueue::dump() const
{
    const_iterator i = pc_map.begin();
    const_iterator e = pc_map.end();

    for (; i != e; ++i)
	cprintf("%d: event at %#x: %s\n", curTick, (*i)->pc(),
		(*i)->descr());
}

PCEventQueue::range_t
PCEventQueue::equal_range(Addr pc)
{
    return std::equal_range(pc_map.begin(), pc_map.end(), pc, MapCompare());
}

BreakPCEvent::BreakPCEvent(PCEventQueue *q, const std::string &desc, Addr addr,
			   bool del)
    : PCEvent(q, desc, addr), remove(del)
{
}

void
BreakPCEvent::process(ExecContext *xc)
{
    StringWrap name(xc->cpu->name() + ".break_event");
    DPRINTFN("break event %s triggered\n", descr());
    debug_break();
    if (remove)
	delete this;
}

#if FULL_SYSTEM
extern "C"
void
sched_break_pc_sys(System *sys, Addr addr)
{
    new BreakPCEvent(&sys->pcEventQueue, "debug break", addr, true);
}

extern "C"
void
sched_break_pc(Addr addr)
{
     for (vector<System *>::iterator sysi = System::systemList.begin();
	  sysi != System::systemList.end(); ++sysi) {
	 sched_break_pc_sys(*sysi, addr);
    }

}
#endif
