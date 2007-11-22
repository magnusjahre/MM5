/*
 * Copyright (c) 2004
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

#ifndef __KERN_LINUX_ALIGNED_HH__
#define __KERN_LINUX_ALIGNED_HH__


/* GCC 3.3.X has a bug in which attributes+typedefs don't work. 3.2.X is fine
 * as in 3.4.X, but the bug is marked will not fix in 3.3.X so here is 
 * the work around.
 */
#if __GNUC__ == 3 && __GNUC_MINOR__  != 3
typedef uint64_t uint64_ta __attribute__ ((aligned (8))) ;
typedef int64_t int64_ta __attribute__ ((aligned (8))) ;  
typedef Addr Addr_a __attribute__ ((aligned (8))) ;
#else
#define uint64_ta uint64_t __attribute__ ((aligned (8)))
#define int64_ta int64_t __attribute__ ((aligned (8)))
#define Addr_a Addr __attribute__ ((aligned (8)))
#endif /* __GNUC__ __GNUC_MINOR__ */

#endif /* __KERN_LINUX_ALIGNED_HH__ */
