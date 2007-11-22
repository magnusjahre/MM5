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

#ifndef __BASE_RANGE_HH__
#define __BASE_RANGE_HH__

#include <cassert>
#include <iostream>
#include <string>

/**
 * @param s range string
 * EndExclusive Ranges are in the following format:
 * @verbatim
 *    <range> := {<start_val>}:{<end>}
 *    <start>   := <end_val> | +<delta>
 * @endverbatim
 */
template <class T>
bool __parse_range(const std::string &s, T &start, T &end);

template <class T>
struct Range
{
    T start;
    T end;

    Range() { invalidate(); }

    template <class U>
    Range(const std::pair<U, U> &r)
	: start(r.first), end(r.second)
    {}

    template <class U>
    Range(const Range<U> &r)
	: start(r.start), end(r.end)
    {}

    Range(const std::string &s)
    {
	if (!__parse_range(s, start, end))
	    invalidate();
    }

    template <class U>
    const Range<T> &operator=(const Range<U> &r)
    {
	start = r.start;
	end = r.end;
	return *this;
    }

    template <class U>
    const Range<T> &operator=(const std::pair<U, U> &r)
    {
	start = r.first;
	end = r.second;
	return *this;
    }

    const Range &operator=(const std::string &s)
    {
	if (!__parse_range(s, start, end))
	    invalidate();
	return *this;
    }

    void invalidate() { start = 1; end = 0; }
    T size() const { return end - start + 1; }
    bool valid() const { return start < end; }
};

template <class T>
inline std::ostream &
operator<<(std::ostream &o, const Range<T> &r)
{
    o << '[' << r.start << "," << r.end << ']';
    return o;
}

template <class T>
inline Range<T>
RangeEx(T start, T end)
{ return std::make_pair(start, end - 1); }

template <class T>
inline Range<T>
RangeIn(T start, T end)
{ return std::make_pair(start, end); }

template <class T, class U>
inline Range<T>
RangeSize(T start, U size)
{ return std::make_pair(start, start + size - 1); }

////////////////////////////////////////////////////////////////////////
//
// Range to Range Comparisons
//

/**
 * @param range1 is a range.
 * @param range2 is a range.
 * @return if range1 and range2 are identical.
 */
template <class T, class U>
inline bool
operator==(const Range<T> &range1, const Range<U> &range2)
{
    return range1.start == range2.start && range1.end == range2.end;
}

/**
 * @param range1 is a range.
 * @param range2 is a range.
 * @return if range1 and range2 are not identical.
 */
template <class T, class U>
inline bool
operator!=(const Range<T> &range1, const Range<U> &range2)
{
    return range1.start != range2.start || range1.end != range2.end;
}

/**
 * @param range1 is a range.
 * @param range2 is a range.
 * @return if range1 is less than range2 and does not overlap range1.
 */
template <class T, class U>
inline bool
operator<(const Range<T> &range1, const Range<U> &range2)
{
    return range1.start < range2.start;
}

/**
 * @param range1 is a range.
 * @param range2 is a range.
 * @return if range1 is less than range2.  range1 may overlap range2,
 * but not extend beyond the end of range2.
 */
template <class T, class U>
inline bool
operator<=(const Range<T> &range1, const Range<U> &range2)
{
    return range1.start <= range2.start;
}

/**
 * @param range1 is a range.
 * @param range2 is a range.
 * @return if range1 is greater than range2 and does not overlap range2.
 */
template <class T, class U>
inline bool
operator>(const Range<T> &range1, const Range<U> &range2)
{
    return range1.start > range2.start;
}

/**
 * @param range1 is a range.
 * @param range2 is a range.
 * @return if range1 is greater than range2.  range1 may overlap range2,
 * but not extend beyond the beginning of range2.
 */
template <class T, class U>
inline bool
operator>=(const Range<T> &range1, const Range<U> &range2)
{
    return range1.start >= range2.start;
}

////////////////////////////////////////////////////////////////////////
//
// Position to Range Comparisons
//

/**
 * @param pos position compared to the range.
 * @param range range compared against.
 * @return indicates that position pos is within the range.
 */
template <class T, class U>
inline bool
operator==(const T &pos, const Range<U> &range)
{
    return pos >= range.start && pos <= range.end;
}

/**
 * @param pos position compared to the range.
 * @param range range compared against.
 * @return indicates that position pos is not within the range.
 */
template <class T, class U>
inline bool
operator!=(const T &pos, const Range<U> &range)
{
    return pos < range.start || pos > range.end;
}

/**
 * @param pos position compared to the range.
 * @param range range compared against.
 * @return indicates that position pos is below the range.
 */
template <class T, class U>
inline bool
operator<(const T &pos, const Range<U> &range)
{
    return pos < range.start;
}

/**
 * @param pos position compared to the range.
 * @param range range compared against.
 * @return indicates that position pos is below or in the range.
 */
template <class T, class U>
inline bool
operator<=(const T &pos, const Range<U> &range)
{
    return pos <= range.end;
}

/**
 * @param pos position compared to the range.
 * @param range range compared against.
 * @return indicates that position pos is above the range.
 */
template <class T, class U>
inline bool
operator>(const T &pos, const Range<U> &range)
{
    return pos > range.end;
}

/**
 * @param pos position compared to the range.
 * @param range range compared against.
 * @return indicates that position pos is above or in the range.
 */
template <class T, class U>
inline bool
operator>=(const T &pos, const Range<U> &range)
{
    return pos >= range.start;
}

////////////////////////////////////////////////////////////////////////
//
// Range to Position Comparisons (for symmetry)
//

/**
 * @param range range compared against.
 * @param pos position compared to the range.
 * @return indicates that position pos is within the range.
 */
template <class T, class U>
inline bool
operator==(const Range<T> &range, const U &pos)
{
    return pos >= range.start && pos <= range.end;
}

/**
 * @param range range compared against.
 * @param pos position compared to the range.
 * @return indicates that position pos is not within the range.
 */
template <class T, class U>
inline bool
operator!=(const Range<T> &range, const U &pos)
{
    return pos < range.start || pos > range.end;
}

/**
 * @param range range compared against.
 * @param pos position compared to the range.
 * @return indicates that position pos is above the range.
 */
template <class T, class U>
inline bool
operator<(const Range<T> &range, const U &pos)
{
    return range.end < pos;
}

/**
 * @param range range compared against.
 * @param pos position compared to the range.
 * @return indicates that position pos is above or in the range.
 */
template <class T, class U>
inline bool
operator<=(const Range<T> &range, const U &pos)
{
    return range.start <= pos;
}

/**
 * @param range range compared against.
 * @param pos position compared to the range.
 * 'range > pos' indicates that position pos is below the range.
 */
template <class T, class U>
inline bool
operator>(const Range<T> &range, const U &pos)
{
    return range.start > pos;
}

/**
 * @param range range compared against.
 * @param pos position compared to the range.
 * 'range >= pos' indicates that position pos is below or in the range.
 */
template <class T, class U>
inline bool
operator>=(const Range<T> &range, const U &pos)
{
    return range.end >= pos;
}

#endif // __BASE_RANGE_HH__
