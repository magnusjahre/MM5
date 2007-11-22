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

#ifndef __CPRINTF_HH__
#define __CPRINTF_HH__

#include <iostream>
#include <list>
#include <sstream>
#include <string>

namespace cp {

#include "base/cprintf_formats.hh"

class ArgList
{
  private:
    class Base
    {
      public:
	virtual ~Base() {}
	virtual void process(std::ostream &out, Format &fmt) = 0;
    };

    template <typename T>
    class Node : public Base
    {
      public:
	const T &data;

      public:
	Node(const T &d) : data(d) {}
	virtual void process(std::ostream &out, Format &fmt) {
	    switch (fmt.format) {
	      case Format::character:
		format_char(out, data, fmt);
		break;

	      case Format::integer:
		format_integer(out, data, fmt);
		break;

	      case Format::floating:
		format_float(out, data, fmt);
		break;

	      case Format::string:
		format_string(out, data, fmt);
		break;

	      default:
		out << "<bad format>";
		break;
	    }
	}
    };

    typedef std::list<Base *> list_t;

  protected:
    list_t objects;
    std::ostream *stream;

  public:
    ArgList() : stream(&std::cout) {}
    ~ArgList();

    template<class T>
    void append(const T &data) {
	Base *obj = new ArgList::Node<T>(data);
	objects.push_back(obj);
    }

    template<class T>
    void prepend(const T &data) {
	Base *obj = new ArgList::Node<T>(data);
	objects.push_front(obj);
    }

    void dump(const std::string &format);
    void dump(std::ostream &strm, const std::string &fmt)
	{ stream = &strm; dump(fmt); }

    std::string dumpToString(const std::string &format);

    friend ArgList &operator<<(std::ostream &str, ArgList &list);
};

template<class T>
inline ArgList &
operator,(ArgList &alist, const T &data)
{
    alist.append(data);
    return alist;
}

class ArgListNull {
};

inline ArgList &
operator,(ArgList &alist, ArgListNull)
{ return alist; }

//
// cprintf(format, args, ...) prints to cout
// (analogous to printf())
//
inline void
__cprintf(const std::string &format, ArgList &args)
{ args.dump(format); delete &args; }
#define __cprintf__(format, args...) \
    cp::__cprintf(format, (*(new cp::ArgList), args))
#define cprintf(args...) \
    __cprintf__(args, cp::ArgListNull())

//
// ccprintf(stream, format, args, ...) prints to the specified stream
// (analogous to fprintf())
//
inline void
__ccprintf(std::ostream &stream, const std::string &format, ArgList &args)
{ args.dump(stream, format); delete &args; }
#define __ccprintf__(stream, format, args...) \
    cp::__ccprintf(stream, format, (*(new cp::ArgList), args))
#define ccprintf(stream, args...) \
    __ccprintf__(stream, args, cp::ArgListNull())

//
// csprintf(format, args, ...) returns a string
// (roughly analogous to sprintf())
//
inline std::string
__csprintf(const std::string &format, ArgList &args)
{ std::string s = args.dumpToString(format); delete &args; return s; }
#define __csprintf__(format, args...) \
    cp::__csprintf(format, (*(new cp::ArgList), args))
#define csprintf(args...) \
    __csprintf__(args, cp::ArgListNull())

template<class T>
inline ArgList &
operator<<(ArgList &list, const T &data)
{
    list.append(data);
    return list;
}

inline ArgList &
operator<<(std::ostream &str, ArgList &list)
{
    list.stream = &str;
    return list;
}

class ArgListTemp
{
  private:
    std::string format;
    ArgList *args;

  public:
    ArgListTemp(const std::string &f) : format(f) { args = new ArgList; }
    ~ArgListTemp() { args->dump(format); delete args; }

    operator ArgList *() { return args; }
};

#define cformat(format) \
    (*((cp::ArgList *)cp::ArgListTemp(format)))
}

#endif // __CPRINTF_HH__
