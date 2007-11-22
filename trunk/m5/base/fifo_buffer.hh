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

#ifndef __FIFO_BUFFER_HH__
#define __FIFO_BUFFER_HH__

#include "base/res_list.hh"


//
//  The FifoBuffer requires only that the objects to be used have a default
//  constructor and a dump() method
//
template<class T>
class FifoBuffer
{
  public:
    typedef typename res_list<T>::iterator iterator;

  private:
    res_list<T> *buffer;

    unsigned size;

  public:
    FifoBuffer(unsigned sz)
    {
	buffer = new res_list<T>(sz, true, 0);
	size = sz;
    }

    void add(T &item)
    {
	assert(buffer->num_free() > 0);
	buffer->add_head(item);
    }

    iterator head() { return buffer->head(); }
    iterator tail() { return buffer->tail(); }

    unsigned count() {return buffer->count();}
    unsigned free_slots() {return buffer->num_free();}

    T *peek() { return (count() > 0) ? tail().data_ptr() : 0; }

    T remove()
    {
	assert(buffer->count() > 0);
	T rval = *buffer->tail();
	buffer->remove_tail();
	return rval;
    }

    void dump();

    ~FifoBuffer() { delete buffer; }
};


#endif

