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

#ifndef __PC_EVENT_HH__
#define __PC_EVENT_HH__

#include <vector>

#include "mem/mem_req.hh"

class ExecContext;
class PCEventQueue;

class PCEvent
{
  protected:
    static const Addr badpc = MemReq::inval_addr;

  protected:
    std::string description;
    PCEventQueue *queue;
    Addr evpc;

  public:
    PCEvent(PCEventQueue *q, const std::string &desc, Addr pc);

    virtual ~PCEvent() { if (queue) remove(); }

    // for DPRINTF
    virtual const std::string name() const { return description; }

    std::string descr() const { return description; }
    Addr pc() const { return evpc; }

    bool remove();
    virtual void process(ExecContext *xc) = 0;
};

class PCEventQueue
{
  protected:
    typedef PCEvent * record_t;
    class MapCompare {
      public:
	bool operator()(const record_t &l, const record_t &r) const {
	    return l->pc() < r->pc();
	}
	bool operator()(const record_t &l, Addr pc) const {
	    return l->pc() < pc;
	}
	bool operator()(Addr pc, const record_t &r) const {
	    return pc < r->pc();
	}
    };
    typedef std::vector<record_t> map_t;

  public:
    typedef map_t::iterator iterator;
    typedef map_t::const_iterator const_iterator;

  protected:
    typedef std::pair<iterator, iterator> range_t;
    typedef std::pair<const_iterator, const_iterator> const_range_t;

  protected:
    map_t pc_map;

    bool doService(ExecContext *xc);

  public:
    PCEventQueue();
    ~PCEventQueue();

    bool remove(PCEvent *event);
    bool schedule(PCEvent *event);
    bool service(ExecContext *xc)
    {
	if (pc_map.empty())
	    return false;

	return doService(xc);
    }

    range_t equal_range(Addr pc);
    range_t equal_range(PCEvent *event) { return equal_range(event->pc()); }

    void dump() const;
};


inline
PCEvent::PCEvent(PCEventQueue *q, const std::string &desc, Addr pc)
    : description(desc), queue(q), evpc(pc)
{ 
    queue->schedule(this);
}

inline bool
PCEvent::remove()
{
    if (!queue)
	panic("cannot remove an uninitialized event;");

    return queue->remove(this);
}

class BreakPCEvent : public PCEvent
{
  protected:
    bool remove;

  public:
    BreakPCEvent(PCEventQueue *q, const std::string &desc, Addr addr,
		 bool del = false);
    virtual void process(ExecContext *xc);
};

#endif // __PC_EVENT_HH__
