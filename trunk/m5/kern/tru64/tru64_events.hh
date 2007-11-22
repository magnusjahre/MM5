/*
 * Copyright (c) 2003, 2004, 2005
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

#ifndef __TRU64_EVENTS_HH__
#define __TRU64_EVENTS_HH__

#include <string>

#include "cpu/pc_event.hh"
#include "kern/system_events.hh"

class ExecContext;

class BadAddrEvent : public SkipFuncEvent
{
  public:
    BadAddrEvent(PCEventQueue *q, const std::string &desc, Addr addr)
	: SkipFuncEvent(q, desc, addr) {}
    virtual void process(ExecContext *xc);
};

class PrintfEvent : public PCEvent
{
  public:
    PrintfEvent(PCEventQueue *q, const std::string &desc, Addr addr)
	: PCEvent(q, desc, addr) {}
    virtual void process(ExecContext *xc);
};

class DebugPrintfEvent : public PCEvent
{
  private:
    bool raw;

  public:
    DebugPrintfEvent(PCEventQueue *q, const std::string &desc, Addr addr,
		     bool r = false)
	: PCEvent(q, desc, addr), raw(r) {}
    virtual void process(ExecContext *xc);
};

class DebugPrintfrEvent : public DebugPrintfEvent
{
  public:
    DebugPrintfrEvent(PCEventQueue *q, const std::string &desc, Addr addr)
	: DebugPrintfEvent(q, desc, addr, true)
    {}
};

class DumpMbufEvent : public PCEvent
{
  public:
    DumpMbufEvent(PCEventQueue *q, const std::string &desc, Addr addr)
	: PCEvent(q, desc, addr) {}
    virtual void process(ExecContext *xc);
};

#endif // __TRU64_EVENTS_HH__
