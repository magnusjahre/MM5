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

/* @file
 * Device register definitions for a device's PCI config space
 */

#ifndef __PITREG_H__
#define __PITREG_H__

#include <sys/types.h>

// Control Word Format

#define PIT_SEL_SHFT  0x6
#define PIT_RW_SHFT   0x4
#define PIT_MODE_SHFT 0x1
#define PIT_BCD_SHFT  0x0

#define PIT_SEL_MASK  0x3
#define PIT_RW_MASK   0x3
#define PIT_MODE_MASK 0x7
#define PIT_BCD_MASK  0x1

#define GET_CTRL_FIELD(x, s, m) (((x) >> s) & m)
#define GET_CTRL_SEL(x) GET_CTRL_FIELD(x, PIT_SEL_SHFT, PIT_SEL_MASK)
#define GET_CTRL_RW(x) GET_CTRL_FIELD(x, PIT_RW_SHFT, PIT_RW_MASK)
#define GET_CTRL_MODE(x) GET_CTRL_FIELD(x, PIT_MODE_SHFT, PIT_MODE_MASK)
#define GET_CTRL_BCD(x) GET_CTRL_FIELD(x, PIT_BCD_SHFT, PIT_BCD_MASK)

#define PIT_READ_BACK 0x3

#define PIT_RW_LATCH_COMMAND 0x0
#define PIT_RW_LSB_ONLY      0x1
#define PIT_RW_MSB_ONLY      0x2
#define PIT_RW_16BIT         0x3

#define PIT_MODE_INTTC    0x0
#define PIT_MODE_ONESHOT  0x1
#define PIT_MODE_RATEGEN  0x2
#define PIT_MODE_SQWAVE   0x3
#define PIT_MODE_SWSTROBE 0x4
#define PIT_MODE_HWSTROBE 0x5

#define PIT_BCD_FALSE 0x0
#define PIT_BCD_TRUE  0x1

#endif // __PITREG_H__
