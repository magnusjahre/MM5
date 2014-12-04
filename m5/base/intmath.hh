/*
 * Copyright (c) 2001, 2003, 2004, 2005
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

#ifndef __INTMATH_HH__
#define __INTMATH_HH__

#include <assert.h>

#include "sim/host.hh"

// Returns the prime number one less than n.
int PrevPrime(int n);

// Determine if a number is prime
template <class T>
inline bool
IsPrime(T n)
{
    T i;

    if (n == 2 || n == 3)
	return true;

    // Don't try every odd number to prove if it is a prime.
    // Toggle between every 2nd and 4th number.
    // (This is because every 6th odd number is divisible by 3.)
    for (i = 5; i*i <= n; i += 6) {
	if (((n % i) == 0 ) || ((n % (i + 2)) == 0) ) {
	    return false;
	}
    }

    return true;
}

template <class T>
inline T
LeastSigBit(T n)
{
    return n & ~(n - 1);
}

template <class T>
inline bool
IsPowerOf2(T n)
{
    return n != 0 && LeastSigBit(n) == n;
}

inline int
FloorLog2(unsigned x)
{
    assert(x > 0);

    int y = 0;

    if (x & 0xffff0000) { y += 16; x >>= 16; }
    if (x & 0x0000ff00) { y +=  8; x >>=  8; }
    if (x & 0x000000f0) { y +=  4; x >>=  4; }
    if (x & 0x0000000c) { y +=  2; x >>=  2; }
    if (x & 0x00000002) { y +=  1; }

    return y;
}

inline int
FloorLog2(unsigned long x)
{
    assert(x > 0);

    int y = 0;

#if defined(__LP64__)
    if (x & ULL(0xffffffff00000000)) { y += 32; x >>= 32; }
#endif
    if (x & 0xffff0000) { y += 16; x >>= 16; }
    if (x & 0x0000ff00) { y +=  8; x >>=  8; }
    if (x & 0x000000f0) { y +=  4; x >>=  4; }
    if (x & 0x0000000c) { y +=  2; x >>=  2; }
    if (x & 0x00000002) { y +=  1; }

    return y;
}

inline int
FloorLog2(unsigned long long x)
{
    assert(x > 0);

    int y = 0;

    if (x & ULL(0xffffffff00000000)) { y += 32; x >>= 32; }
    if (x & ULL(0x00000000ffff0000)) { y += 16; x >>= 16; }
    if (x & ULL(0x000000000000ff00)) { y +=  8; x >>=  8; }
    if (x & ULL(0x00000000000000f0)) { y +=  4; x >>=  4; }
    if (x & ULL(0x000000000000000c)) { y +=  2; x >>=  2; }
    if (x & ULL(0x0000000000000002)) { y +=  1; }

    return y;
}

inline int
FloorLog2(int x)
{
    assert(x > 0);
    return FloorLog2((unsigned)x);
}

inline int
FloorLog2(long x)
{
    assert(x > 0);
    return FloorLog2((unsigned long)x);
}

inline int
FloorLog2(long long x)
{
    assert(x > 0);
    return FloorLog2((unsigned long long)x);
}

template <class T>
inline int
CeilLog2(T n)
{
    if (n == 1)
	return 0;

    return FloorLog2(n - (T)1) + 1;
}

template <class T>
inline T
FloorPow2(T n)
{
    return (T)1 << FloorLog2(n);
}

template <class T>
inline T
CeilPow2(T n)
{
    return (T)1 << CeilLog2(n);
}

template <class T>
inline T
DivCeil(T a, T b)
{
    return (a + b - 1) / b;
}

template <class T>
inline T
RoundUp(T val, T align)
{
    T mask = align - 1;
    return (val + mask) & ~mask;
}

template <class T>
inline T
RoundDown(T val, T align)
{
    T mask = align - 1;
    return val & ~mask;
}

inline bool
IsHex(char c)
{
    return (c >= '0' && c <= '9') ||
	(c >= 'A' && c <= 'F') ||
	(c >= 'a' && c <= 'f');
}

inline bool
IsOct(char c)
{
    return c >= '0' && c <= '7';
}

inline bool
IsDec(char c)
{
    return c >= '0' && c <= '9';
}

inline int
Hex2Int(char c)
{
  if (c >= '0' && c <= '9')
    return (c - '0');

  if (c >= 'A' && c <= 'F')
    return (c - 'A') + 10;

  if (c >= 'a' && c <= 'f')
    return (c - 'a') + 10;

  return 0;
}

#endif // __INTMATH_HH__
