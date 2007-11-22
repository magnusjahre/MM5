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

#include <sys/types.h>
#ifdef __osf__
#include <sys/resource.h>
#endif

#include <unistd.h>

#include <iostream>

#include "sim/host.hh"
#include "base/misc.hh"

using namespace std;
/*
 * PC-based execution-time profile... complements gprof since you
 * get feedback on individual instructions instead of entire functions
 */
char *profile_start, *profile_end;
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
typedef char profile_t;
#else
typedef uint8_t profile_t;
#endif
profile_t *profile_buffer;

#ifdef __osf__
void profil(unsigned short *, unsigned int, void  *, unsigned int);
#endif

#ifdef DO_PROFILE
static void
init_profile(char *start, char *end)
{
    int profile_len;

    profile_start = start;
    profile_end = end;
    profile_len = (profile_end - profile_start) / 2;
    profile_buffer = new profile_t[profile_len];
#ifndef __osf__
    profil(profile_buffer, profile_len * sizeof(profile_t),
	   (size_t)profile_start, 65536);
#else
    panic("profiles not supported on tru64 machines");
#endif
}
#endif // DO_PROFILE

void
dump_profile()
{
    int i;
    int profile_len;

    if (profile_buffer == NULL)
	return;

    profile_len = (profile_end - profile_start) / 2;
    for (i = 0; i < profile_len; ++i)
	if (profile_buffer[i] > 0)
	    ccprintf(cerr, "%#08x: %d\n", (intptr_t)profile_start + 2 * i,
		     profile_buffer[i]);
}
