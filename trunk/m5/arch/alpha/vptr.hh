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

#ifndef __ARCH_ALPHA_VPTR_HH__
#define __ARCH_ALPHA_VPTR_HH__

#include "arch/alpha/vtophys.hh"

class ExecContext;

template <class T>
class VPtr
{
  public:
    typedef T Type;

  private:
    ExecContext *xc;
    Addr ptr;

  public:
    ExecContext *GetXC() const { return xc; }
    Addr GetPointer() const { return ptr; }

  public:
    explicit VPtr(ExecContext *_xc, Addr p = 0) : xc(_xc), ptr(p) { }
    template <class U>
    VPtr(const VPtr<U> &vp) : xc(vp.GetXC()), ptr(vp.GetPointer()) {}
    ~VPtr() {}

    bool operator!() const
    {
	return ptr == 0;
    }

    VPtr<T> operator+(int offset)
    {
	VPtr<T> ptr(*this);
	ptr += offset;

	return ptr;
    }

    const VPtr<T> &operator+=(int offset)
    {
	ptr += offset;
	assert((ptr & (AlphaISA::PageBytes - 1)) + sizeof(T) 
               < AlphaISA::PageBytes);

	return *this;
    }

    const VPtr<T> &operator=(Addr p)
    {
	assert((p & (AlphaISA::PageBytes)) + sizeof(T) < AlphaISA::PageBytes);
	ptr = p;

	return *this;
    }

    template <class U>
    const VPtr<T> &operator=(const VPtr<U> &vp)
    {
	xc = vp.GetXC();
	ptr = vp.GetPointer();

	return *this;
    }

    operator T *()
    {
	void *addr = vtomem(xc, ptr, sizeof(T));
	return (T *)addr;
    }

    T *operator->()
    {
	void *addr = vtomem(xc, ptr, sizeof(T));
	return (T *)addr;
    }

    T &operator*()
    {
	void *addr = vtomem(xc, ptr, sizeof(T));
	return *(T *)addr;
    }
};

#endif // __ARCH_ALPHA_VPTR_HH__
