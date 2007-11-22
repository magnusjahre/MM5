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


#include <cstdio>
#include <iostream>
#include <list>
#include <string>
#include <sstream>

#include "base/cprintf.hh"

using namespace std;

int
main()
{
    char foo[9];
    cprintf("%s\n", foo);

    cprintf("%shits%%s + %smisses%%s\n", "test", "test");
    cprintf("%%s%-10s %c he went home \'\"%d %#o %#x %1.5f %1.2E\n",
	    "hello", 'A', 1, 0xff, 0xfffffffffffffULL, 3.141592653589, 1.1e10);

    cout << cformat("%s %#x %s\n") << "hello" << 0 << "foo 0\n";
    cerr << cformat("%s %#x\n") << "hello" << 1 << "foo 1\n";

    cprintf("another test\n");

    stringstream buffer;
    ccprintf(buffer, "%-10s %c he home \'\"%d %#o %#x %1.5f %1.2E\n",
	     "hello", 'A', 1, 0xff, 0xfffffffffffffULL, 3.14159265, 1.1e10);

    double f = 314159.26535897932384;

    #define ctest(x, y) printf(x, y); cprintf(x, y); cprintf("\n");
    ctest("%1.8f\n", f);
    ctest("%2.8f\n", f);
    ctest("%3.8f\n", f);
    ctest("%4.8f\n", f);
    ctest("%5.8f\n", f);
    ctest("%6.8f\n", f);
    ctest("%12.8f\n", f);
    ctest("%1000.8f\n", f);
    ctest("%1.0f\n", f);
    ctest("%1.1f\n", f);
    ctest("%1.2f\n", f);
    ctest("%1.3f\n", f);
    ctest("%1.4f\n", f);
    ctest("%1.5f\n", f);
    ctest("%1.6f\n", f);
    ctest("%1.7f\n", f);
    ctest("%1.8f\n", f);
    ctest("%1.9f\n", f);
    ctest("%1.10f\n", f);
    ctest("%1.11f\n", f);
    ctest("%1.12f\n", f);
    ctest("%1.13f\n", f);
    ctest("%1.14f\n", f);
    ctest("%1.15f\n", f);
    ctest("%1.16f\n", f);
    ctest("%1.17f\n", f);
    ctest("%1.18f\n", f);

    cout << "foo\n";

    f = 0.00000026535897932384;
    ctest("%1.8f\n", f);
    ctest("%2.8f\n", f);
    ctest("%3.8f\n", f);
    ctest("%4.8f\n", f);
    ctest("%5.8f\n", f);
    ctest("%6.8f\n", f);
    ctest("%12.8f\n", f);
    ctest("%1.0f\n", f);
    ctest("%1.1f\n", f);
    ctest("%1.2f\n", f);
    ctest("%1.3f\n", f);
    ctest("%1.4f\n", f);
    ctest("%1.5f\n", f);
    ctest("%1.6f\n", f);
    ctest("%1.7f\n", f);
    ctest("%1.8f\n", f);
    ctest("%1.9f\n", f);
    ctest("%1.10f\n", f);
    ctest("%1.11f\n", f);
    ctest("%1.12f\n", f);
    ctest("%1.13f\n", f);
    ctest("%1.14f\n", f);
    ctest("%1.15f\n", f);
    ctest("%1.16f\n", f);
    ctest("%1.17f\n", f);
    ctest("%1.18f\n", f);

    f = 0.00000026535897932384;
    ctest("%1.8e\n", f);
    ctest("%2.8e\n", f);
    ctest("%3.8e\n", f);
    ctest("%4.8e\n", f);
    ctest("%5.8e\n", f);
    ctest("%6.8e\n", f);
    ctest("%12.8e\n", f);
    ctest("%1.0e\n", f);
    ctest("%1.1e\n", f);
    ctest("%1.2e\n", f);
    ctest("%1.3e\n", f);
    ctest("%1.4e\n", f);
    ctest("%1.5e\n", f);
    ctest("%1.6e\n", f);
    ctest("%1.7e\n", f);
    ctest("%1.8e\n", f);
    ctest("%1.9e\n", f);
    ctest("%1.10e\n", f);
    ctest("%1.11e\n", f);
    ctest("%1.12e\n", f);
    ctest("%1.13e\n", f);
    ctest("%1.14e\n", f);
    ctest("%1.15e\n", f);
    ctest("%1.16e\n", f);
    ctest("%1.17e\n", f);
    ctest("%1.18e\n", f);

    cout << buffer.str();

    cout.width(0);
    cout.precision(1);
    cout << f << "\n";

    string foo1 = "string test";
    cprintf("%s\n", foo1);

    stringstream foo2;
    foo2 << "stringstream test";
    cprintf("%s\n", foo2);

    cprintf("%c  %c\n", 'c', 65);

    cout << '9';
    return 0;
}
