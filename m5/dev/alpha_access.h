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

#ifndef __ALPHA_ACCESS_H__
#define __ALPHA_ACCESS_H__

/** @file
 * System Console Memory Mapped Register Definition
 */

#define ALPHA_ACCESS_VERSION (1303)

#ifdef CONSOLE
typedef unsigned uint32_t;
typedef unsigned long uint64_t;
#endif

// This structure hacked up from simos
struct AlphaAccess
{
    uint32_t	last_offset;		// 00: must be first field
    uint32_t	version;		// 04:
    uint32_t	numCPUs;		// 08:
    uint32_t	intrClockFrequency;	// 0C: Hz
    uint64_t	cpuClock;		// 10: MHz
    uint64_t	mem_size;		// 18:

    // Loaded kernel
    uint64_t	kernStart;		// 20:
    uint64_t	kernEnd;		// 28:
    uint64_t	entryPoint;		// 30:

    // console disk stuff
    uint64_t	diskUnit;		// 38:
    uint64_t	diskCount;		// 40:
    uint64_t	diskPAddr;		// 48:
    uint64_t	diskBlock;		// 50:
    uint64_t	diskOperation;		// 58:

    // console simple output stuff
    uint64_t	outputChar;		// 60: Placeholder for output
    uint64_t	inputChar;		// 68: Placeholder for input

    // MP boot
    uint64_t	bootStrapImpure;	// 70:
    uint32_t	bootStrapCPU;		// 78:
    uint32_t	align2;			// 7C: Dummy placeholder for alignment
};

#endif // __ALPHA_ACCESS_H__
