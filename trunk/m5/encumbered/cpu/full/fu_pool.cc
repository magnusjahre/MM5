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

#include <sstream>

#include "encumbered/cpu/full/fu_pool.hh"
#include "sim/builder.hh"

using namespace std;


////////////////////////////////////////////////////////////////////////////
//
//  The funciton unit
//
FuncUnit::FuncUnit()
{
    capabilityList.reset();
}


//  Copy constructor
FuncUnit::FuncUnit(const FuncUnit &fu)
{

    for (int i = 0; i < Num_OpClasses; ++i) {
	opLatencies[i] = fu.opLatencies[i];
	issueLatencies[i] = fu.issueLatencies[i];
    }

    capabilityList = fu.capabilityList;
}


void
FuncUnit::addCapability(OpClass cap, unsigned oplat, unsigned issuelat)
{
    if (issuelat == 0 || oplat == 0)
	panic("FuncUnit:  you don't really want a zero-cycle latency do you?");

    capabilityList.set(cap);

    opLatencies[cap] = oplat;
    issueLatencies[cap] = issuelat;
}

bool
FuncUnit::provides(OpClass capability)
{
    return capabilityList[capability];
}

bitset<Num_OpClasses>
FuncUnit::capabilities()
{
    return capabilityList;
}

unsigned &
FuncUnit::opLatency(OpClass cap)
{
    return opLatencies[cap];
}

unsigned
FuncUnit::issueLatency(OpClass capability)
{
    return issueLatencies[capability];
}

////////////////////////////////////////////////////////////////////////////
//
//  A pool of funciton units
//

FuncUnitPool::~FuncUnitPool()
{
    freeListIterator i = freeList.begin();
    freeListIterator end = freeList.end();
    for (; i != end; ++i)
	delete *i;

    for (int i = 0; i < list_length; ++i) {
	freeListIterator j = busyList[i].begin();
	freeListIterator end = busyList[i].end();
	for (; j != end; ++j)
	    delete *j;
    }
}


// Constructor
FuncUnitPool::FuncUnitPool(string name, vector<FUDesc *> paramList)
    : SimObject(name)
{
    unsigned max_latency = 0;

    numFU = 0;

    freeList.clear();

    for (int i = 0; i < Num_OpClasses; ++i)
	maxOpLatencies[i] = 0;

    //
    //  Iterate through the list of FUDescData structures
    //
    for (FUDDiterator i = paramList.begin(); i != paramList.end(); ++i) {

	//
	//  Don't bother with this if we're not going to create any FU's
	//
	if ((*i)->number) {
	    //
	    //  Create the FuncUnit object from this structure
	    //   - add the capabilities listed in the FU's operation
	    //     description
	    //
	    //  We create the first unit, then duplicate it as needed
	    //
	    FuncUnit *fu = new FuncUnit;

           numFU++;
	    OPDDiterator j = (*i)->opDescList.begin();
	    OPDDiterator end = (*i)->opDescList.end();
	    for (; j != end; ++j) {
		// indicate that this pool has this capability
               // numFU++;
		capabilityList.set((*j)->opClass);

		// indicate that this FU has the capability
		fu->addCapability((*j)->opClass, (*j)->opLat, (*j)->issueLat);

		if ((*j)->opLat > maxOpLatencies[(*j)->opClass])
		    maxOpLatencies[(*j)->opClass] = (*j)->opLat;

		//  find out how deep we need to make the busy list
		if ((*j)->opLat > max_latency)
		    max_latency = (*j)->opLat;
	    }

	    //  Add the appropriate number of copies of this FU to the list
	    ostringstream s;

	    s << (*i)->name() << "(0)";
	    fu->name = s.str();
	    freeList.push_back(fu);

	    for (int c = 1; c < (*i)->number; ++c) {
		ostringstream s;
               numFU++;
		FuncUnit *fu2 = new FuncUnit(*fu);

		s << (*i)->name() << "(" << c << ")";
		fu2->name = s.str();
		freeList.push_back(fu2);
	    }
	}
    }

    //
    //  Figure out what the smallest power of 2 is that will
    //  be larger than the maximum latency FU in this pool
    //
    unsigned length = 0;
    for (int i = 4; i < 8; ++i) {
	if (1 << i >= max_latency + 1) {
	    length = i;
	    break;
	}
    }

    //
    //  Use that power of 2 to create the busy-list
    //
    if (length) {
	// dump() needs this...
	list_length = 1 << length;
	// sets size & clears entries
	busyList.init(1 << length);
    } else
	panic("how LONG are your FU latencies!?!?!?!?");
}

void
FuncUnitPool::annotateMemoryUnits(unsigned hit_latency)
{
    maxOpLatencies[MemReadOp] = hit_latency;

    freeListIterator i = freeList.begin();
    freeListIterator iend = freeList.end();
    for (; i != iend; ++i) {
        if ((*i)->provides(MemReadOp))
            (*i)->opLatency(MemReadOp) = hit_latency;

        if ((*i)->provides(MemWriteOp))
            (*i)->opLatency(MemWriteOp) = hit_latency;
    }
}

