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


#include <iostream.h>

#include "base/str.hh"
#include "base/loader/symtab.hh"

Tick curTick = 0;

void
usage(const char *progname)
{
    cout << "Usage: " << progname << " <symbol file> <symbol>" << endl;

    exit(1);
}

int
main(int argc, char *argv[])
{
    SymbolTable symtab;

    if (argc != 3)
	usage(argv[0]);

    if (!symtab.load(argv[1])) {
	cout << "could not load symbol file: " << argv[1] << endl;
	exit(1);
    }

    string symbol = argv[2];
    Addr address;

    if (!to_number(symbol, address)) {
	if (!symtab.findAddress(symbol, address)) {
	    cout << "could not find symbol: " << symbol << endl;
	    exit(1);
	}

	cout << symbol << " -> " << "0x" << hex << address << endl;
    } else {
	if (!symtab.findSymbol(address, symbol)) {
	    cout << "could not find address: " << address << endl;
	    exit(1);
	}

	cout << "0x" << hex << address << " -> " << symbol<< endl;
    }

    return 0;
}
