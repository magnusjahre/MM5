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

#include "base/intmath.hh"
#include "base/range.hh"
#include "base/str.hh"

using namespace std;

template <class T>
bool
__x_parse_range(const std::string &str, T &first, T &last)
{
    std::vector<std::string> values;
    tokenize(values, str, ':');

    T thefirst, thelast;

    if (values.size() != 2)
	return false;

    std::string s = values[0];
    std::string e = values[1];

    if (!to_number(s, thefirst))
	return false;

    bool increment = (e[0] == '+');
    if (increment)
	e = e.substr(1);

    if (!to_number(e, thelast))
	return false;

    if (increment)
	thelast += thefirst - 1;

    first = thefirst;
    last = thelast;

    return true;
}

#define RANGE_PARSE(type) \
template<> bool \
__parse_range(const std::string &s, type &first, type &last) \
{ return __x_parse_range(s, first, last); }

RANGE_PARSE(unsigned long long);
RANGE_PARSE(signed long long);
RANGE_PARSE(unsigned long);
RANGE_PARSE(signed long);
RANGE_PARSE(unsigned int);
RANGE_PARSE(signed int);
RANGE_PARSE(unsigned short);
RANGE_PARSE(signed short);
RANGE_PARSE(unsigned char);
RANGE_PARSE(signed char);
