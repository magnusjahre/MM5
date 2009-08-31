/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005
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

#include <assert.h>

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "cpu/smt.hh"
#include "base/misc.hh"

#include "sim/eventq.hh"
#include "base/trace.hh"
#include "sim/root.hh"

using namespace std;

//
// Main Event Queue
//
// Events on this queue are processed at the *beginning* of each
// cycle, before the pipeline simulation is performed.
//
EventQueue mainEventQueue("MainEventQueue");

void
EventQueue::insert(Event *event)
{
    if (head == NULL || event->when() < head->when() ||
	(event->when() == head->when() &&
	 event->priority() <= head->priority())) {
	event->next = head;
	head = event;
    } else {
	Event *prev = head;
	Event *curr = head->next;

	while (curr) {
	    if (event->when() <= curr->when() &&
		(event->when() < curr->when() ||
		 event->priority() <= curr->priority()))
		break;

	    prev = curr;
	    curr = curr->next;
	}

	event->next = curr;
	prev->next = event;
    }
}

void
EventQueue::remove(Event *event)
{
    if (head == NULL)
	return;

    if (head == event){
	head = event->next;
	return;
    }

    Event *prev = head;
    Event *curr = head->next;
    while (curr && curr != event) {
 	prev = curr;
	curr = curr->next;
    }

    if (curr == event)
	prev->next = curr->next;
}

void
EventQueue::serviceOne()
{
    Event *event = head;
    event->clearFlags(Event::Scheduled);
    head = event->next;

    // handle action
    if (!event->squashed())
	event->process();
    else
	event->clearFlags(Event::Squashed);

    if (event->getFlags(Event::AutoDelete) && !event->scheduled())
	delete event;
}


void
Event::serialize(std::ostream &os)
{
    SERIALIZE_SCALAR(_when);
    SERIALIZE_SCALAR(_priority);
    SERIALIZE_ENUM(_flags);
}


void
Event::unserialize(Checkpoint *cp, const string &section)
{
	if (scheduled()) deschedule();

	UNSERIALIZE_SCALAR(_when);
	UNSERIALIZE_SCALAR(_priority);

	// need to see if original event was in a scheduled, unsquashed
	// state, but don't want to restore those flags in the current
	// object itself (since they aren't immediately true)
	UNSERIALIZE_ENUM(_flags);
//	bool wasScheduled = (_flags & Scheduled) && !(_flags & Squashed);
//	_flags &= ~(Squashed | Scheduled);

//	if (wasScheduled) {
//		DPRINTF(Config, "rescheduling at %d\n", _when);
//		schedule(_when);
//	}

	// HACK: make sure all checkpointed events are serviced at the current tick
	// This is needed to carry out all outstanding work before we enter detailed simulation
	schedule(curTick);
}

void
EventQueue::serialize(ostream &os)
{
    std::list<Event *> eventPtrs;

    int numEvents = 0;
    Event *event = head;
    while (event) {
        if (event->getFlags(Event::AutoSerialize)) {
            eventPtrs.push_back(event);
            paramOut(os, csprintf("event%d", numEvents++), event->name());
        }
        event = event->next;
    }

    SERIALIZE_SCALAR(numEvents);

    for (std::list<Event *>::iterator it=eventPtrs.begin();
         it != eventPtrs.end(); ++it) {
        (*it)->nameOut(os);
        (*it)->serialize(os);
    }
}

void
EventQueue::unserialize(Checkpoint *cp, const std::string &section)
{
    int numEvents;
    UNSERIALIZE_SCALAR(numEvents);

    std::string eventName;
    for (int i = 0; i < numEvents; i++) {
        // get the pointer value associated with the event
        paramIn(cp, section, csprintf("event%d", i), eventName);

        // create the event based on its pointer value
	Serializable::create(cp, eventName);
    }
}

void
EventQueue::dump()
{
    cprintf("============================================================\n");
    cprintf("EventQueue Dump  (cycle %d)\n", curTick);
    cprintf("------------------------------------------------------------\n");

    if (empty())
        cprintf("<No Events>\n");
    else {
	Event *event = head;
	while (event) {
	    event->dump();
	    event = event->next;
	}
    }

    cprintf("============================================================\n");
}

extern "C"
void
dumpMainQueue()
{
    mainEventQueue.dump();
}


const char *
Event::description()
{
    return "generic";
}

#if TRACING_ON
void
Event::trace(const char *action)
{
    // This DPRINTF is unconditional because calls to this function
    // are protected by an 'if (DTRACE(Event))' in the inlined Event
    // methods.
    //
    // This is just a default implementation for derived classes where
    // it's not worth doing anything special.  If you want to put a
    // more informative message in the trace, override this method on
    // the particular subclass where you have the information that
    // needs to be printed.
    DPRINTFN("%s event %s @ %d\n", description(), action, when());
}
#endif

void
Event::dump()
{
    cprintf("Event  (%s)\n", description());
    cprintf("Flags: %#x\n", _flags);
#if TRACING_ON
    cprintf("Created: %d\n", when_created);
#endif
    if (scheduled()) {
#if TRACING_ON
        cprintf("Scheduled at  %d\n", when_scheduled);
#endif
        cprintf("Scheduled for %d, priority %d\n", when(), _priority);
    }
    else {
        cprintf("Not Scheduled\n");
    }
}
