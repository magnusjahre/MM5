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

#ifndef __BASE_BITFIELD_HH__
#define __BASE_BITFIELD_HH__

#include "sim/host.hh"

/**
 * Generate a 64-bit mask of 'nbits' 1s, right justified.
 */
inline uint64_t
mask(int nbits)
{
    return (nbits == 64) ? (uint64_t)-1LL : (1ULL << nbits) - 1;
}


/**
 * Extract the bitfield from position 'first' to 'last' (inclusive)
 * from 'val' and right justify it.  MSB is numbered 63, LSB is 0.
 */
template <class T>
inline
T
bits(T val, int first, int last)
{
    int nbits = first - last + 1;
    return (val >> last) & mask(nbits);
}

/**
 * Sign-extend an N-bit value to 64 bits.
 */
template <int N>
inline
int64_t
sext(uint64_t val)
{
    int sign_bit = bits(val, N-1, N-1);
    return sign_bit ? (val | ~mask(N)) : val;
}

#endif // __BASE_BITFIELD_HH__
