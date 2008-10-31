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

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <iostream>
// #include <string>
#include <cstring>

#include "base/time.hh"

using namespace std;

struct _timeval
{
    timeval tv;
};

double
convert(const timeval &tv)
{
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

Time::Time(bool set_now)
{
    time = new _timeval;
    if (set_now)
	set();
}

Time::Time(const timeval &val)
{
    time = new _timeval;
    set(val);
}

Time::Time(const Time &val)
{
    time = new _timeval;
    set(val.get());
}

Time::~Time()
{
    delete time;
}

const timeval &
Time::get() const
{
    return time->tv;
}

void
Time::set()
{
    ::gettimeofday(&time->tv, NULL);
}

void
Time::set(const timeval &tv)
{
    memcpy(&time->tv, &tv, sizeof(timeval));
}

double
Time::operator()() const
{
    return convert(get());
}

string 
Time::date(string format) const
{
    const timeval &tv = get();
    time_t sec = tv.tv_sec;
    char buf[256];

    if (format.empty()) {
	ctime_r(&sec, buf);
	buf[24] = '\0';
	return buf;
    }

    struct tm *tm = localtime(&sec);
    strftime(buf, sizeof(buf), format.c_str(), tm);
    return buf;
}

ostream &
operator<<(ostream &out, const Time &start)
{
    out << start.date();
    return out;
}

Time
operator-(const Time &l, const Time &r)
{
    timeval tv;
    timersub(&l.get(), &r.get(), &tv);
    return tv;
}

const Time Time::start(true);
