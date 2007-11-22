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

#ifndef __AOUT_MACHDEP_H__
#define __AOUT_MACHDEP_H__

///
/// Funky Alpha 64-bit a.out header used for PAL code.
///
struct aout_exechdr {
    uint16_t	magic;		///< magic number
    uint16_t	vstamp;		///< version stamp?
    uint16_t	bldrev;		///< ???
    uint16_t	padcell;	///< padding
    uint64_t	tsize;		///< text segment size
    uint64_t	dsize;		///< data segment size
    uint64_t	bsize;		///< bss segment size
    uint64_t	entry;		///< entry point
    uint64_t	text_start;	///< text base address
    uint64_t	data_start;	///< data base address
    uint64_t	bss_start;	///< bss base address
    uint32_t	gprmask;	///< GPR mask (unused, AFAIK)
    uint32_t	fprmask;	///< FPR mask (unused, AFAIK)
    uint64_t	gp_value;	///< global pointer reg value
};

#define AOUT_LDPGSZ	8192

#define N_GETMAGIC(ex)	((ex).magic)

#define N_BADMAX

#define N_TXTADDR(ex)	((ex).text_start)
#define N_DATADDR(ex)	((ex).data_start)
#define N_BSSADDR(ex)	((ex).bss_start)

#define N_TXTOFF(ex)	\
	(N_GETMAGIC(ex) == ZMAGIC ? 0 : sizeof(struct aout_exechdr))

#define N_DATOFF(ex)	N_ALIGN(ex, N_TXTOFF(ex) + (ex).tsize)

#endif /* !__AOUT_MACHDEP_H__*/
