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


#include "sim/syscall_emul.hh"
#include "sim/process.hh"
#include "cpu/exec_context.hh"

SyscallReturn
getpidFunc(SyscallDesc *desc, int callnum, Process *process,
	   ExecContext *xc)
{
    // Make up a PID.  There's no interprocess communication in
    // fake_syscall mode, so there's no way for a process to know it's
    // not getting a unique value.

    // This is one of the funky syscalls that has two return values,
    // with the second one (parent PID) going in r20.
    xc->regs.intRegFile[20] = 99;
    return 100;
}


SyscallReturn
getuidFunc(SyscallDesc *desc, int callnum, Process *process,
	   ExecContext *xc)
{
    // Make up a UID and EUID... it shouldn't matter, and we want the
    // simulation to be deterministic.

    // EUID goes in r20.
    xc->regs.intRegFile[20] = 100;	// EUID
    return 100;		// UID
}


SyscallReturn
getgidFunc(SyscallDesc *desc, int callnum, Process *process,
	   ExecContext *xc)
{
    // Get current group ID.  EGID goes in r20.
    xc->regs.intRegFile[20] = 100;
    return 100;
}


SyscallReturn
setuidFunc(SyscallDesc *desc, int callnum, Process *process,
	   ExecContext *xc)
{
    // can't fathom why a benchmark would call this.
    warn("Ignoring call to setuid(%d)\n", xc->getSyscallArg(0));
    return 0;
}


