/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

#include "base/callback.hh"
#include "base/inifile.hh"
#include "base/match.hh"
#include "base/misc.hh"
#include "base/trace.hh"
#include "base/stats/events.hh"
#include "sim/configfile.hh"
#include "sim/host.hh"
#include "sim/sim_object.hh"
#include "sim/stats.hh"
#include "sim/param.hh"

using namespace std;


////////////////////////////////////////////////////////////////////////
//
// SimObject member definitions
//
////////////////////////////////////////////////////////////////////////

//
// static list of all SimObjects, used for initialization etc.
//
SimObject::SimObjectList SimObject::simObjectList;

namespace Stats {
    extern ObjectMatch event_ignore;
}

//
// SimObject constructor: used to maintain static simObjectList
//
SimObject::SimObject(Params *p)
    : _params(p)
{
#ifdef DEBUG
    doDebugBreak = false;
#endif
    
    doRecordEvent = !Stats::event_ignore.match(name());
    simObjectList.push_back(this);
}

//
// SimObject constructor: used to maintain static simObjectList
//
SimObject::SimObject(const string &_name)
    : _params(new Params)
{
    _params->name = _name;
#ifdef DEBUG
    doDebugBreak = false;
#endif

    doRecordEvent = !Stats::event_ignore.match(name());
    simObjectList.push_back(this);
}

void
SimObject::init()
{
}

//
// no default statistics, so nothing to do in base implementation
//
void
SimObject::regStats()
{
}

void
SimObject::regFormulas()
{
}

void
SimObject::resetStats()
{
}

//
// static function:
//   call regStats() on all SimObjects and then regFormulas() on all
//   SimObjects.
//
struct SimObjectResetCB : public Callback
{
    virtual void process() { SimObject::resetAllStats(); }
};

namespace {
    static SimObjectResetCB StatResetCB;
}

void
SimObject::regAllStats()
{
    SimObjectList::iterator i;
    SimObjectList::iterator end = simObjectList.end();

    /**
     * @todo change cprintfs to DPRINTFs
     */
    for (i = simObjectList.begin(); i != end; ++i) {
#ifdef STAT_DEBUG
	cprintf("registering stats for %s\n", (*i)->name());
#endif
	(*i)->regStats();
    }

    for (i = simObjectList.begin(); i != end; ++i) {
#ifdef STAT_DEBUG
	cprintf("registering formulas for %s\n", (*i)->name());
#endif
	(*i)->regFormulas();
    }

    Stats::registerResetCallback(&StatResetCB);
}

//
// static function: call init() on all SimObjects.
//
void
SimObject::initAll()
{
    SimObjectList::iterator i = simObjectList.begin();
    SimObjectList::iterator end = simObjectList.end();

    for (; i != end; ++i) {
	SimObject *obj = *i;
	obj->init();
    }
}

//
// static function: call resetStats() on all SimObjects.
//
void
SimObject::resetAllStats()
{
    SimObjectList::iterator i = simObjectList.begin();
    SimObjectList::iterator end = simObjectList.end();

    for (; i != end; ++i) {
        SimObject *obj = *i;
        obj->resetStats();
    }
}

//
// static function: serialize all SimObjects.
//
void
SimObject::serializeAll(ostream &os)
{
    SimObjectList::reverse_iterator ri = simObjectList.rbegin();
    SimObjectList::reverse_iterator rend = simObjectList.rend();

    for (; ri != rend; ++ri) {
	SimObject *obj = *ri;
	obj->nameOut(os);
	obj->serialize(os);
   }
}

#ifdef DEBUG
//
// static function: flag which objects should have the debugger break
//
void
SimObject::debugObjectBreak(const string &objs)
{
    SimObjectList::const_iterator i = simObjectList.begin();
    SimObjectList::const_iterator end = simObjectList.end();

    ObjectMatch match(objs);
    for (; i != end; ++i) {
	SimObject *obj = *i;
	obj->doDebugBreak = match.match(obj->name());
   }
}

extern "C"
void
debugObjectBreak(const char *objs)
{
    SimObject::debugObjectBreak(string(objs));
}
#endif

void
SimObject::recordEvent(const std::string &stat)
{
    if (doRecordEvent)
	Stats::recordEvent(stat);
}

DEFINE_SIM_OBJECT_CLASS_NAME("SimObject", SimObject)
