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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m5op.h"

char *progname;

void
usage()
{
    printf("usage: m5 ivlb <interval>\n"
           "       m5 ivle <interval>\n"
           "       m5 initparam\n"
           "       m5 sw99param\n"
           "       m5 exit [delay]\n"
           "       m5 resetstats [delay [period]]\n"
           "       m5 dumpstats [delay [period]]\n"
           "       m5 dumpresetstats [delay [period]]\n"
           "       m5 checkpoint [delay [period]]\n"
	   "\n"
	   "All times in nanoseconds!\n");
    exit(1);
}

#define COMPARE(X) (strcmp(X, command) == 0)

int
main(int argc, char *argv[])
{
    char *command;
    uint64_t param;
    uint64_t arg1 = 0;
    uint64_t arg2 = 0;
    
    progname = argv[0];
    if (argc < 2)
        usage();

    command = argv[1];

    if (COMPARE("ivlb")) {
        if (argc != 3)
	    usage();

	arg1 = strtoul(argv[2], NULL, 0);
        ivlb(arg1);
	return 0;
    }

    if (COMPARE("ivle")) {
	if (argc != 3)
	    usage();

	arg1 = strtoul(argv[2], NULL, 0);
        ivle(arg1);
	return 0;
    }

    if (COMPARE("initparam")) {
        if (argc != 2)
	    usage();

        printf("%ld", initparam());
	return 0;
    }

    if (COMPARE("sw99param")) {
        if (argc != 2)
	    usage();
        
        param = initparam();
        // run-time, rampup-time, rampdown-time, warmup-time, connections
        printf("%d %d %d %d %d", (param >> 48) & 0xfff, 
               (param >> 36) & 0xfff, (param >> 24) & 0xfff, 
               (param >> 12) & 0xfff, (param >> 0) & 0xfff);

	return 0;
    }

    if (COMPARE("exit")) {
	switch (argc) {
	  case 3:
	    arg1 = strtoul(argv[2], NULL, 0);
	  case 2:
	    m5exit(arg1);
	    return 0;

	  default:
	    usage();
	}
    }

    if (COMPARE("resetstats")) {
	switch (argc) {
	  case 4:
	    arg2 = strtoul(argv[3], NULL, 0);
	  case 3:
	    arg1 = strtoul(argv[2], NULL, 0);
	  case 2:
	    reset_stats(arg1, arg2);
	    return 0;

	  default:
	    usage();
	}
    }
    
    if (COMPARE("dumpstats")) {
	switch (argc) {
	  case 4:
	    arg2 = strtoul(argv[3], NULL, 0);
	  case 3:
	    arg1 = strtoul(argv[2], NULL, 0);
	  case 2:
	    dump_stats(arg1, arg2);
	    return 0;

	  default:
	    usage();
	}
    }

    if (COMPARE("dumpresetstats")) {
	switch (argc) {
	  case 4:
	    arg2 = strtoul(argv[3], NULL, 0);
	  case 3:
	    arg1 = strtoul(argv[2], NULL, 0);
	  case 2:
	    dumpreset_stats(arg1, arg2);
	    return 0;

	  default:
	    usage();
	}
    }

    if (COMPARE("checkpoint")) {
	switch (argc) {
	  case 4:
	    arg2 = strtoul(argv[3], NULL, 0);
	  case 3:
	    arg1 = strtoul(argv[2], NULL, 0);
	  case 2:
	    checkpoint(arg1, arg2);
	    return 0;

	  default:
	    usage();
	}

	return 0;
    }

    usage();
}
