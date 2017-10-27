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

#ifndef __BASE_RANDOM_HH__
#define __BASE_RANDOM_HH__

#include "sim/host.hh"

long getLong();
double getDouble();

template <typename T>
struct Random;

template<> struct Random<int8_t>
{
    static int8_t get()
    { return getLong() & (int8_t)-1; }
};

template<> struct Random<uint8_t>
{
    static uint8_t get()
    { return getLong() & (uint8_t)-1; }
};

template<> struct Random<int16_t>
{
    static int16_t get()
    { return getLong() & (int16_t)-1; }
};

template<> struct Random<uint16_t>
{
    static uint16_t get()
    { return getLong() & (uint16_t)-1; }
};

template<> struct Random<int32_t>
{
    static int32_t get()
    { return (int32_t)getLong(); }
};

template<> struct Random<uint32_t>
{
    static uint32_t get()
    { return (uint32_t)getLong(); }
};

template<> struct Random<int64_t>
{
    static int64_t get()
    { return (int64_t)getLong() << 32 | (uint64_t)getLong(); }
};

template<> struct Random<uint64_t>
{
    static uint64_t get()
    { return (uint64_t)getLong() << 32 | (uint64_t)getLong(); }
};

template<> struct Random<float>
{
    static float get()
    { return getDouble(); }
};

template<> struct Random<double>
{
    static double get()
    { return getDouble(); }
};

#endif // __BASE_RANDOM_HH__
