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

#ifndef __BASE_STATS_FLAGS_HH__
#define __BASE_STATS_FLAGS_HH__
namespace Stats {

/**
 * Define the storage for format flags.
 * @todo Can probably shrink this.
 */
typedef u_int32_t StatFlags;

/** Nothing extra to print. */
const StatFlags none =		0x00000000;
/** This Stat is Initialized */
const StatFlags init =		0x00000001;
/** Print this stat. */
const StatFlags print =		0x00000002;
/** Print the total. */
const StatFlags total =		0x00000010;
/** Print the percent of the total that this entry represents. */
const StatFlags pdf =		0x00000020;
/** Print the cumulative percentage of total upto this entry. */
const StatFlags cdf =		0x00000040;
/** Print the distribution. */
const StatFlags dist = 		0x00000080;
/** Don't print if this is zero. */
const StatFlags nozero =	0x00000100;
/** Don't print if this is NAN */
const StatFlags nonan =		0x00000200;
/** Used for SS compatability. */
const StatFlags __substat = 	0x80000000;

/** Mask of flags that can't be set directly */
const StatFlags __reserved =	init | print | __substat;

enum DisplayMode
{
    mode_m5,
    mode_simplescalar
};

extern DisplayMode DefaultMode;

/* namespace Stats */ }

#endif //  __BASE_STATS_FLAGS_HH__
