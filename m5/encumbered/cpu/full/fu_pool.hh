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

#ifndef __ENCUMBERED_CPU_FULL_FU_POOL_HH__
#define __ENCUMBERED_CPU_FULL_FU_POOL_HH__

#include <bitset>
#include <list>
#include <string>
#include <vector>

#include "base/sched_list.hh"
#include "encumbered/cpu/full/op_class.hh"
#include "sim/sim_object.hh"

////////////////////////////////////////////////////////////////////////////
//
//  Structures used ONLY during the initialization phase...
//
//
//

struct OpDesc : public SimObject
{
    OpClass opClass;
    unsigned    opLat;
    unsigned    issueLat;

    OpDesc(std::string name, OpClass c, unsigned o, unsigned i)
	: SimObject(name), opClass(c), opLat(o), issueLat(i) {};
};

struct FUDesc : public SimObject
{
    std::vector<OpDesc *> opDescList;
    unsigned         number;

    FUDesc(std::string name, std::vector<OpDesc *> l, unsigned n)
	: SimObject(name), opDescList(l), number(n) {};
};

typedef std::vector<OpDesc *>::iterator OPDDiterator;
typedef std::vector<FUDesc *>::iterator FUDDiterator;




////////////////////////////////////////////////////////////////////////////
//
//  The actual FU object
//
//
//
class FuncUnit
{
  private:
    unsigned opLatencies[Num_OpClasses];
    unsigned issueLatencies[Num_OpClasses];
    std::bitset<Num_OpClasses> capabilityList;

  public:
    FuncUnit();
    FuncUnit(const FuncUnit &fu);

    std::string name;

    void addCapability(OpClass cap, unsigned oplat, unsigned issuelat);

    bool provides(OpClass capability);
    std::bitset<Num_OpClasses> capabilities();

    unsigned &opLatency(OpClass capability);
    unsigned issueLatency(OpClass capability);
};



////////////////////////////////////////////////////////////////////////////
//
//  A pool of FU objects
//
//
//

class FuncUnitPool : public SimObject
{
  private:
    // TODO: convert to array of events scheduled on demand
    SchedList< std::list<FuncUnit *> > busyList;

    unsigned maxOpLatencies[Num_OpClasses];

    typedef std::list<FuncUnit *>::iterator freeListIterator;

    unsigned list_length;

    std::bitset<Num_OpClasses> capabilityList;

  public:
    int numFU;
    // TODO: possibly convert to array of lists indexed by capability
    std::list<FuncUnit *> freeList;


    // constructor
    FuncUnitPool(std::string name, std::vector<FUDesc *> l);
    ~FuncUnitPool();

    void annotateMemoryUnits(unsigned hit_latency);
    int getUnit(OpClass capability);

    void tick();
    void dump();

    unsigned getLatency(OpClass capability) {
	return maxOpLatencies[capability];
    }

    std::list<FuncUnit *>::iterator fl_iterator;
};

#endif // __FU_POOL_HH__
