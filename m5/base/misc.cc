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

#include <iostream>
#include <string>
#include <cstdlib>

#include "base/cprintf.hh"
#include "base/hostinfo.hh"
#include "base/misc.hh"
#include "base/output.hh"
#include "base/trace.hh"
#include "sim/host.hh"
#include "sim/root.hh"

using namespace std;

void
__panic(const string &format, cp::ArgList &args, const char *func,
	const char *file, int line)
{
    string fmt = "panic: " + format;
    switch (fmt[fmt.size() - 1]) {
      case '\n':
      case '\r':
	break;
      default:
	fmt += "\n";
    }

    fmt += " @ cycle %d\n[%s:%s, line %d]\n";

    args.append(curTick);
    args.append(func);
    args.append(file);
    args.append(line);
    args.dump(cerr, fmt);

    delete &args;

    abort();
}

void
__fatal(const string &format, cp::ArgList &args, const char *func,
	const char *file, int line)
{
    string fmt = "fatal: " + format;

    switch (fmt[fmt.size() - 1]) {
      case '\n':
      case '\r':
	break;
      default:
	fmt += "\n";
    }

    fmt += " @ cycle %d\n[%s:%s, line %d]\n";
    fmt += "Memory Usage: %ld KBytes\n";

    args.append(curTick);
    args.append(func);
    args.append(file);
    args.append(line);
    args.append(memUsage());
    args.dump(cerr, fmt);

    delete &args;

    exit(1);
}

void
__warn(const string &format, cp::ArgList &args, const char *func,
       const char *file, int line)
{
    string fmt = "warn: " + format;

    switch (fmt[fmt.size() - 1]) {
      case '\n':
      case '\r':
	break;
      default:
	fmt += "\n";
    }

#ifdef VERBOSE_WARN
    fmt += " @ cycle %d\n[%s:%s, line %d]\n";
    args.append(curTick);
    args.append(func);
    args.append(file);
    args.append(line);
#endif

    args.dump(cerr, fmt);
    if (simout.isFile(*outputStream))
	args.dump(*outputStream, fmt);

    delete &args;
}
