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

#include <string>
#include <vector>

#include "base/misc.hh"
#include "mem/functional/memory_control.hh"
#include "sim/builder.hh"
#include "sim/sim_object.hh"

using namespace std;

MemoryController::MemoryController(const string &name, int cap)
    : FunctionalMemory(name), mapsize(0), mapcap(cap)
{
    map = new devmap[cap];
}

MemoryController::~MemoryController()
{
    if (map)
	delete [] map;
}

void
MemoryController::add_child(FunctionalMemory *obj, const Range<Addr> &range)
{
    if (mapsize == mapcap)
	panic("map is not large enough to add another child\n");

    // check for overlapping ranges!
    for (int i = 0; i < mapsize; ++i) {
	devmap *dm = &map[i];
	if (range <= dm->range && range >= dm->range)
	    panic("range of %s %#x overlaps with range of %s %#x\n",
		  obj->name(), range, dm->obj->name(), dm->range);
    }

    devmap *mp = &map[mapsize++];

    mp->obj = obj;
    mp->range = range;
    mp->range.start &= EV5::PAddrImplMask;
    mp->range.end &= EV5::PAddrImplMask;

    if (mp->range.start & EV5::PAddrUncachedBit43)
        mp->range.start &= EV5::PAddrUncachedMask;

    if (mp->range.end & EV5::PAddrUncachedBit43)
        mp->range.end &= EV5::PAddrUncachedMask;

    DPRINTF(MMU, "add %s: range %#x\n", obj->name(), mp->range);
}

void
MemoryController::update_child(FunctionalMemory *obj, 
                               const Range<Addr> &oldRange,
                               const Range<Addr> &newRange)
{
    DPRINTF(MMU, "update: name=%s, old range %#x new range %#x\n",
	    obj->name(), oldRange, newRange);

    Range<Addr> old = oldRange;
    old.start &= EV5::PAddrImplMask;
    old.end &= EV5::PAddrImplMask;

    // find and change the range
    for (int i = 0; i < mapsize; i++) {
	Range<Addr> &range = map[i].range;

        if (range == old && obj == map[i].obj) {
	    range = newRange;
	    range.start &= EV5::PAddrImplMask;
            range.end &= EV5::PAddrImplMask;

            if (range.start & EV5::PAddrUncachedBit43)
                range.start &= EV5::PAddrUncachedMask;
            
            if (range.end & EV5::PAddrUncachedBit43)
                range.end &= EV5::PAddrUncachedMask;

            // check for overlaps
            for (int j = 0; j < mapsize; j++) {
                if (j != i) {
                    devmap *dm = &map[j];
                    if (range <= dm->range && range >= dm->range)
                        panic("range %s %#x overlaps with %s %#x\n",
                              obj->name(), range, dm->obj->name(), dm->range);
                }
            }

            return;
        }
    }

    panic("Unable to find matching range for child");
}

bool
MemoryController::badaddr(Addr paddr) const
{
    for (int i = 0; i < mapsize; i++)
	if (paddr == map[i].range)
	    return map[i].obj->badaddr(paddr);

    return true;
}

MemoryController::devmap *
MemoryController::find_child(MemReqPtr &req) const
{
    for (int i = 0; i < mapsize; i++) {
	if (req->paddr == map[i].range) {
	    int index = i;

	    // Move up one.
	    if (i > 0) {
		devmap temp = map[i - 1];
		map[i - 1] = map[i];
		map[i] = temp;

		index = i - 1;
	    }

	    return &map[index];
	}
    }

    DPRINTF(MMU, "child not found. vaddr=%#x paddr=%#x\n",
	    req->vaddr, req->paddr);

    return NULL;
}

void
MemoryController::dump() const
{
    for (int i = 0; i < mapsize; i++)
	cprintf("%d: %s: range=%#x\n", i, map[i].obj->name(), map[i].range);
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MemoryController)

    Param<int> capacity;

END_DECLARE_SIM_OBJECT_PARAMS(MemoryController)

BEGIN_INIT_SIM_OBJECT_PARAMS(MemoryController)

INIT_PARAM(capacity, "Maximum Number of children")


END_INIT_SIM_OBJECT_PARAMS(MemoryController)

CREATE_SIM_OBJECT(MemoryController)
{
    return new MemoryController(getInstanceName(), capacity);
}

REGISTER_SIM_OBJECT("MemoryController", MemoryController)

