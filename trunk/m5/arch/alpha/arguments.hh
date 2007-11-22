/*
 * Copyright (c) 2003, 2004, 2005
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

#ifndef __ARGUMENTS_HH__
#define __ARGUMENTS_HH__

#include <assert.h>

#include "arch/alpha/vtophys.hh"
#include "base/refcnt.hh"
#include "sim/host.hh"

class ExecContext;

class AlphaArguments
{
  protected:
    ExecContext *xc;
    int number;
    uint64_t getArg(bool fp = false);

  protected:
    class Data : public RefCounted
    {
      public:
	Data(){}
	~Data();

      private:
	std::list<char *> data;

      public:
	char *alloc(size_t size);
    };

    RefCountingPtr<Data> data;

  public:
    AlphaArguments(ExecContext *ctx, int n = 0)
	: xc(ctx), number(n), data(NULL)
	{ assert(number >= 0); data = new Data;}
    AlphaArguments(const AlphaArguments &args)
	: xc(args.xc), number(args.number), data(args.data) {}
    ~AlphaArguments() {}

    ExecContext *getExecContext() const { return xc; }

    const AlphaArguments &operator=(const AlphaArguments &args) {
	xc = args.xc;
	number = args.number;
	data = args.data;
	return *this;
    }

    AlphaArguments &operator++() {
	++number;
	assert(number >= 0);
	return *this;
    }

    AlphaArguments operator++(int) {
	AlphaArguments args = *this;
	++number;
	assert(number >= 0);
	return args;
    }

    AlphaArguments &operator--() {
	--number;
	assert(number >= 0);
	return *this;
    }

    AlphaArguments operator--(int) {
	AlphaArguments args = *this;
	--number;
	assert(number >= 0);
	return args;
    }

    const AlphaArguments &operator+=(int index) {
	number += index;
	assert(number >= 0);
	return *this;
    }

    const AlphaArguments &operator-=(int index) {
	number -= index;
	assert(number >= 0);
	return *this;
    }

    AlphaArguments operator[](int index) {
	return AlphaArguments(xc, index);
    }

    template <class T>
    operator T() {
	assert(sizeof(T) <= sizeof(uint64_t));
	T data = static_cast<T>(getArg());
	return data;
    }

    template <class T>
    operator T *() {
	T *buf = (T *)data->alloc(sizeof(T));
	CopyData(xc, buf, getArg(), sizeof(T));
	return buf;
    }

    operator char *() {
	char *buf = data->alloc(2048);
	CopyString(xc, buf, getArg(), 2048);
	return buf;
    }
};

#endif // __ARGUMENTS_HH__
