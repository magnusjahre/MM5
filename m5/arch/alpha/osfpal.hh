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

#ifndef __OSFPAL_HH__
#define __OSFPAL_HH__

struct PAL
{
    enum {
	// Privileged PAL functions
	halt = 0x00,
	cflush = 0x01,
	draina = 0x02,
	cserve = 0x09,
	swppal = 0x0a,
	wripir = 0x0d,
	rdmces = 0x10,
	wrmces = 0x11,
	wrfen = 0x2b,
	wrvptptr = 0x2d,
	swpctx = 0x30,
	wrval = 0x31,
	rdval = 0x32,
	tbi = 0x33,
	wrent = 0x34,
	swpipl = 0x35,
	rdps = 0x36,
	wrkgp = 0x37,
	wrusp = 0x38,
	wrperfmon = 0x39,
	rdusp = 0x3a,
	whami = 0x3c,
	retsys = 0x3d,
	wtint = 0x3e,
	rti = 0x3f,

	// unprivileged pal functions
	bpt = 0x80,
	bugchk = 0x81,
	callsys = 0x83,
	imb = 0x86,
	urti = 0x92,
	rdunique = 0x9e,
	wrunique = 0x9f,
	gentrap = 0xaa,
	clrfen = 0xae,
	nphalt = 0xbe,
	copypal = 0xbf,
	NumCodes
    };

    static const char *name(int index);
};

#endif // __OSFPAL_HH__
