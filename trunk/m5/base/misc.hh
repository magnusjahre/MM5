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

#ifndef __MISC_HH__
#define __MISC_HH__

#include <assert.h>
#include "base/cprintf.hh"

//
// This implements a cprintf based panic() function.  panic() should
// be called when something happens that should never ever happen
// regardless of what the user does (i.e., an acutal m5 bug).  panic()
// calls abort which can dump core or enter the debugger.
//
//
void __panic(const std::string&, cp::ArgList &, const char*, const char*, int)
    __attribute__((noreturn));
#define __panic__(format, args...) \
    __panic(format, (*(new cp::ArgList), args), \
        __FUNCTION__, __FILE__, __LINE__)
#define panic(args...) \
    __panic__(args, cp::ArgListNull())

//
// This implements a cprintf based fatal() function.  fatal() should
// be called when the simulation cannot continue due to some condition
// that is the user's fault (bad configuration, invalid arguments,
// etc.) and not a simulator bug.  fatal() calls exit(1), i.e., a
// "normal" exit with an error code, as opposed to abort() like
// panic() does.
//
void __fatal(const std::string&, cp::ArgList &, const char*, const char*, int)
    __attribute__((noreturn));
#define __fatal__(format, args...) \
    __fatal(format, (*(new cp::ArgList), args), \
        __FUNCTION__, __FILE__, __LINE__)
#define fatal(args...) \
    __fatal__(args, cp::ArgListNull())

//
// This implements a cprintf based warn
//
void __warn(const std::string&, cp::ArgList &, const char*, const char*, int);
#define __warn__(format, args...) \
    __warn(format, (*(new cp::ArgList), args), \
	   __FUNCTION__, __FILE__, __LINE__)
#define warn(args...) \
    __warn__(args, cp::ArgListNull())

//
// assert() that prints out the current cycle
//
#define m5_assert(TEST) \
   if (!(TEST)) { \
     std::cerr << "Assertion failure, curTick = " << curTick << std::endl; \
   } \
   assert(TEST);

#endif // __MISC_HH__
