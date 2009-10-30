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

#ifndef __SIM_SIM_EVENTS_HH__
#define __SIM_SIM_EVENTS_HH__

#include "sim/eventq.hh"
#include <cstdio>

//
// Event to terminate simulation at a particular cycle/instruction
//
class SimExitEvent : public Event
{
  private:
    // string explaining why we're terminating
    std::string cause;
    int code;

  public:
    SimExitEvent(const std::string &_cause, int c = 0)
	: Event(&mainEventQueue, Sim_Exit_Pri), cause(_cause),
	  code(c)
	{
		printf("Processing termination event at tick %i\n", (int) curTick);
		fflush(stdout);
		schedule(curTick); 
	}

    SimExitEvent(Tick _when, const std::string &_cause, int c = 0)
	: Event(&mainEventQueue, Sim_Exit_Pri), cause(_cause),
	  code(c)
	{ 
		printf("Processing termination event at tick %i\n", (int) _when);
		fflush(stdout);
		schedule(_when); 
	}

    SimExitEvent(EventQueue *q, const std::string &_cause, int c = 0)
	: Event(q, Sim_Exit_Pri), cause(_cause), code(c)
	{ 
		printf("Processing termination event at tick %i\n", (int) curTick);
		fflush(stdout);
		schedule(curTick); 
	}

    SimExitEvent(EventQueue *q, Tick _when, const std::string &_cause,
		 int c = 0)
	: Event(q, Sim_Exit_Pri), cause(_cause), code(c)
	{ 
		printf("Processing termination event at tick %i\n", (int) _when);
		fflush(stdout);
		schedule(_when); 
	}

    void process();	// process event

    virtual const char *description();
};

//
// Event class to terminate simulation after 'n' related events have
// occurred using a shared counter: used to terminate when *all*
// threads have reached a particular instruction count
//
class CountedExitEvent : public Event
{
  private:
    std::string cause;	// string explaining why we're terminating
    int &downCounter;	// decrement & terminate if zero

  public:
    CountedExitEvent(EventQueue *q, const std::string &_cause,
		     Tick _when, int &_downCounter);

    void process();	// process event

    virtual const char *description();
};

//
// Event to check swap usage
//
class CheckSwapEvent : public Event
{
  private:
    int interval;

  public:
    CheckSwapEvent(EventQueue *q, int ival)
	: Event(q), interval(ival)
    { schedule(curTick + interval); }

    void process();	// process event

    virtual const char *description();
};

//
// Progress event: print out cycle every so often so we know we're
// making forward progress.
//
class ProgressEvent : public Event
{
  protected:
    Tick interval;

  public:
    ProgressEvent(EventQueue *q, Tick ival)
        : Event(q), interval(ival)
    { schedule(curTick + interval); }

    void process();	// process event

    virtual const char *description();
};

#endif  // __SIM_SIM_EVENTS_HH__
