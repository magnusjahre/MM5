/*
 * Copyright (c) 2000, 2001, 2003, 2004, 2005
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

#ifndef __FAST_ALLOC_H__
#define __FAST_ALLOC_H__

#include <stddef.h>

// Fast structure allocator.  Designed for small objects that are
// frequently allocated and deallocated.  This code is derived from the
// 'alloc_struct' package used in WWT and Blizzard.  C++ provides a
// much nicer framework for the same optimization.  The package is
// implemented as a class, FastAlloc.  Allocation and deletion are
// performed using FastAlloc's new and delete operators.  Any object
// that derives from the FastAlloc class will transparently use this
// allocation package.

// The static allocate() and deallocate() methods can also be called
// directly if desired.

// In order for derived classes to call delete with the correct
// structure size even when they are deallocated via a base-type
// pointer, they must have a virtual destructor.  It is sufficient for
// FastAlloc to declare a virtual destructor (as it does); it is not
// required for derived classes to declare their own destructor.  The
// compiler will automatically generate a virtual destructor for each
// derived class.  However, it is more efficient if each derived class
// defines an inline destructor, so that the compiler can statically
// collapse the destructor call chain back up the inheritance
// hierarchy.

// Uncomment this #define to track in-use objects
// (for debugging memory leaks).
//#define FAST_ALLOC_DEBUG

// Uncomment this #define to count news, deletes, and chunk allocations
// (by bucket).
// #define FAST_ALLOC_STATS

#include "config/no_fast_alloc.hh"

#if NO_FAST_ALLOC

class FastAlloc {
};

#else

class FastAlloc {
  public:

    static void *allocate(size_t);
    static void deallocate(void *, size_t);

    void *operator new(size_t);
    void operator delete(void *, size_t);

#ifdef FAST_ALLOC_DEBUG
    FastAlloc();
    FastAlloc(FastAlloc*,FastAlloc*);	// for inUseHead, see below
    virtual ~FastAlloc();
#else
    virtual ~FastAlloc() {}
#endif

  private:

    // Max_Alloc_Size is the largest object that can be allocated with
    // this class.  There's no fundamental limit, but this limits the
    // size of the freeLists array.  Let's not make this really huge
    // like in Blizzard.
    static const int Max_Alloc_Size = 512;

    // Alloc_Quantum is the difference in size between adjacent
    // buckets in the free list array.
    static const int Log2_Alloc_Quantum = 3;
    static const int Alloc_Quantum = (1 << Log2_Alloc_Quantum);

    // Num_Buckets = bucketFor(Max_Alloc_Size) + 1
    static const int Num_Buckets =
	((Max_Alloc_Size + Alloc_Quantum - 1) >> Log2_Alloc_Quantum) + 1;

    // when we call new() for more structures, how many should we get?
    static const int Num_Structs_Per_New = 20;

    static int bucketFor(size_t);
    static void *moreStructs(int bucket);

    static void *freeLists[Num_Buckets];

#ifdef FAST_ALLOC_STATS
    static unsigned newCount[Num_Buckets];
    static unsigned deleteCount[Num_Buckets];
    static unsigned allocCount[Num_Buckets];
#endif

#ifdef FAST_ALLOC_DEBUG
    // per-object debugging fields
    bool inUse;			// in-use flag
    FastAlloc *inUsePrev;	// ptrs to build list of in-use objects
    FastAlloc *inUseNext;

    // static (global) debugging vars
    static int numInUse;	// count in-use objects
    static FastAlloc inUseHead;	// dummy head for list of in-use objects

  public:
    // functions to dump debugging info (see fast_alloc.cc for C
    // versions that might be more agreeable to call from gdb)
    static void dump_summary();
    static void dump_oldest(int n);
#endif
};


inline
int FastAlloc::bucketFor(size_t sz)
{
    return (sz + Alloc_Quantum - 1) >> Log2_Alloc_Quantum;
}


inline
void *FastAlloc::allocate(size_t sz)
{
    int b;
    void *p;

    if (sz > Max_Alloc_Size)
	return (void *)::new char[sz];

    b = bucketFor(sz);
    p = freeLists[b];

    if (p)
	freeLists[b] = *(void **)p;
    else
	p = moreStructs(b);

#ifdef FAST_ALLOC_STATS
    ++newCount[b];
#endif

    return p;
}


inline
void FastAlloc::deallocate(void *p, size_t sz)
{
    int b;

    if (sz > Max_Alloc_Size)
    {
	::delete [] (char *)p;
	return;
    }

    b = bucketFor(sz);
    *(void **)p = freeLists[b];
    freeLists[b] = p;
#ifdef FAST_ALLOC_STATS
    ++deleteCount[b];
#endif
}


inline
void *FastAlloc::operator new(size_t sz)
{
    return allocate(sz);
}


inline
void FastAlloc::operator delete(void *p, size_t sz)
{
    deallocate(p, sz);
}

#endif // NO_FAST_ALLOC

#endif // __FAST_ALLOC_H__
