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

/*
 * This code was originally written by Steve Reinhardt as part of
 * the Wisconsin Wind Tunnel simulator.  Relicensed as part of M5
 * by permission.
 */

#ifndef NO_FAST_ALLOC

#ifdef __GNUC__
#pragma implementation
#endif

#include <assert.h>
#include "base/fast_alloc.hh"

void *FastAlloc::freeLists[Num_Buckets];

#ifdef FAST_ALLOC_STATS
unsigned FastAlloc::newCount[Num_Buckets];
unsigned FastAlloc::deleteCount[Num_Buckets];
unsigned FastAlloc::allocCount[Num_Buckets];
#endif

void *FastAlloc::moreStructs(int bucket)
{
    assert(bucket > 0 && bucket < Num_Buckets);

    int sz = bucket * Alloc_Quantum;
    const int nstructs = Num_Structs_Per_New;	// how many to allocate?
    char *p = ::new char[nstructs * sz];

#ifdef FAST_ALLOC_STATS
    ++allocCount[bucket];
#endif

    freeLists[bucket] = p;
    for (int i = 0; i < (nstructs-2); ++i, p += sz)
	*(void **)p = p + sz;
    *(void **)p = 0;

    return (p + sz);
}


#ifdef FAST_ALLOC_DEBUG

#include <typeinfo>
#include <iostream>
#include <iomanip>
#include <map>
#include <string>

using namespace std;

// count of in-use FastAlloc objects
int FastAlloc::numInUse;

// dummy head & tail object for doubly linked list of in-use FastAlloc
// objects
FastAlloc FastAlloc::inUseHead(&FastAlloc::inUseHead, &FastAlloc::inUseHead);

// special constructor for dummy head: make inUsePrev & inUseNext
// point to self
FastAlloc::FastAlloc(FastAlloc *prev, FastAlloc *next)
{
    inUsePrev = prev;
    inUseNext = next;
}


// constructor: marks as in use, add to in-use list
FastAlloc::FastAlloc()
{
    // mark this object in use
    inUse = true;

    // update count
    ++numInUse;

    // add to tail of list of in-use objects ("before" dummy head)
    FastAlloc *myNext = &inUseHead;
    FastAlloc *myPrev = inUseHead.inUsePrev;

    inUsePrev = myPrev;
    inUseNext = myNext;
    myPrev->inUseNext = this;
    myNext->inUsePrev = this;
}

// destructor: mark not in use, remove from in-use list
FastAlloc::~FastAlloc()
{
    assert(inUse);
    inUse = false;

    --numInUse;
    assert(numInUse >= 0);

    // remove me from in-use list
    inUsePrev->inUseNext = inUseNext;
    inUseNext->inUsePrev = inUsePrev;
}


// summarize in-use list
void
FastAlloc::dump_summary()
{
    map<string, int> typemap;

    for (FastAlloc *p = inUseHead.inUseNext; p != &inUseHead; p = p->inUseNext)
    {
	++typemap[typeid(*p).name()];
    }

    map<string, int>::const_iterator mapiter;

    cout << " count  type\n"
	 << " -----  ----\n";
    for (mapiter = typemap.begin(); mapiter != typemap.end(); ++mapiter)
    {
	cout << setw(6) << mapiter->second << "  " << mapiter->first << endl;
    }
}


// show oldest n items on in-use list
void
FastAlloc::dump_oldest(int n)
{
    // sanity check: don't want to crash the debugger if you forget to
    // pass in a parameter
    if (n < 0 || n > numInUse)
    {
	cout << "FastAlloc::dump_oldest: bad arg " << n
	     << " (" << numInUse << " objects in use" << endl;
	return;
    }

    for (FastAlloc *p = inUseHead.inUsePrev;
	 p != &inUseHead && n > 0;
	 p = p->inUsePrev, --n)
    {
	cout << p << " " << typeid(*p).name() << endl;
    }
}


//
// C interfaces to FastAlloc::dump_summary() and FastAlloc::dump_oldest().
// gdb seems to have trouble with calling C++ functions directly.
//
extern "C" void
fast_alloc_summary()
{
    FastAlloc::dump_summary();
}

extern "C" void
fast_alloc_oldest(int n)
{
    FastAlloc::dump_oldest(n);
}

#endif

#endif // NO_FAST_ALLOC
