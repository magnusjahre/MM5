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

#ifndef __BASE_STATS_BIN_HH__
#define __BASE_STATS_BIN_HH__

#include <cassert>
#include <string>

namespace Stats {

//////////////////////////////////////////////////////////////////////
//
// Binning Interface
//
//////////////////////////////////////////////////////////////////////
struct MainBin
{
    class BinBase;
    friend class MainBin::BinBase;

  private:
    std::string _name;
    char *mem;

  protected:
    off_t memsize;
    off_t size() const { return memsize; }
    char *memory(off_t off);

  public:
    static MainBin *&curBin()
    {
	static MainBin *current = NULL;
	return current;
    }

    static void setCurBin(MainBin *bin) { curBin() = bin; }
    static MainBin *current() { assert(curBin()); return curBin(); }

    static off_t &offset()
    {
	static off_t offset = 0;
	return offset;
    }

    static off_t new_offset(size_t size)
    {
	size_t mask = sizeof(u_int64_t) - 1;
	off_t off = offset();

	// That one is for the last trailing flags byte.
	offset() += (size + 1 + mask) & ~mask;
	return off;
    }

  public:
    MainBin(const std::string &name);
    ~MainBin();

    const std::string &
    name() const
    {
	return _name;
    }

    void
    activate()
    { 
        setCurBin(this); 
    }

    class BinBase
    {
      private:
	int offset;

      public:
	BinBase() : offset(-1) {}
	void allocate(size_t size)
	{
	    offset = new_offset(size);
	}
	char *access()
	{
	    assert(offset != -1);
	    return current()->memory(offset);
	}
    };

    template <class Storage>
    class Bin : public BinBase
    {
      public:
	typedef typename Storage::Params Params;

      public:
	enum { binned = true };
	Bin() { allocate(sizeof(Storage)); }
	bool initialized() const { return true; }
	void init(Params &params) { }

	int size() const { return 1; }

	Storage *
	data(Params &params)
	{
	    assert(initialized());
	    char *ptr = access();
	    char *flags = ptr + sizeof(Storage);
	    if (!(*flags & 0x1)) {
		*flags |= 0x1;
		new (ptr) Storage(params);
	    }
	    return reinterpret_cast<Storage *>(ptr);
        }

	void
	reset()
	{
	    char *ptr = access();
	    char *flags = ptr + size() * sizeof(Storage);
	    if (!(*flags & 0x1))
		return;

	    Storage *s = reinterpret_cast<Storage *>(ptr);
	    s->reset();
	}
    };

    template <class Storage>
    class VectorBin : public BinBase
    {
      public:
	typedef typename Storage::Params Params;

      private:
	int _size;

      public:
        enum { binned = true };
	VectorBin() : _size(0) {}

	bool initialized() const { return _size > 0; }
	void init(int s, Params &params)
	{
	    assert(!initialized());
	    assert(s > 0);
	    _size = s;
	    allocate(_size * sizeof(Storage));
	}

	int size() const { return _size; }

	Storage *data(int index, Params &params)
	{
	    assert(initialized());
	    assert(index >= 0 && index < size());
	    char *ptr = access();
	    char *flags = ptr + size() * sizeof(Storage);
	    if (!(*flags & 0x1)) {
		*flags |= 0x1;
		for (int i = 0; i < size(); ++i)
		    new (ptr + i * sizeof(Storage)) Storage(params);
	    }
	    return reinterpret_cast<Storage *>(ptr + index * sizeof(Storage));
        }
	void reset()
	{
	    char *ptr = access();
	    char *flags = ptr + size() * sizeof(Storage);
	    if (!(*flags & 0x1))
		return;

	    for (int i = 0; i < _size; ++i) {
		char *p = ptr + i * sizeof(Storage);
		Storage *s = reinterpret_cast<Storage *>(p);
		s->reset();
	    }
	}
    };
};

struct NoBin
{
    template <class Storage>
    struct Bin
    {
      public:
	typedef typename Storage::Params Params;  
	enum { binned = false };

      private:
	char ptr[sizeof(Storage)];

      public:
	~Bin()
	{
	    reinterpret_cast<Storage *>(ptr)->~Storage();
	}

	bool initialized() const { return true; }
	void init(Params &params)
	{
	    new (ptr) Storage(params);
	}
	int size() const{ return 1; }
	Storage *data(Params &params)
	{
	    assert(initialized());
	    return reinterpret_cast<Storage *>(ptr);
	}
	void reset()
	{
	    Storage *s = reinterpret_cast<Storage *>(ptr);
	    s->reset();
	}
    };

    template <class Storage>
    struct VectorBin
    {
      public:
	typedef typename Storage::Params Params;  
	enum { binned = false };

      private:
	char *ptr;
	int _size;

      public:
	VectorBin() : ptr(NULL) { }
	~VectorBin()
	{
	    if (!initialized())
		return;

	    for (int i = 0; i < _size; ++i) {
		char *p = ptr + i * sizeof(Storage);
		reinterpret_cast<Storage *>(p)->~Storage();
	    }
	    delete [] ptr;
	}

	bool initialized() const { return ptr != NULL; }
	void init(int s, Params &params)
	{
	    assert(s > 0 && "size must be positive!");
	    assert(!initialized());
	    _size = s;
	    ptr = new char[_size * sizeof(Storage)];
	    for (int i = 0; i < _size; ++i)
		new (ptr + i * sizeof(Storage)) Storage(params);
	}

	int size() const { return _size; }

	Storage *data(int index, Params &params)
	{
	    assert(initialized());
	    assert(index >= 0 && index < size());
	    return reinterpret_cast<Storage *>(ptr + index * sizeof(Storage));
	}
	void reset()
	{
	    for (int i = 0; i < _size; ++i) {
		char *p = ptr + i * sizeof(Storage);
		Storage *s = reinterpret_cast<Storage *>(p);
		s->reset();
	    }
	}
    };
};

/* namespace Stats */ }

#endif // __BASE_STATS_BIN_HH__
