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

#ifndef SCHED_LIST_HH
#define SCHED_LIST_HH

#include <list>
#include "base/misc.hh"

//  Any types you use this class for must be covered here...
namespace {
    void ClearEntry(int &i) { i = 0; };
    void ClearEntry(unsigned &i) { i = 0; };
    void ClearEntry(double &i) { i = 0; };
    template <class T> void ClearEntry(std::list<T> &l) { l.clear(); };
};


//
//  this is a special list type that allows the user to insert elements at a
//  specified positive offset from the "current" element, but only allow them
//  be extracted from the "current" element
//


template <class T>
class SchedList
{
    T *data_array;
    unsigned position;
    unsigned size;
    unsigned mask;

  public:
    SchedList(unsigned size);
    SchedList(void);

    void init(unsigned size);

    T &operator[](unsigned offset);

    void advance(void);

    void clear(void);
};



//
//  Constructor
//
template<class T>
SchedList<T>::SchedList(unsigned _size)
{
    size = _size;

    //  size must be a power of two
    if (size & (size-1)) {
	panic("SchedList: size must be a power of two");
    }

    if (size < 2) {
	panic("SchedList: you don't want a list that small");
    }

    //  calculate the bit mask for the modulo operation
    mask = size - 1;

    data_array = new T[size];

    if (!data_array) {
	panic("SchedList: could not allocate memory");
    }

    clear();
}

template<class T>
SchedList<T>::SchedList(void)
{
    data_array = 0;
    size = 0;
}


template<class T> void
SchedList<T>::init(unsigned _size)
{
    size = _size;

    if (!data_array) {
	//  size must be a power of two
	if (size & (size-1)) {
	    panic("SchedList: size must be a power of two");
	}

	if (size < 2) {
	    panic("SchedList: you don't want a list that small");
	}

	//  calculate the bit mask for the modulo operation
	mask = size - 1;

	data_array = new T[size];

	if (!data_array) {
	    panic("SchedList: could not allocate memory");
	}

	clear();
    }
}


template<class T> void
SchedList<T>::advance(void)
{
    ClearEntry(data_array[position]);

    //    position = (++position % size);
    position = (position+1) & mask;
}


template<class T> void
SchedList<T>::clear(void)
{
    for (unsigned i=0; i<size; ++i) {
	ClearEntry(data_array[i]);
    }

    position = 0;
}


template<class T>  T&
SchedList<T>::operator[](unsigned offset)
{
    if (offset >= size) {
	panic("SchedList: can't access element beyond current pointer");
    }

    //    unsigned p = (position + offset) % size;
    unsigned p = (position + offset) & mask;

    return data_array[p];
}



#endif
