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


#ifndef __ARCH_ALPHA_BYTE_SWAP_HH__
#define __ARCH_ALPHA_BYTE_SWAP_HH__

#if defined(linux)
#include <endian.h>
#else
#include <machine/endian.h>
#endif

#include "sim/byte_swap.hh"

/* Note: the host byte-order is defined by BYTE_ORDER, but there is no macro to specify the guest byte-order, instead it is assumed to be little endian */

#if BYTE_ORDER == BIG_ENDIAN

static inline uint64_t
letoh(uint64_t x) { return swp_byte64(x); }

static inline int64_t
letoh(int64_t x) { return swp_byte64((uint64_t)x); }

static inline uint32_t
letoh(uint32_t x) { return swp_byte32(x); }

static inline int32_t
letoh(int32_t x) { return swp_byte32((uint32_t)x); }

static inline uint16_t
letoh(uint16_t x) { return swp_byte16(x); }

static inline int16_t
letoh(int16_t x) { return swp_byte16((uint16_t)x); }

static inline uint8_t
letoh(uint8_t x) { return x; }

static inline int8_t
letoh(int8_t x) { return x; }

static inline double 
letoh(double x) { return swp_byte32((uint64_t)x); }
 
static inline float 
letoh(float x) { return swp_byte64((uint32_t)x); }

static inline uint64_t
letoh(uint64_t x) { return swp_byte64(x); }


static inline int64_t
gtoh(int64_t x) { return swp_byte64((uint64_t)x); }

static inline uint32_t
gtoh(uint32_t x) { return swp_byte32(x); }

static inline int32_t
gtoh(int32_t x) { return swp_byte32((uint32_t)x); }

static inline uint16_t
gtoh(uint16_t x) { return swp_byte16(x); }

static inline int16_t
gtoh(int16_t x) { return swp_byte16((uint16_t)x); }

static inline uint8_t
gtoh(uint8_t x) { return x; }

static inline int8_t
gtoh(int8_t x) { return x; }

static inline double 
gtoh(double x) { return swp_byte32((uint64_t)x); }
 
static inline float 
gtoh(float x) { return swp_byte64((uint32_t)x); }

#elif BYTE_ORDER == LITTLE_ENDIAN

static inline uint64_t
letoh(uint64_t x) { return x; }

static inline int64_t
letoh(int64_t x) { return x; }

static inline uint32_t
letoh(uint32_t x) { return x; }

static inline int32_t
letoh(int32_t x) { return x; }

static inline uint16_t
letoh(uint16_t x) { return x; }

static inline int16_t
letoh(int16_t x) { return x; }

static inline uint8_t
letoh(uint8_t x) { return x; }

static inline int8_t
letoh(int8_t x) { return x; }

static inline double 
letoh(double x) { return x; }
 
static inline float 
letoh(float x) { return x; }


static inline uint64_t
gtoh(uint64_t x) { return x; }

static inline int64_t
gtoh(int64_t x) { return x; }

static inline uint32_t
gtoh(uint32_t x) { return x; }

static inline int32_t
gtoh(int32_t x) { return x; }

static inline uint16_t
gtoh(uint16_t x) { return x; }

static inline int16_t
gtoh(int16_t x) { return x; }

static inline uint8_t
gtoh(uint8_t x) { return x; }

static inline int8_t
gtoh(int8_t x) { return x; }

static inline double 
gtoh(double x) { return x; }
 
static inline float 
gtoh(float x) { return x; }

#else 

#error Invalid Endianness 

#endif /* BYTE_ORDER */

static inline uint64_t
htole(uint64_t x) { return letoh(x); }

static inline int64_t
htole(int64_t x) { return letoh(x); }

static inline uint32_t
htole(uint32_t x) { return letoh(x); }

static inline int32_t
htole(int32_t x) { return letoh(x); }

static inline uint16_t
htole(uint16_t x) { return letoh(x); }

static inline int16_t
htole(int16_t x) { return letoh(x); }

static inline uint8_t
htole(uint8_t x) { return x; }

static inline int8_t
htole(int8_t x) { return x; }

static inline double 
htole(double x) { return letoh(x); }
 
static inline float 
htole(float x) { return letoh(x); }


static inline uint64_t
htog(uint64_t x) { return gtoh(x); }

static inline int64_t
htog(int64_t x) { return gtoh(x); }

static inline uint32_t
htog(uint32_t x) { return gtoh(x); }

static inline int32_t
htog(int32_t x) { return gtoh(x); }

static inline uint16_t
htog(uint16_t x) { return gtoh(x); }

static inline int16_t
htog(int16_t x) { return gtoh(x); }

static inline uint8_t
htog(uint8_t x) { return x; }

static inline int8_t
htog(int8_t x) { return x; }

static inline double 
htog(double x) { return gtoh(x); }
 
static inline float 
htog(float x) { return gtoh(x); }

#endif /* __ARCH_ALPHA_BYTE_SWAP_HH__ */
