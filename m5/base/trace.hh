/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

#ifndef __BASE_TRACE_HH__
#define __BASE_TRACE_HH__

#include <vector>

#include "base/cprintf.hh"
#include "base/match.hh"
#include "sim/host.hh"
#include "sim/root.hh"

#ifndef TRACING_ON
#ifndef NDEBUG
#define TRACING_ON	1
#else
#define TRACING_ON	0
#endif
#endif

#include "base/traceflags.hh"

namespace Trace {

    typedef std::vector<bool> FlagVec;

    extern FlagVec flags;

#if TRACING_ON
    const bool On				= true;
#else
    const bool On				= false;
#endif

    inline bool
    IsOn(int t)
    {
	return flags[t];

    }

    void dump(const uint8_t *data, int count);

    class Record
    {
      protected:
	Tick cycle;

	Record(Tick _cycle)
	    : cycle(_cycle)
	{
	}

      public:
	virtual ~Record() {}

	virtual void dump(std::ostream &) = 0;
    };

    class PrintfRecord : public Record
    {
      private:
	const char *format;
	const std::string &name;
	cp::ArgList &args;

      public:
	PrintfRecord(const char *_format, cp::ArgList &_args,
		     Tick cycle, const std::string &_name)
	    : Record(cycle), format(_format), name(_name), args(_args)
	{
	}

	virtual ~PrintfRecord();

	virtual void dump(std::ostream &);
    };

    class DataRecord : public Record
    {
      private:
	const std::string &name;
	uint8_t *data;
	int len;

      public:
	DataRecord(Tick cycle, const std::string &name,
		   const void *_data, int _len);
	virtual ~DataRecord();

	virtual void dump(std::ostream &);
    };

    class Log
    {
      private:
	int	 size;		// number of records in log
	Record **buffer;	// array of 'size' Record ptrs (circular buf)
	Record **nextRecPtr;	// next slot to use in buffer
	Record **wrapRecPtr;	// &buffer[size], for quick wrap check

      public:

	Log();
	~Log();

	void init(int _size);

	void append(Record *);	// append trace record to log
	void dump(std::ostream &);	// dump contents to stream
    };

    extern Log theLog;

    extern ObjectMatch ignore;

    inline void
    dprintf(const char *format, cp::ArgList &args, Tick cycle,
	    const std::string &name)
    {
	if (name.empty() || !ignore.match(name))
	    theLog.append(new Trace::PrintfRecord(format, args, cycle, name));
    }

    inline void
    dataDump(Tick cycle, const std::string &name, const void *data, int len)
    {
	theLog.append(new Trace::DataRecord(cycle, name, data, len));
    }

    extern const std::string DefaultName;
};

// This silly little class allows us to wrap a string in a functor
// object so that we can give a name() that DPRINTF will like
struct StringWrap
{
    std::string str;
    StringWrap(const std::string &s) : str(s) {}
    const std::string &operator()() const { return str; }
};

inline const std::string &name() { return Trace::DefaultName; }
std::ostream &DebugOut();

//
// DPRINTF is a debugging trace facility that allows one to
// selectively enable tracing statements.  To use DPRINTF, there must
// be a function or functor called name() that returns a const
// std::string & in the current scope.
//
// If you desire that the automatic printing not occur, use DPRINTFR
// (R for raw)
//

#if TRACING_ON

#define DTRACE(x) (Trace::IsOn(Trace::x))

#define DCOUT(x) if (Trace::IsOn(Trace::x)) DebugOut()

#define DDUMP(x, data, count) \
do { \
    if (Trace::IsOn(Trace::x)) \
        Trace::dataDump(curTick, name(), data, count);	\
} while (0)

#define __dprintf(cycle, name, format, args...) \
    Trace::dprintf(format, (*(new cp::ArgList), args), cycle, name)

#define DPRINTF(x, args...) \
do { \
    if (Trace::IsOn(Trace::x)) \
        __dprintf(curTick, name(), args, cp::ArgListNull()); \
} while (0)

#define DPRINTFR(x, args...) \
do { \
    if (Trace::IsOn(Trace::x)) \
        __dprintf((Tick)-1, string(), args, cp::ArgListNull()); \
} while (0)

#define DPRINTFN(args...) \
do { \
    __dprintf(curTick, name(), args, cp::ArgListNull()); \
} while (0)

#define DPRINTFNR(args...) \
do { \
    __dprintf((Tick)-1, string(), args, cp::ArgListNull()); \
} while (0)

#else // !TRACING_ON

#define DTRACE(x) (false)
#define DCOUT(x) if (0) DebugOut()
#define DPRINTF(x, args...) do {} while (0)
#define DPRINTFR(args...) do {} while (0)
#define DPRINTFN(args...) do {} while (0)
#define DPRINTFNR(args...) do {} while (0)
#define DDUMP(x, data, count) do {} while (0)

#endif	// TRACING_ON

#endif // __BASE_TRACE_HH__