int
FuncUnitPool::getUnit(OpClass capability)
{
    //  If this pool doesn't have the specified capability,
    //  return this information to the caller
    if (!capabilityList[capability])
	return -2;

    //  Search for the capability from the FU's in the free list
    freeListIterator i = freeList.begin();
    freeListIterator end = freeList.end();
    for (; i != end; ++i) {
	if ((*i)->provides(capability)) {
	    unsigned issuelat = (*i)->issueLatency(capability);
	    unsigned oplat = (*i)->opLatency(capability);

	    //
	    //  Schedule the FU for when we can issue to it next
	    //
	    //  The units in slot zero become available next cycle (latency 1),
	    //  so we subtract one to fix the off-by-one situation
	    //
	    busyList[issuelat - 1].push_back(*i);

	    freeList.erase(i);  // !!invalidates iterator

	    //  return the number of cycles before the result is available
	    return oplat;
	}
    }

    //  No FU available
    return -1;
}


void
FuncUnitPool::tick()
{
    if (!busyList[0].empty()) {
	list<FuncUnit *>::iterator i = busyList[0].begin();
	list<FuncUnit *>::iterator end = busyList[0].end();
	for (; i != end; ++i)
	    freeList.push_front(*i);
    }

    busyList.advance();  // clears out the list at offset zero
}

void
FuncUnitPool::dump()
{
    //    cout << "Function Unit Pool (" << name() << ")\n";
    cout << "======================================\n";
    cout << "Free List (" << freeList.size() << " elements):\n";

    freeListIterator i = freeList.begin();
    freeListIterator end = freeList.end();
    for (; i != end; ++i)
	cout << "  " << (*i)->name << "\n";

    cout << "======================================\n";
    cout << "Busy List:\n";
    for (int i = 0; i < list_length; ++i) {
	cout << "  [" << i << "] : ";

	freeListIterator j = busyList[i].begin();
	freeListIterator end = busyList[i].end();
	for (; j != end; ++j)
	    cout << (*j)->name << " ";

	cout << "\n";
    }
}

// 

////////////////////////////////////////////////////////////////////////////
//
//  The SimObjects we use to get the FU information into the simulator
//
////////////////////////////////////////////////////////////////////////////

//
//  We use 3 objects to specify this data in the INI file:
//    (1) OpDesc - Describes the operation class & latencies
//                   (multiple OpDesc objects can refer to the same
//                   operation classes)
//    (2) FUDesc - Describes the operations available in the unit &
//                   the number of these units
//    (3) FUPool - Contails a list of FUDesc objects to make available
//
//


//
//  The operation-class description object
//

/* OpClass -> description string */
const char *
opClassStrings[Num_OpClasses] =
{
    "(null)",
    "IntAlu",
    "IntMult",
    "IntDiv",
    "FloatAdd",
    "FloatCmp",
    "FloatCvt",
    "FloatMult",
    "FloatDiv",
    "FloatSqrt",
    "MemRead",
    "MemWrite",
    "IprAccess",
    "InstPrefetch"
};

BEGIN_DECLARE_SIM_OBJECT_PARAMS(OpDesc)

    SimpleEnumParam<OpClass> opClass;
    Param<unsigned>    opLat;
    Param<unsigned>    issueLat;

END_DECLARE_SIM_OBJECT_PARAMS(OpDesc)

BEGIN_INIT_SIM_OBJECT_PARAMS(OpDesc)

    INIT_ENUM_PARAM(opClass, "type of operation", opClassStrings),
    INIT_PARAM(opLat,        "cycles until result is available"),
    INIT_PARAM(issueLat,     "cycles until another can be issued")

END_INIT_SIM_OBJECT_PARAMS(OpDesc)


CREATE_SIM_OBJECT(OpDesc)
{
    return new OpDesc(getInstanceName(), opClass, opLat, issueLat);
}

REGISTER_SIM_OBJECT("OpDesc", OpDesc)


//
//  The FuDesc object
//

BEGIN_DECLARE_SIM_OBJECT_PARAMS(FUDesc)

    SimObjectVectorParam<OpDesc *> opList;
    Param<unsigned>                count;

END_DECLARE_SIM_OBJECT_PARAMS(FUDesc)


BEGIN_INIT_SIM_OBJECT_PARAMS(FUDesc)

    INIT_PARAM(opList, "list of operation classes for this FU type"),
    INIT_PARAM(count,  "number of these FU's available")

END_INIT_SIM_OBJECT_PARAMS(FUDesc)


CREATE_SIM_OBJECT(FUDesc)
{
    return new FUDesc(getInstanceName(), opList, count);
}

REGISTER_SIM_OBJECT("FUDesc", FUDesc)


//
//  The FuPool object
//

BEGIN_DECLARE_SIM_OBJECT_PARAMS(FuncUnitPool)

    SimObjectVectorParam<FUDesc *> FUList;

END_DECLARE_SIM_OBJECT_PARAMS(FuncUnitPool)


BEGIN_INIT_SIM_OBJECT_PARAMS(FuncUnitPool)

    INIT_PARAM(FUList, "list of FU's for this pool")

END_INIT_SIM_OBJECT_PARAMS(FuncUnitPool)


CREATE_SIM_OBJECT(FuncUnitPool)
{
    return new FuncUnitPool(getInstanceName(), FUList);
}

REGISTER_SIM_OBJECT("FuncUnitPool", FuncUnitPool)

