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

/**
 * @file
 * Definitions of BusInterface class.
 */

#include <string>

#include "mem/bus/bus_interface.hh"
#include "base/statistics.hh"

using namespace std;

template <class BusType>
BusInterface<BusType>::BusInterface(const string &name, HierParams *hier,
                                    BusType *_bus, bool master, bool _isShadow)
    : BaseInterface(name, hier), bus(_bus)
{
    isShadow = _isShadow;
    if(_isShadow){
        id = 42;
    }
    else{
        id = bus->registerInterface(this, master);
        bus->rangeChange();
    }
}

template <class BusType>
MemAccessResult
BusInterface<BusType>::access(MemReqPtr &req)
{
    return BA_NO_RESULT;
}

template <class BusType>
void
BusInterface<BusType>::respond(MemReqPtr &req, Tick time)
{
    fatal("%s: No implementation\n", name());
}

template <class BusType>
bool
BusInterface<BusType>::grantAddr()
{
    fatal("%s: No implementation\n", name());
    return false;
}

template <class BusType>
bool
BusInterface<BusType>::grantData()
{
    fatal("%s: No implementation\n", name());
    return false;
}

template <class BusType>
void
BusInterface<BusType>::deliver(MemReqPtr &req)
{
    fatal("%s: No implementation\n", name());
}

template <class BusType>
void
BusInterface<BusType>::snoop(MemReqPtr &req)
{
    fatal("%s: No implementation\n", name());
}

template <class BusType>
void
BusInterface<BusType>::snoopResponse(MemReqPtr &req)
{
    fatal("%s: No implementation\n", name());
}

template <class BusType>
Tick
BusInterface<BusType>::probe(MemReqPtr &req, bool update)
{
    fatal("%s: No Implementation\n", name());
    return 0;
}

template <class BusType>
void
BusInterface<BusType>::collectRanges(std::list<Range<Addr> > &range_list)
{
    bus->collectRanges(range_list);
}

template <class BusType>
void
BusInterface<BusType>::getRange(std::list<Range<Addr> > &range_list)
{
    
    for (int i = 0; i < ranges.size(); ++i) {
	range_list.push_back(ranges[i]);
    }
}

template <class BusType>
void
BusInterface<BusType>::rangeChange()
{
}
