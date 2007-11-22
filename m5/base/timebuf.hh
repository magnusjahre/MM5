/*
 * Copyright (c) 2004, 2005
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

#ifndef __BASE_TIMEBUF_HH__
#define __BASE_TIMEBUF_HH__

#include <vector>

using namespace std;

template <class T>
class TimeBuffer
{
  protected:
    int past;
    int future;
    int size;

    char *data;
    vector<char *> index;
    int base;

    void valid(int idx)
    {
	assert (idx >= -past && idx <= future);
    }

  public:
    friend class wire;
    class wire
    {
        friend class TimeBuffer;
      protected:
	TimeBuffer<T> *buffer;
	int index;

	void set(int idx)
	{
	    buffer->valid(idx);
	    index = idx;
	}

	wire(TimeBuffer<T> *buf, int i)
	    : buffer(buf), index(i)
	{ }

      public:
        wire()
        { }

	wire(const wire &i)
	    : buffer(i.buffer), index(i.index)
	{ }

	const wire &operator=(const wire &i)
	{
	    buffer = i.buffer;
	    set(i.index);
	    return *this;
	}

	const wire &operator=(int idx)
	{
	    set(idx);
	    return *this;
	}

	const wire &operator+=(int offset)
	{
	    set(index + offset);
	    return *this;
	}

	const wire &operator-=(int offset)
	{
	    set(index - offset);
	    return *this;
	}

	wire &operator++()
	{
	    set(index + 1);
	    return *this;
	}

	wire &operator++(int)
	{
	    int i = index;
	    set(index + 1);
	    return wire(this, i);
	}

	wire &operator--()
	{
	    set(index - 1);
	    return *this;
	}

	wire &operator--(int)
	{
	    int i = index;
	    set(index - 1);
	    return wire(this, i);
	}
	T &operator*() const { return *buffer->access(index); }
	T *operator->() const { return buffer->access(index); }
    };


  public:
    TimeBuffer(int p, int f)
	: past(p), future(f), size(past + future + 1), 
          data(new char[size * sizeof(T)]), index(size), base(0)
    {
	assert(past >= 0 && future >= 0);
	char *ptr = data;
	for (int i = 0; i < size; i++) {
	    index[i] = ptr;
	    memset(ptr, 0, sizeof(T));
	    new (ptr) T;
	    ptr += sizeof(T);
	}
    }

    TimeBuffer()
        : data(NULL)
    {
    }

    ~TimeBuffer()
    {
	for (int i = 0; i < size; ++i)
	    (reinterpret_cast<T *>(index[i]))->~T();
	delete [] data;
    }

    void
    advance()
    {
	if (++base >= size)
            base = 0;

        int ptr = base + future;
        if (ptr >= size)
            ptr -= size;
        (reinterpret_cast<T *>(index[ptr]))->~T();
        memset(index[ptr], 0, sizeof(T));
        new (index[ptr]) T;
    }

    T *access(int idx)
    {
        //Need more complex math here to calculate index.
        valid(idx);

        int vector_index = idx + base;
        if (vector_index >= size) {
            vector_index -= size;
        } else if (vector_index < 0) {
            vector_index += size;
        }
        
        return reinterpret_cast<T *>(index[vector_index]);
    }

    T &operator[](int idx)
    {
        //Need more complex math here to calculate index.
        valid(idx);

        int vector_index = idx + base;
        if (vector_index >= size) {
            vector_index -= size;
        } else if (vector_index < 0) {
            vector_index += size;
        }

        return reinterpret_cast<T &>(*index[vector_index]);
    }

    wire getWire(int idx)
    {
	valid(idx);
        
	return wire(this, idx);
    }

    wire zero()
    {
	return wire(this, 0);
    }
};

#endif // __BASE_TIMEBUF_HH__

